#include "doctest.h"
#include "cb_scan.hpp"
#include <cstring>
#include <vector>
using namespace spike;

namespace {
const core::Vec3 kAvatar{123.5f, 30.25f, -456.75f};
const core::Vec3 kCamera{130.0f, 34.0f, -450.0f};

// A plausible constant buffer: junk floats with one float3 planted at `at`.
std::vector<float> make_cb(size_t floats, size_t at, float a, float b, float c) {
    std::vector<float> cb(floats);
    for (size_t i = 0; i < floats; ++i) cb[i] = 0.001f * (float)i - 3.7f;
    cb[at] = a; cb[at + 1] = b; cb[at + 2] = c;
    return cb;
}

int find(const std::vector<float>& cb, ScanResult* out) {
    Candidate c[kMaxCandidates];
    int n = build_candidates(kAvatar, kCamera, c);
    for (int i = 0; i < n; ++i) out[i] = ScanResult{};
    scan_buffer(cb.data(), cb.size() * sizeof(float), c, n, out);
    return n;
}
}  // namespace

TEST_CASE("candidates cover metres, inches and camera-relative") {
    Candidate c[kMaxCandidates];
    int n = build_candidates(kAvatar, kCamera, c);
    REQUIRE(n == 4);
    CHECK(c[1].v.x == doctest::Approx(kAvatar.x * kInchesPerMeter));
    CHECK(c[2].v.z == doctest::Approx(kAvatar.z - kCamera.z));
    CHECK(c[3].v.y == doctest::Approx((kAvatar.y - kCamera.y) * kInchesPerMeter));
    CHECK(c[1].tol > c[0].tol);   // inches need a proportionally looser window
}

TEST_CASE("finds a world position stored in metres, same axis order") {
    auto cb = make_cb(64, 12, kAvatar.x, kAvatar.y, kAvatar.z);
    ScanResult r[kMaxCandidates];
    int n = find(cb, r);
    CHECK(any_hit(r, n));
    CHECK(r[0].exact.hit);
    CHECK(r[0].exact.offset == 12 * sizeof(float));
    CHECK(r[0].exact.dist == doctest::Approx(0.0f).epsilon(0.001));
}

TEST_CASE("finds a matrix translation in inches at m[12..14]") {
    auto cb = make_cb(64, 16 + 12, kAvatar.x * kInchesPerMeter,
                      kAvatar.y * kInchesPerMeter, kAvatar.z * kInchesPerMeter);
    ScanResult r[kMaxCandidates];
    int n = find(cb, r);
    CHECK(any_hit(r, n));
    CHECK(r[1].exact.hit);
    CHECK(r[1].exact.offset == 28 * sizeof(float));
}

TEST_CASE("finds a camera-relative position under a swapped, flipped axis order") {
    // Z-up engine convention: (x, y, z)_mumble stored as (x, -z, y).
    core::Vec3 rel = kAvatar - kCamera;
    auto cb = make_cb(64, 40, rel.x * kInchesPerMeter,
                      -rel.z * kInchesPerMeter, rel.y * kInchesPerMeter);
    ScanResult r[kMaxCandidates];
    int n = find(cb, r);
    CHECK(any_hit(r, n));
    CHECK(r[3].permuted.hit);
    CHECK(r[3].permuted.offset == 40 * sizeof(float));
    CHECK_FALSE(r[3].exact.hit);   // only the order-insensitive test should fire
}

TEST_CASE("unrelated data produces no hit but still reports how close it got") {
    auto cb = make_cb(64, 12, 8.0f, -2.5f, 61.0f);
    ScanResult r[kMaxCandidates];
    int n = find(cb, r);
    CHECK_FALSE(any_hit(r, n));
    for (int i = 0; i < n; ++i) {
        CHECK(r[i].exact.found);          // a near miss is still an answer
        CHECK(r[i].exact.dist > 0.0f);
    }
}

TEST_CASE("buffers too small to hold a float3 are skipped safely") {
    float tiny[2] = {kAvatar.x, kAvatar.y};
    Candidate c[kMaxCandidates];
    int n = build_candidates(kAvatar, kCamera, c);
    ScanResult r[kMaxCandidates];
    scan_buffer(tiny, sizeof(tiny), c, n, r);
    CHECK_FALSE(any_hit(r, n));
    CHECK_FALSE(r[0].exact.found);
}

TEST_CASE("NaN and infinity in the buffer are never treated as a match") {
    auto cb = make_cb(32, 8, 0.0f, 0.0f, 0.0f);
    cb[8]  = std::nanf("");
    cb[9]  = INFINITY;
    cb[10] = kAvatar.x;
    ScanResult r[kMaxCandidates];
    int n = find(cb, r);
    CHECK_FALSE(any_hit(r, n));
}

TEST_CASE("finds a column-major world matrix translation (elements 3/7/11)") {
    // Column-major float4x4: translation lands 16 bytes apart, not contiguous.
    std::vector<float> cb(64);
    for (size_t i = 0; i < cb.size(); ++i) cb[i] = 0.001f * (float)i - 3.7f;
    const size_t m = 16;                        // matrix starts at float 16
    cb[m + 3]  = kAvatar.x;
    cb[m + 7]  = kAvatar.y;
    cb[m + 11] = kAvatar.z;
    ScanResult r[kMaxCandidates];
    int n = find(cb, r);
    CHECK(any_hit(r, n));
    CHECK(r[0].col_major.hit);
    CHECK(r[0].col_major.offset == (m + 3) * sizeof(float));
    CHECK_FALSE(r[0].exact.hit);                // contiguous scan alone would miss it
}

TEST_CASE("clip-space scan finds a float4 that projects onto the character") {
    ClipTarget t;
    t.screen_w = 1920.0f; t.screen_h = 1080.0f;
    t.screen_x = 960.0f;  t.screen_y = 400.0f;
    t.tol_px = 50.0f;

    // Build a clip-space point that lands exactly on the target.
    const float w = 23.5f;
    const float ndc_x = (t.screen_x / t.screen_w) * 2.0f - 1.0f;
    const float ndc_y = 1.0f - (t.screen_y / t.screen_h) * 2.0f;
    std::vector<float> cb(48, -7.25f);
    const size_t at = 20;
    cb[at] = ndc_x * w; cb[at + 1] = ndc_y * w; cb[at + 2] = 0.4f * w; cb[at + 3] = w;

    ClipBest b;
    scan_buffer_clip(cb.data(), cb.size() * sizeof(float), t, b);
    CHECK(b.hit);
    CHECK(b.offset == at * sizeof(float));
    CHECK(b.w == doctest::Approx(w));
    CHECK(b.at_x == doctest::Approx(t.screen_x).epsilon(0.01));
    CHECK(b.at_y == doctest::Approx(t.screen_y).epsilon(0.01));
    CHECK_FALSE(b.col_major);
}

TEST_CASE("clip-space scan ignores points behind the camera") {
    ClipTarget t;
    t.screen_w = 1920.0f; t.screen_h = 1080.0f;
    t.screen_x = 960.0f;  t.screen_y = 400.0f;
    const float w = -23.5f;                    // negative w: behind the eye
    const float ndc_x = 0.0f, ndc_y = 0.25926f;
    std::vector<float> cb(48, 1.0e9f);         // junk far outside the NDC box
    cb[20] = ndc_x * w; cb[21] = ndc_y * w; cb[22] = 0.4f * w; cb[23] = w;

    ClipBest b;
    scan_buffer_clip(cb.data(), cb.size() * sizeof(float), t, b);
    CHECK_FALSE(b.hit);
}
