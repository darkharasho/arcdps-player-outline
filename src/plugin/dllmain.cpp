#include <windows.h>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cstdio>
#include "imgui.h"
#include "arcdps.h"
#include "mumble_link.hpp"
#include "marker.hpp"
#include "camera.hpp"
#include "one_euro.hpp"
#include "config.hpp"

static arcdps_exports g_arc{};
static const char* kName  = "player_outline";
static const char* kBuild = "0.1.0";
static plugin::MumbleReader g_reader;
static plugin::Config g_cfg;
static core::OneEuro g_fx, g_fy, g_fh;   // smooth anchor x/y and body height (px)
static HMODULE g_self = nullptr;
static char g_ini[MAX_PATH] = {0};

static void reset_smoothing() { g_fx.reset(); g_fy.reset(); g_fh.reset(); }

static void resolve_ini_path() {
    if (!GetModuleFileNameA(g_self, g_ini, MAX_PATH)) { g_ini[0] = 0; return; }
    char* slash = std::strrchr(g_ini, '\\');
    if (!slash) slash = std::strrchr(g_ini, '/');
    char* tail = slash ? slash + 1 : g_ini;
    std::strncpy(tail, "arcdps_player_outline.ini",
                 (size_t)(g_ini + MAX_PATH - tail - 1));
}

static unsigned cfg_rgba() {
    return IM_COL32((int)(g_cfg.color[0] * 255), (int)(g_cfg.color[1] * 255),
                    (int)(g_cfg.color[2] * 255), (int)(g_cfg.opacity * 255));
}

// --- Rally point: a reference position you drop with a hotkey (runtime only) ---
static bool       g_rally_set = false;
static core::Vec3 g_rally_pos{};
static core::Vec3 g_last_avatar{};
static bool       g_have_avatar = false;
static bool       g_rebinding = false;

static unsigned lerp_rgba(const float a[3], const float b[3], float t, float opacity) {
    if (t < 0) t = 0; if (t > 1) t = 1;
    float r  = a[0] + (b[0] - a[0]) * t;
    float g  = a[1] + (b[1] - a[1]) * t;
    float bl = a[2] + (b[2] - a[2]) * t;
    return IM_COL32((int)(r * 255), (int)(g * 255), (int)(bl * 255), (int)(opacity * 255));
}

// called each frame by arcdps; not_charsel_or_loading==1 when safe to draw
static uintptr_t imgui_cb(uint32_t not_charsel_or_loading, uint32_t /*hide*/) {
    if (!not_charsel_or_loading || !g_cfg.enabled) { reset_smoothing(); return 0; }

    core::AvatarState avatar; core::CameraState cam;
    if (!g_reader.sample(avatar, cam)) { reset_smoothing(); return 0; }

    // Track latest avatar position so the rally hotkey works even when the
    // marker itself is off-screen.
    g_last_avatar = avatar.position;
    g_have_avatar = true;

    ImVec2 sz = ImGui::GetIO().DisplaySize;
    core::ScreenPoint feet = core::world_to_screen(avatar.position, cam, sz.x, sz.y);
    if (feet.behind || !feet.on_screen) { reset_smoothing(); return 0; }

    float dt = ImGui::GetIO().DeltaTime;

    // Distance to the rally point, and optional tint of the marker by that distance.
    unsigned rgba = cfg_rgba();
    float rally_dist = -1.0f;
    if (g_rally_set) {
        rally_dist = core::length(avatar.position - g_rally_pos);
        if (g_cfg.rally_tint) {
            const float near_c[3] = {0.25f, 1.0f, 0.35f};   // green (with group)
            const float far_c[3]  = {1.0f, 0.35f, 0.25f};   // red (drifted)
            float span = g_cfg.rally_far - g_cfg.rally_near;
            float t = span > 0 ? (rally_dist - g_cfg.rally_near) / span : 0.0f;
            rgba = lerp_rgba(near_c, far_c, t, g_cfg.opacity);
        }
    }

    switch (g_cfg.style) {
        case plugin::MarkerStyle::SilhouetteGlow: {
            // Size by WORLD distance, not the projected feet->head gap. The gap
            // foreshortens (collapses when the camera pitches down); distance
            // gives a stable on-screen height that only shrinks as you truly move
            // away. focal = perpendicular pixels per world unit at unit distance.
            float focal = (sz.y * 0.5f) / std::tan(cam.fov_y * 0.5f);
            float dist  = core::length(cam.position - avatar.position);
            if (dist < 0.5f) dist = 0.5f;
            float body_px = focal * g_cfg.char_height / dist;
            if (body_px < 22.0f) body_px = 22.0f;   // stay readable when far
            float sx = g_fx.filter(feet.x, dt);
            float sy = g_fy.filter(feet.y, dt);
            float sh = g_fh.filter(body_px, dt);
            plugin::draw_silhouette_glow(sx, sy - sh * 0.5f, sh, rgba,
                                         g_cfg.glow_width, g_cfg.glow_amount);
            break;
        }
        case plugin::MarkerStyle::GroundRing: {
            core::ScreenPoint p = feet;
            p.x = g_fx.filter(feet.x, dt);
            p.y = g_fy.filter(feet.y, dt);
            plugin::draw_ground_ring(p, g_cfg.ring_radius, rgba);
            break;
        }
        case plugin::MarkerStyle::Chevron: {
            core::Vec3 hw = avatar.position + core::Vec3{0.0f, g_cfg.head_offset, 0.0f};
            core::ScreenPoint h = core::world_to_screen(hw, cam, sz.x, sz.y);
            float ax = h.behind ? feet.x : h.x;
            float ay = h.behind ? feet.y : h.y;
            float sx = g_fx.filter(ax, dt);
            float sy = g_fy.filter(ay, dt);
            plugin::draw_chevron(sx, sy, g_cfg.chevron_size, rgba);
            break;
        }
    }

    // Rally point: on-screen diamond + distance readout near the self marker.
    if (g_rally_set && g_cfg.rally_show) {
        core::ScreenPoint rp = core::world_to_screen(g_rally_pos, cam, sz.x, sz.y);
        if (!rp.behind && rp.on_screen)
            plugin::draw_rally_marker(rp.x, rp.y, 18.0f, IM_COL32(255, 210, 60, 235));
        char lbl[32];
        std::snprintf(lbl, sizeof lbl, "%.0f m", rally_dist);
        ImGui::GetBackgroundDrawList()->AddText(ImVec2(feet.x + 12, feet.y + 4),
                                                IM_COL32(255, 255, 255, 235), lbl);
    }
    return 0;
}

// arcdps window-message callback: rally hotkey + rebind capture.
static uintptr_t wnd_cb(HWND, UINT msg, WPARAM wParam, LPARAM) {
    if (msg == WM_KEYDOWN) {
        if (g_rebinding) { g_cfg.rally_key = (int)wParam; g_rebinding = false; return 0; }
        if ((int)wParam == g_cfg.rally_key && g_have_avatar) {
            g_rally_pos = g_last_avatar;
            g_rally_set = true;
        }
    }
    return (uintptr_t)msg;   // pass the message through to the game
}

static uintptr_t options_cb() {
    plugin::draw_options(g_cfg);
    ImGui::Separator();
    if (g_rally_set) {
        float d = g_have_avatar ? core::length(g_last_avatar - g_rally_pos) : 0.0f;
        ImGui::Text("Rally set - %.0f m away", d);
    } else {
        ImGui::TextUnformatted("Rally: not set");
    }
    if (ImGui::Button("Set at me") && g_have_avatar) { g_rally_pos = g_last_avatar; g_rally_set = true; }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) g_rally_set = false;
    ImGui::SameLine();
    if (ImGui::Button(g_rebinding ? "press a key..." : "Rebind hotkey")) g_rebinding = true;
    ImGui::SameLine();
    ImGui::Text("(hotkey VK 0x%02X)", g_cfg.rally_key);
    return 0;
}

static arcdps_exports* mod_init() {
    resolve_ini_path();
    if (g_ini[0]) plugin::load_config(g_cfg, g_ini);
    g_arc.size = sizeof(arcdps_exports);
    g_arc.sig = 0x504F4C4E;
    g_arc.imguivers = IMGUI_VERSION_NUM;
    g_arc.out_name = kName;
    g_arc.out_build = kBuild;
    g_arc.imgui = (void*)imgui_cb;
    g_arc.options_end = (void*)options_cb;
    g_arc.wnd_nofilter = (void*)wnd_cb;
    return &g_arc;
}
static uintptr_t mod_release() {
    if (g_ini[0]) plugin::save_config(g_cfg, g_ini);
    return 0;
}

extern "C" __declspec(dllexport)
void* get_init_addr(char* /*arcversion*/, void* imguictx, void* /*id3dptr*/,
                    HMODULE /*arcdll*/, void* mallocfn, void* freefn,
                    uint32_t /*d3dversion*/) {
    ImGui::SetCurrentContext((ImGuiContext*)imguictx);
    ImGui::SetAllocatorFunctions(
        (void*(*)(size_t,void*))mallocfn, (void(*)(void*,void*))freefn);
    return (void*)mod_init;
}

extern "C" __declspec(dllexport)
void* get_release_addr() { return (void*)mod_release; }

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) g_self = hinst;
    return TRUE;
}
