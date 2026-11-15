#pragma once
// THROWAWAY SPIKE CODE — differential draw identification.
//
// Seven constant-buffer captures proved the player's transform is not in a
// vertex-stage constant buffer, so this probe stops looking for a position and
// identifies the character's draws by behaviour instead: GW2's first-person
// camera does not draw your character, so the draws present in third person
// and absent in first person ARE your character.
//
// This header is the pure, Windows-free half so it can be unit tested natively.
#include <cstdint>
#include <cstddef>

namespace spike {

// A draw's identity, independent of how any transform is encoded: which shaders
// ran, how much geometry, and which render target it went to. The render target
// matters because a first-person frame still draws your character into the
// shadow map — without it, a shadow-pass draw would mask the very disappearance
// we are looking for.
struct Fingerprint {
    const void* vs  = nullptr;
    const void* ps  = nullptr;
    const void* rtv = nullptr;
    uint32_t index_count = 0;
    uint32_t instances   = 0;
    uint8_t  kind        = 0;   // 0 DrawIndexed, 1 DrawIndexedInstanced, 2 DrawInstanced
};

inline bool same(const Fingerprint& a, const Fingerprint& b) {
    return a.vs == b.vs && a.ps == b.ps && a.rtv == b.rtv &&
           a.index_count == b.index_count && a.instances == b.instances &&
           a.kind == b.kind;
}

inline uint64_t mix64(uint64_t x) {
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

inline uint64_t hash_fp(const Fingerprint& f) {
    uint64_t h = mix64((uint64_t)(uintptr_t)f.vs);
    h = mix64(h ^ (uint64_t)(uintptr_t)f.ps);
    h = mix64(h ^ (uint64_t)(uintptr_t)f.rtv);
    h = mix64(h ^ ((uint64_t)f.index_count << 32 | f.instances));
    return mix64(h ^ f.kind);
}

// Open-addressed count table. Sized well above a GW2 frame's observed ~6200
// draws so a frame of entirely unique fingerprints still fits without evicting.
constexpr int kTableSlots = 16384;   // power of two, linear probe

struct FpTable {
    Fingerprint fp[kTableSlots];
    int32_t     count[kTableSlots];   // 0 means the slot is empty
    int  used     = 0;
    int  draws    = 0;                // total draws offered, including duplicates
    bool overflow = false;            // table filled up; results are incomplete

    FpTable() { clear(); }

    void clear() {
        for (int i = 0; i < kTableSlots; ++i) { fp[i] = Fingerprint{}; count[i] = 0; }
        used = 0; draws = 0; overflow = false;
    }

    void add(const Fingerprint& f) {
        ++draws;
        size_t i = (size_t)(hash_fp(f) & (kTableSlots - 1));
        for (int probe = 0; probe < kTableSlots; ++probe) {
            size_t s = (i + (size_t)probe) & (kTableSlots - 1);
            if (count[s] == 0) {
                // Refuse to insert into a full table rather than spin forever.
                if (used >= kTableSlots - 1) { overflow = true; return; }
                fp[s] = f; count[s] = 1; ++used; return;
            }
            if (same(fp[s], f)) { ++count[s]; return; }
        }
        overflow = true;
    }

    // Occurrences of f in this table; 0 if absent.
    int find(const Fingerprint& f) const {
        size_t i = (size_t)(hash_fp(f) & (kTableSlots - 1));
        for (int probe = 0; probe < kTableSlots; ++probe) {
            size_t s = (i + (size_t)probe) & (kTableSlots - 1);
            if (count[s] == 0) return 0;
            if (same(fp[s], f)) return count[s];
        }
        return 0;
    }
};

// Fingerprints appearing in EVERY `present` table and NO `absent` table.
// Invokes sink(fingerprint, fewest_occurrences_in_any_present_table) for each.
//
// Requiring all-present and none-absent is deliberate: a single third-person
// frame contains thousands of draws that merely happen to be missing from one
// first-person frame (particles, other players, LOD changes). Only a draw that
// survives every third-person capture and appears in none of the first-person
// ones is a real candidate.
template <class Sink>
void diff(const FpTable* present, int n_present,
          const FpTable* absent,  int n_absent, Sink sink) {
    if (n_present <= 0) return;
    const FpTable& base = present[0];
    for (int s = 0; s < kTableSlots; ++s) {
        if (base.count[s] == 0) continue;
        const Fingerprint& f = base.fp[s];
        int fewest = base.count[s];
        bool ok = true;
        for (int p = 1; p < n_present && ok; ++p) {
            int c = present[p].find(f);
            if (c == 0) ok = false;
            else if (c < fewest) fewest = c;
        }
        for (int a = 0; a < n_absent && ok; ++a)
            if (absent[a].find(f) != 0) ok = false;
        if (ok) sink(f, fewest);
    }
}

}  // namespace spike
