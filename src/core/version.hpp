#pragma once
// Release tag parsing and version comparison for the update notice.
// Pure: no Windows headers, no allocation.
#include <cstddef>
#include <cstring>

namespace core {

// Copies the tag from a ".../releases/tag/<tag>" URL into out. The tag ends at
// the first '/', '?', '#' or the end of the string. False if there is no tag
// or it does not fit.
inline bool parse_release_tag(const char* location, char* out, size_t out_size) {
    if (!location || !out || out_size == 0) return false;
    static const char kMarker[] = "/releases/tag/";
    const char* p = std::strstr(location, kMarker);
    if (!p) return false;
    p += sizeof(kMarker) - 1;
    size_t n = 0;
    while (p[n] && p[n] != '/' && p[n] != '?' && p[n] != '#') ++n;
    if (n == 0 || n >= out_size) return false;
    std::memcpy(out, p, n);
    out[n] = '\0';
    return true;
}

// "v1.2.3", "1.2.3" or "1.2.3-anything" -> {1, 2, 3}. False for anything else.
inline bool parse_version(const char* s, int out[3]) {
    if (!s) return false;
    if (*s == 'v' || *s == 'V') ++s;
    for (int i = 0; i < 3; ++i) {
        if (*s < '0' || *s > '9') return false;
        int v = 0, digits = 0;
        while (*s >= '0' && *s <= '9') {
            if (++digits > 6) return false;      // keeps v well inside int
            v = v * 10 + (*s - '0');
            ++s;
        }
        out[i] = v;
        if (i < 2) { if (*s != '.') return false; ++s; }
    }
    return true;
}

// True only when both versions parse and remote is strictly higher.
inline bool is_newer(const char* remote, const char* local) {
    int r[3], l[3];
    if (!parse_version(remote, r) || !parse_version(local, l)) return false;
    for (int i = 0; i < 3; ++i)
        if (r[i] != l[i]) return r[i] > l[i];
    return false;
}

}  // namespace core
