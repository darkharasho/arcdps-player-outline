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
struct CameraState { Vec3 position{}; Vec3 front{}; Vec3 up{0.0f, 1.0f, 0.0f}; float fov_y{1.222f}; };

// Broad game mode, derived from GW2's MumbleContext mapType. Unknown/loading
// maps classify as PvE so the marker fails toward being shown.
enum class GameMode { PvE, WvW, PvP };

// Player race, read from the identity JSON "race" field (0..4). Unknown when
// the field is absent or out of range.
enum class GameRace { Asura, Charr, Human, Norn, Sylvari, Unknown };

float parse_identity_fov(const char* utf8_identity, float fallback);
GameRace parse_identity_race(const char* utf8_identity);
AvatarState read_avatar(const LinkedMem& m);
CameraState read_camera(const LinkedMem& m, float fov_y);

// mapType lives in the GW2 MumbleContext packed into LinkedMem::context, at
// byte offset 32 (after serverAddress[28] + uint32 mapId).
uint32_t read_map_type(const LinkedMem& m);
GameMode classify_map_type(uint32_t map_type);

// uiState bitfield: uint32 at context offset 48 (after mapType, shardId,
// instance, buildId). Bit 0 = map open, bit 3 = game has focus.
uint32_t read_ui_state(const LinkedMem& m);
bool     ui_map_open(uint32_t ui_state);
bool     ui_game_focused(uint32_t ui_state);
}
