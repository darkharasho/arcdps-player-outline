#include "mumble_link.hpp"
#include <windows.h>
#include <cstring>

namespace plugin {

MumbleReader::~MumbleReader() {
    if (own_view_) UnmapViewOfFile((LPCVOID)own_view_);
    if (own_handle_) CloseHandle((HANDLE)own_handle_);
}

void MumbleReader::ensure_link_object() {
    if (own_handle_) return;
    HANDLE h = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                  0, sizeof(core::LinkedMem), L"MumbleLink");
    if (!h) return;
    // Hold a view so the section stays alive for GW2 to write into. We never
    // read this pointer directly; sample() locates the live block by scanning.
    own_view_ = MapViewOfFile(h, FILE_MAP_READ, 0, 0, sizeof(core::LinkedMem));
    own_handle_ = h;
}

// A region holds the live MumbleLink block if it parses as version 2, is
// actively ticking, and its identity looks like the JSON GW2 writes.
static bool looks_like_mumble(const core::LinkedMem* m) {
    return m->uiVersion == 2 && m->uiTick != 0 && m->identity[0] == L'{';
}

// Walk our address space for GW2's MumbleLink block. It lives in a small
// MEM_MAPPED, readable region (Wine backs the shared section with a temp file).
static const core::LinkedMem* scan() {
    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t addr = 0;
    const uintptr_t kMax = (uintptr_t)0x00007FFFFFFFFFFFull;
    const DWORD readable = PAGE_READWRITE | PAGE_READONLY | PAGE_WRITECOPY | PAGE_EXECUTE_READ;
    while (addr < kMax && VirtualQuery((LPCVOID)addr, &mbi, sizeof mbi)) {
        uintptr_t base = (uintptr_t)mbi.BaseAddress;
        if (mbi.State == MEM_COMMIT && mbi.Type == MEM_MAPPED &&
            (mbi.Protect & readable) && !(mbi.Protect & PAGE_GUARD) &&
            mbi.RegionSize >= sizeof(core::LinkedMem) && mbi.RegionSize <= 0x20000) {
            const core::LinkedMem* m = (const core::LinkedMem*)mbi.BaseAddress;
            if (looks_like_mumble(m)) return m;
        }
        addr = base + mbi.RegionSize;
        if (addr <= base) break;   // guard against wrap
    }
    return nullptr;
}

// Confirm a cached pointer still refers to a committed, readable region large
// enough to hold a LinkedMem. Guards against the region being unmapped/reused
// between frames — dereferencing a stale pointer in-process would crash GW2.
static bool region_ok(const void* p) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(p, &mbi, sizeof mbi) == 0) return false;
    const DWORD readable = PAGE_READWRITE | PAGE_READONLY | PAGE_WRITECOPY | PAGE_EXECUTE_READ;
    if (mbi.State != MEM_COMMIT || !(mbi.Protect & readable) || (mbi.Protect & PAGE_GUARD))
        return false;
    uintptr_t end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    return (uintptr_t)p + sizeof(core::LinkedMem) <= end;
}

// Torn-read-safe copy: GW2 rewrites the struct ~60x/sec. Read the tick, copy,
// re-read the tick; if it changed we caught a half-written frame — retry.
static bool copy_stable(const core::LinkedMem* src, core::LinkedMem& dst) {
    for (int i = 0; i < 6; ++i) {
        uint32_t t1 = ((volatile const core::LinkedMem*)src)->uiTick;
        std::memcpy(&dst, src, sizeof dst);
        uint32_t t2 = ((volatile const core::LinkedMem*)src)->uiTick;
        if (t1 == t2) return true;
    }
    return false;   // never settled this frame
}

bool MumbleReader::sample(core::AvatarState& avatar, core::CameraState& cam,
                          SessionInfo& session) {
    if (!link_ || !region_ok(link_) || !looks_like_mumble(link_)) link_ = scan();
    if (!link_) return false;
    core::LinkedMem lm;
    if (!copy_stable(link_, lm)) return false;
    avatar = core::read_avatar(lm);
    if (!avatar.valid) return false;
    char utf8[512] = {0};
    WideCharToMultiByte(CP_UTF8, 0, lm.identity, -1, utf8, sizeof(utf8) - 1, nullptr, nullptr);
    cam = core::read_camera(lm, core::parse_identity_fov(utf8, 1.222f));
    session.mode = core::classify_map_type(core::read_map_type(lm));
    session.race = core::parse_identity_race(utf8);
    return true;
}

}  // namespace plugin
