#pragma once
#include <cstdint>
#include "vec3.hpp"
namespace core {

// Exact GW2 MumbleLink shared-memory layout.
struct LinkedMem {
    uint32_t uiVersion;
    uint32_t uiTick;
    float fAvatarPosition[3];
    float fAvatarFront[3];
    float fAvatarTop[3];
    wchar_t name[256];
    float fCameraPosition[3];
    float fCameraFront[3];
    float fCameraTop[3];
    wchar_t identity[256];
    uint32_t context_len;
    unsigned char context[256];
    wchar_t description[2048];
};

struct AvatarState { Vec3 position{}; bool valid{false}; };
struct CameraState { Vec3 position{}; Vec3 front{}; float fov_y{1.222f}; };

float parse_identity_fov(const char* utf8_identity, float fallback);
AvatarState read_avatar(const LinkedMem& m);
CameraState read_camera(const LinkedMem& m, float fov_y);
}
