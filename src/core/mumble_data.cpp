#include "mumble_data.hpp"
#include <cstdlib>
#include <cstring>
namespace core {

float parse_identity_fov(const char* id, float fallback) {
    if (!id) return fallback;
    const char* key = std::strstr(id, "\"fov\"");
    if (!key) return fallback;
    const char* colon = std::strchr(key, ':');
    if (!colon) return fallback;
    char* end = nullptr;
    float v = std::strtof(colon + 1, &end);
    if (end == colon + 1 || v <= 0.0f) return fallback;
    return v;
}

AvatarState read_avatar(const LinkedMem& m) {
    AvatarState a;
    a.position = { m.fAvatarPosition[0], m.fAvatarPosition[1], m.fAvatarPosition[2] };
    bool nonzero = a.position.x!=0 || a.position.y!=0 || a.position.z!=0;
    a.valid = (m.uiTick != 0) && nonzero;
    return a;
}

uint32_t read_map_type(const LinkedMem& m) {
    uint32_t mt = 0;
    std::memcpy(&mt, m.context + 32, sizeof mt);   // serverAddress[28] + mapId(4)
    return mt;
}

GameMode classify_map_type(uint32_t map_type) {
    switch (map_type) {
        case 9: case 10: case 11: case 12:
        case 13: case 14: case 15: case 19:
            return GameMode::WvW;
        case 2: case 6: case 8:
            return GameMode::PvP;
        default:
            return GameMode::PvE;   // open world, instances, unknown/loading
    }
}

CameraState read_camera(const LinkedMem& m, float fov_y) {
    CameraState c;
    c.position = { m.fCameraPosition[0], m.fCameraPosition[1], m.fCameraPosition[2] };
    c.front    = normalized({ m.fCameraFront[0], m.fCameraFront[1], m.fCameraFront[2] });
    Vec3 up    = { m.fCameraTop[0], m.fCameraTop[1], m.fCameraTop[2] };
    // GW2's real camera up avoids look_at gimbal-lock when looking straight
    // up/down. If the game gives no up, pick a world axis NOT parallel to front
    // (world-up would still gimbal-lock a straight-down camera).
    if (up.x == 0 && up.y == 0 && up.z == 0)
        up = (c.front.y * c.front.y > 0.98f) ? Vec3{0.0f, 0.0f, 1.0f} : Vec3{0.0f, 1.0f, 0.0f};
    c.up       = normalized(up);
    c.fov_y    = fov_y;
    return c;
}
}
