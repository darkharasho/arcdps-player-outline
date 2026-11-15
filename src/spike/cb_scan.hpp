#pragma once
// THROWAWAY SPIKE CODE — see docs/superpowers/specs/ for the real feature set.
// Pure logic for "does this constant buffer contain our avatar's position?".
// Kept free of Windows/D3D so it can be unit-tested natively.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "vec3.hpp"

namespace spike {

// GW2 reports MumbleLink positions in meters; its world/render coords are in
// inches. A shader may also hold the position camera-relative, and may use a
// different axis order/handedness than MumbleLink's Y-up. We therefore test a
// small set of candidate vectors, each with both an exact (same component
// order) and a permuted (any axis order/sign) match.
constexpr float kInchesPerMeter = 39.3701f;
constexpr int   kMaxCandidates  = 4;

struct Candidate {
    const char* name;   // "world_m", "world_in", "camrel_m", "camrel_in"
    core::Vec3  v;
    float       tol;    // match tolerance in that candidate's own units
};

// Fills out[] with the candidates to hunt for. Returns how many were written.
inline int build_candidates(const core::Vec3& avatar, const core::Vec3& cam,
                            Candidate out[kMaxCandidates]) {
    const core::Vec3 rel = avatar - cam;
    const float tol_m = 0.10f;                      // 10 cm
    const float tol_i = tol_m * kInchesPerMeter;
    out[0] = {"world_m",  avatar,                     tol_m};
    out[1] = {"world_in", avatar * kInchesPerMeter,   tol_i};
    out[2] = {"camrel_m", rel,                        tol_m};
    out[3] = {"camrel_in", rel * kInchesPerMeter,     tol_i};
    return 4;
}

struct ScanBest {
    bool   found  = false;    // a triple was compared at all
    bool   hit    = false;    // ...and it was within tolerance
    size_t offset = 0;        // byte offset into the buffer
    float  dist   = 0.0f;
    float  value[3] = {0.0f, 0.0f, 0.0f};
};

// Per candidate: closest same-order triple, closest any-order triple, and
// closest column-major translation. HLSL matrices are column-major by default,
// which puts a world matrix's translation at elements 3/7/11 — 16 bytes apart,
// not contiguous — so a contiguous-only scan misses the most likely encoding.
struct ScanResult {
    ScanBest exact;
    ScanBest permuted;
    ScanBest col_major;
};

namespace detail {

inline bool finite3(const float* f) {
    for (int i = 0; i < 3; ++i)
        if (!std::isfinite(f[i])) return false;
    return true;
}

// |a - b| for three components.
inline float dist3(const float* a, const float* b) {
    float s = 0.0f;
    for (int i = 0; i < 3; ++i) { float d = a[i] - b[i]; s += d * d; }
    return std::sqrt(s);
}

// Magnitudes ascending, so an axis-permuted / sign-flipped copy of the same
// point compares equal.
inline void sorted_abs(const float* in, float* out) {
    for (int i = 0; i < 3; ++i) out[i] = std::fabs(in[i]);
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2 - i; ++j)
            if (out[j] > out[j + 1]) { float t = out[j]; out[j] = out[j + 1]; out[j + 1] = t; }
}

inline void keep_best(ScanBest& b, float d, size_t off, const float* v, float tol) {
    if (b.found && d >= b.dist) return;
    b.found = true;
    b.hit = (d <= tol);
    b.dist = d;
    b.offset = off;
    for (int i = 0; i < 3; ++i) b.value[i] = v[i];
}

}  // namespace detail

// Scans every 4-byte-aligned float triple in [data, data+bytes) against each
// candidate, recording the nearest match per candidate. Near misses are kept
// too: "closest we got was 3 metres off" is a real answer, not a null result.
inline void scan_buffer(const void* data, size_t bytes,
                        const Candidate* cands, int n_cands,
                        ScanResult* out) {
    if (!data || bytes < 12 || n_cands <= 0) return;

    float cand_sorted[kMaxCandidates][3];
    for (int c = 0; c < n_cands; ++c)
        detail::sorted_abs(&cands[c].v.x, cand_sorted[c]);

    const auto* base = static_cast<const unsigned char*>(data);
    const size_t last = bytes - 12;
    for (size_t off = 0; off <= last; off += 4) {
        float v[3];
        std::memcpy(v, base + off, sizeof(v));
        if (detail::finite3(v)) {
            float v_sorted[3];
            detail::sorted_abs(v, v_sorted);
            for (int c = 0; c < n_cands; ++c) {
                detail::keep_best(out[c].exact,
                                  detail::dist3(v, &cands[c].v.x), off, v, cands[c].tol);
                detail::keep_best(out[c].permuted,
                                  detail::dist3(v_sorted, cand_sorted[c]), off, v, cands[c].tol);
            }
        }

        // Column-major: elements at off, off+16, off+32.
        if (off + 44 <= bytes) {
            float cm[3];
            for (int i = 0; i < 3; ++i)
                std::memcpy(&cm[i], base + off + (size_t)i * 16, sizeof(float));
            if (detail::finite3(cm))
                for (int c = 0; c < n_cands; ++c)
                    detail::keep_best(out[c].col_major,
                                      detail::dist3(cm, &cands[c].v.x), off, cm, cands[c].tol);
        }
    }
}

// True if any candidate matched within tolerance, in any of the layouts.
inline bool any_hit(const ScanResult* r, int n) {
    for (int i = 0; i < n; ++i)
        if (r[i].exact.hit || r[i].permuted.hit || r[i].col_major.hit) return true;
    return false;
}

// --- clip-space detection --------------------------------------------------
// If a shader receives a combined world-view-projection matrix, our position
// never appears as a recognisable world coordinate: it has already been
// transformed into clip space. But we know where the character lands on screen,
// so we can invert the test — take any float4 as a clip-space point, divide by
// w, and ask whether it lands on our character.
struct ClipTarget {
    float screen_x = 0.0f, screen_y = 0.0f;   // where we know the avatar is, in px
    float screen_w = 0.0f, screen_h = 0.0f;
    float tol_px   = 50.0f;
};

struct ClipBest {
    bool   found = false, hit = false;
    size_t offset = 0;
    float  dist_px = 0.0f;
    float  w = 0.0f;
    float  at_x = 0.0f, at_y = 0.0f;
    bool   col_major = false;
};

namespace detail {
inline void keep_clip(ClipBest& b, const ClipTarget& t, const float* v, size_t off,
                      bool col_major) {
    const float w = v[3];
    if (!std::isfinite(w) || w <= 0.5f) return;          // behind camera / degenerate
    for (int i = 0; i < 3; ++i) if (!std::isfinite(v[i])) return;
    const float ndc_x = v[0] / w, ndc_y = v[1] / w;
    if (ndc_x < -1.5f || ndc_x > 1.5f || ndc_y < -1.5f || ndc_y > 1.5f) return;
    const float sx = (ndc_x * 0.5f + 0.5f) * t.screen_w;
    const float sy = (1.0f - (ndc_y * 0.5f + 0.5f)) * t.screen_h;
    const float dx = sx - t.screen_x, dy = sy - t.screen_y;
    const float d = std::sqrt(dx * dx + dy * dy);
    if (b.found && d >= b.dist_px) return;
    b.found = true; b.hit = (d <= t.tol_px); b.dist_px = d;
    b.offset = off; b.w = w; b.at_x = sx; b.at_y = sy; b.col_major = col_major;
}
}  // namespace detail

// Scans for a float4 that, treated as a clip-space position, projects onto the
// avatar's known screen position. Covers both matrix layouts.
inline void scan_buffer_clip(const void* data, size_t bytes,
                             const ClipTarget& target, ClipBest& out) {
    if (!data || bytes < 16 || target.screen_w <= 0.0f || target.screen_h <= 0.0f) return;
    const auto* base = static_cast<const unsigned char*>(data);
    const size_t last = bytes - 16;
    for (size_t off = 0; off <= last; off += 4) {
        float v[4];
        std::memcpy(v, base + off, sizeof(v));
        detail::keep_clip(out, target, v, off, false);

        if (off + 60 <= bytes) {
            float cm[4];
            for (int i = 0; i < 4; ++i)
                std::memcpy(&cm[i], base + off + (size_t)i * 16, sizeof(float));
            detail::keep_clip(out, target, cm, off, true);
        }
    }
}

}  // namespace spike
