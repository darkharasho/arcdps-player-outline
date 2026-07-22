#include <windows.h>
#include <cstdint>
#include <cmath>
#include <cstring>
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

// called each frame by arcdps; not_charsel_or_loading==1 when safe to draw
static uintptr_t imgui_cb(uint32_t not_charsel_or_loading, uint32_t /*hide*/) {
    if (!not_charsel_or_loading || !g_cfg.enabled) { reset_smoothing(); return 0; }

    core::AvatarState avatar; core::CameraState cam;
    if (!g_reader.sample(avatar, cam)) { reset_smoothing(); return 0; }

    ImVec2 sz = ImGui::GetIO().DisplaySize;
    core::ScreenPoint feet = core::world_to_screen(avatar.position, cam, sz.x, sz.y);
    if (feet.behind || !feet.on_screen) { reset_smoothing(); return 0; }

    float dt = ImGui::GetIO().DeltaTime;
    unsigned rgba = cfg_rgba();

    // Anchor = the point we smooth. Feet for most styles; the head for chevron.
    float ax = feet.x, ay = feet.y;
    if (g_cfg.style == plugin::MarkerStyle::Chevron) {
        core::Vec3 hw = avatar.position + core::Vec3{0.0f, g_cfg.head_offset, 0.0f};
        core::ScreenPoint h = core::world_to_screen(hw, cam, sz.x, sz.y);
        if (!h.behind) { ax = h.x; ay = h.y; }
    }
    float sx = g_fx.filter(ax, dt);
    float sy = g_fy.filter(ay, dt);
    float ox = sx - feet.x, oy = sy - feet.y;   // de-jitter offset (feet-anchored styles)

    switch (g_cfg.style) {
        case plugin::MarkerStyle::GroundRing:
        case plugin::MarkerStyle::RingPip: {
            // Project a world circle on the ground plane so it reads as a circle
            // overhead and an ellipse from the side. Shift by the smoothed offset.
            const int N = 40;
            ImVec2 pts[N];
            bool ok = true;
            for (int i = 0; i < N; ++i) {
                float a = 6.2831853f * i / N;
                core::Vec3 wp = avatar.position +
                    core::Vec3{std::cos(a) * g_cfg.ring_radius, 0.0f, std::sin(a) * g_cfg.ring_radius};
                core::ScreenPoint p = core::world_to_screen(wp, cam, sz.x, sz.y);
                if (p.behind) { ok = false; break; }
                pts[i] = ImVec2(p.x + ox, p.y + oy);
            }
            if (ok) {
                plugin::draw_ground_ring(pts, N, rgba, 2.5f);
                if (g_cfg.style == plugin::MarkerStyle::RingPip) {
                    core::ScreenPoint hp = core::world_to_screen(
                        avatar.position + core::Vec3{0.0f, 1.2f, 0.0f}, cam, sz.x, sz.y);
                    if (!hp.behind) plugin::draw_pip(hp.x + ox, hp.y + oy, 4.0f, rgba);
                }
            }
            break;
        }
        case plugin::MarkerStyle::SilhouetteGlow: {
            float focal = (sz.y * 0.5f) / std::tan(cam.fov_y * 0.5f);
            float dist  = core::length(cam.position - avatar.position);
            if (dist < 0.5f) dist = 0.5f;
            float body_px = focal * g_cfg.char_height / dist;
            if (body_px < 22.0f) body_px = 22.0f;
            float sh = g_fh.filter(body_px, dt);
            plugin::draw_silhouette_glow(sx, sy - sh * 0.5f, sh, rgba,
                                         g_cfg.glow_width, g_cfg.glow_amount);
            break;
        }
        case plugin::MarkerStyle::Chevron: {
            plugin::draw_chevron(sx, sy, g_cfg.chevron_size, rgba);
            break;
        }
        case plugin::MarkerStyle::Beam: {
            core::ScreenPoint top = core::world_to_screen(
                avatar.position + core::Vec3{0.0f, g_cfg.beam_height, 0.0f}, cam, sz.x, sz.y);
            float top_y = top.behind ? (sy - 200.0f) : (top.y + oy);
            plugin::draw_beam(sx, sy, top_y, g_cfg.beam_width, rgba);
            break;
        }
    }
    return 0;
}

static uintptr_t options_cb() { plugin::draw_options(g_cfg); return 0; }

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
