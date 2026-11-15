#include "doctest.h"
#include "fingerprint.hpp"
#include <vector>
using namespace spike;

static Fingerprint fp(const void* vs, const void* ps, const void* rtv,
                      uint32_t ic, uint32_t inst = 1, uint8_t kind = 0) {
    Fingerprint f; f.vs = vs; f.ps = ps; f.rtv = rtv;
    f.index_count = ic; f.instances = inst; f.kind = kind;
    return f;
}
static void* const VS_CHAR = (void*)0x1000;
static void* const PS_CHAR = (void*)0x2000;
static void* const RTV_MAIN   = (void*)0x3000;
static void* const RTV_SHADOW = (void*)0x4000;

// Collect diff results into a vector for inspection.
static std::vector<std::pair<Fingerprint,int>> run_diff(
        const FpTable* p, int np, const FpTable* a, int na) {
    std::vector<std::pair<Fingerprint,int>> out;
    diff(p, np, a, na, [&](const Fingerprint& f, int n) { out.push_back({f, n}); });
    return out;
}

TEST_CASE("table counts repeats of the same fingerprint") {
    FpTable t;
    Fingerprint f = fp(VS_CHAR, PS_CHAR, RTV_MAIN, 900);
    t.add(f); t.add(f); t.add(f);
    CHECK(t.find(f) == 3);
    CHECK(t.used == 1);
    CHECK(t.draws == 3);
    CHECK_FALSE(t.overflow);
}

TEST_CASE("fingerprints differing in any single field are distinct") {
    FpTable t;
    Fingerprint base = fp(VS_CHAR, PS_CHAR, RTV_MAIN, 900, 1, 0);
    t.add(base);
    CHECK(t.find(fp((void*)0x1001, PS_CHAR, RTV_MAIN, 900, 1, 0)) == 0);
    CHECK(t.find(fp(VS_CHAR, (void*)0x2001, RTV_MAIN, 900, 1, 0)) == 0);
    CHECK(t.find(fp(VS_CHAR, PS_CHAR, RTV_SHADOW, 900, 1, 0)) == 0);
    CHECK(t.find(fp(VS_CHAR, PS_CHAR, RTV_MAIN, 901, 1, 0)) == 0);
    CHECK(t.find(fp(VS_CHAR, PS_CHAR, RTV_MAIN, 900, 2, 0)) == 0);
    CHECK(t.find(fp(VS_CHAR, PS_CHAR, RTV_MAIN, 900, 1, 1)) == 0);
    CHECK(t.find(base) == 1);
}

TEST_CASE("diff isolates the draw present in all 3P frames and no 1P frame") {
    Fingerprint character = fp(VS_CHAR, PS_CHAR, RTV_MAIN, 900);
    Fingerprint terrain   = fp((void*)0x11, (void*)0x22, RTV_MAIN, 5000);
    FpTable third[2], first[2];
    for (int i = 0; i < 2; ++i) {
        third[i].add(character);
        third[i].add(terrain);
        first[i].add(terrain);
    }
    auto out = run_diff(third, 2, first, 2);
    REQUIRE(out.size() == 1);
    CHECK(same(out[0].first, character));
}

TEST_CASE("a draw missing from one 3P frame is rejected (transient noise)") {
    // Another player's armour piece: in one third-person frame, not the other.
    Fingerprint other = fp((void*)0x55, (void*)0x66, RTV_MAIN, 700);
    FpTable third[2], first[1];
    third[0].add(other);            // third[1] never sees it
    auto out = run_diff(third, 2, first, 1);
    CHECK(out.empty());
}

TEST_CASE("a draw appearing in any 1P frame is rejected") {
    Fingerprint f = fp(VS_CHAR, PS_CHAR, RTV_MAIN, 900);
    FpTable third[2], first[2];
    third[0].add(f); third[1].add(f);
    first[1].add(f);                // present in just one first-person frame
    auto out = run_diff(third, 2, first, 2);
    CHECK(out.empty());
}

TEST_CASE("shadow-pass draw does not mask the main-pass disappearance") {
    // The character is still drawn into the shadow map in first person. Because
    // the render target is part of the fingerprint, the main-pass draw is still
    // correctly reported as third-person-only.
    Fingerprint main_pass   = fp(VS_CHAR, PS_CHAR, RTV_MAIN,   900);
    Fingerprint shadow_pass = fp(VS_CHAR, PS_CHAR, RTV_SHADOW, 900);
    FpTable third[1], first[1];
    third[0].add(main_pass); third[0].add(shadow_pass);
    first[0].add(shadow_pass);
    auto out = run_diff(third, 1, first, 1);
    REQUIRE(out.size() == 1);
    CHECK(same(out[0].first, main_pass));
    CHECK(out[0].first.rtv == RTV_MAIN);
}

TEST_CASE("reported count is the fewest occurrences across present frames") {
    Fingerprint f = fp(VS_CHAR, PS_CHAR, RTV_MAIN, 900);
    FpTable third[2], first[1];
    third[0].add(f); third[0].add(f); third[0].add(f);
    third[1].add(f); third[1].add(f);
    auto out = run_diff(third, 2, first, 1);
    REQUIRE(out.size() == 1);
    CHECK(out[0].second == 2);
}

TEST_CASE("no present frames yields nothing rather than everything") {
    FpTable first[1];
    first[0].add(fp(VS_CHAR, PS_CHAR, RTV_MAIN, 900));
    auto out = run_diff(nullptr, 0, first, 1);
    CHECK(out.empty());
}

TEST_CASE("clear resets a table for reuse across captures") {
    FpTable t;
    Fingerprint f = fp(VS_CHAR, PS_CHAR, RTV_MAIN, 900);
    t.add(f);
    t.clear();
    CHECK(t.find(f) == 0);
    CHECK(t.used == 0);
    CHECK(t.draws == 0);
}

TEST_CASE("many distinct fingerprints stay retrievable without overflow") {
    FpTable t;
    for (uint32_t i = 0; i < 6000; ++i)                       // ~a full GW2 frame
        t.add(fp(VS_CHAR, PS_CHAR, RTV_MAIN, 100 + i));
    CHECK_FALSE(t.overflow);
    CHECK(t.used == 6000);
    CHECK(t.find(fp(VS_CHAR, PS_CHAR, RTV_MAIN, 100)) == 1);
    CHECK(t.find(fp(VS_CHAR, PS_CHAR, RTV_MAIN, 6099)) == 1);
    CHECK(t.find(fp(VS_CHAR, PS_CHAR, RTV_MAIN, 6100)) == 0);
}

TEST_CASE("overflow is flagged rather than looping forever") {
    FpTable t;
    for (uint32_t i = 0; i < (uint32_t)kTableSlots + 10; ++i)
        t.add(fp(VS_CHAR, PS_CHAR, RTV_MAIN, i));
    CHECK(t.overflow);
    CHECK(t.used <= kTableSlots);
}
