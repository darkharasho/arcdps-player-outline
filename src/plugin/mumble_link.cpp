#include "mumble_link.hpp"
#include <windows.h>
#include <cstring>

namespace plugin {

static float fov_from_identity(const wchar_t* wid) {
    char utf8[512] = {0};
    WideCharToMultiByte(CP_UTF8, 0, wid, -1, utf8, sizeof(utf8)-1, nullptr, nullptr);
    return core::parse_identity_fov(utf8, 1.222f);   // ~70deg vertical fallback
}

MumbleReader::~MumbleReader() {
    if (mem_) UnmapViewOfFile((LPCVOID)mem_);
    if (handle_) CloseHandle((HANDLE)handle_);
}

bool MumbleReader::open() {
    if (mem_) return true;
    HANDLE h = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                  0, sizeof(core::LinkedMem), L"MumbleLink");
    if (!h) return false;
    void* view = MapViewOfFile(h, FILE_MAP_READ, 0, 0, sizeof(core::LinkedMem));
    if (!view) { CloseHandle(h); return false; }
    handle_ = h; mem_ = (const core::LinkedMem*)view;
    return true;
}

bool MumbleReader::sample(core::AvatarState& avatar, core::CameraState& cam) {
    if (!mem_ && !open()) return false;
    if (mem_->uiTick == last_tick_) { /* stale frame, but still usable */ }
    last_tick_ = mem_->uiTick;
    avatar = core::read_avatar(*mem_);
    if (!avatar.valid) return false;
    float fov = fov_from_identity(mem_->identity);
    cam = core::read_camera(*mem_, fov);
    return true;
}
}
