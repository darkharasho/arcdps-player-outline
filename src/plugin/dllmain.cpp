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

#ifndef PLUGIN_VERSION
#define PLUGIN_VERSION "0.0.0-dev"
#endif

static arcdps_exports g_arc{};
static const char* kName  = "player_outline";
static const char* kBuild = PLUGIN_VERSION;   // injected from the git tag at build time
static plugin::MumbleReader g_reader;
static plugin::Config g_cfg;
static core::GameRace g_race = core::GameRace::Unknown;   // last sampled race (for options UI)
static core::OneEuro g_fx, g_fy, g_fh;   // smooth anchor x/y and body height (px)
static HMODULE g_self = nullptr;
static char g_ini[MAX_PATH] = {0};

static void reset_smoothing() { g_fx.reset(); g_fy.reset(); g_fh.reset(); }

static void resolve_ini_path() {
    DWORD n = GetModuleFileNameA(g_self, g_ini, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) { g_ini[0] = 0; return; }   // truncated/failed
    g_ini[n] = 0;                                            // guarantee terminated
    char* slash = std::strrchr(g_ini, '\\');
    if (!slash) slash = std::strrchr(g_ini, '/');
    char* tail = slash ? slash + 1 : g_ini;
    std::strncpy(tail, "arcdps_player_outline.ini",
                 (size_t)(g_ini + MAX_PATH - tail - 1));
    g_ini[MAX_PATH - 1] = 0;
}

static unsigned faded_rgba(float mul) {
    float a = g_cfg.opacity * mul;
    return IM_COL32((int)(g_cfg.color[0] * 255), (int)(g_cfg.color[1] * 255),
                    (int)(g_cfg.color[2] * 255), (int)(a * 255));
}

// Project a world circle on the ground plane into pts[], shifted by the smoothed
// offset. Returns false if any vertex is behind the camera.
static bool build_ground_ring(const core::AvatarState& avatar, const core::CameraState& cam,
                              ImVec2* pts, int n, float radius, float sw, float sh_,
                              float ox, float oy) {
    for (int i = 0; i < n; ++i) {
        float a = 6.2831853f * i / n;
        core::Vec3 wp = avatar.position +
            core::Vec3{std::cos(a) * radius, 0.0f, std::sin(a) * radius};
        core::ScreenPoint p = core::world_to_screen(wp, cam, sw, sh_);
        if (p.behind) return false;
        pts[i] = ImVec2(p.x + ox, p.y + oy);
    }
    return true;
}

// called each frame by arcdps; not_charsel_or_loading==1 when safe to draw,
// hide==1 when arcdps has hidden its UI (its own hide key).
static uintptr_t imgui_cb(uint32_t not_charsel_or_loading, uint32_t hide) {
    if (!not_charsel_or_loading || hide || !g_cfg.enabled) { reset_smoothing(); return 0; }

    core::AvatarState avatar; core::CameraState cam; plugin::SessionInfo session;
    if (!g_reader.sample(avatar, cam, session)) { reset_smoothing(); return 0; }
    g_race = session.race;   // cache for the options panel to highlight

    // Hide over the full-screen map / when alt-tabbed, per the user's toggles.
    if ((g_cfg.hide_when_map_open && session.map_open) ||
        (g_cfg.hide_when_unfocused && !session.focused)) {
        reset_smoothing();
        return 0;
    }

    // Per-mode gate: PvP is always off; PvE/WvW follow their toggles.
    bool mode_on = (session.mode == core::GameMode::WvW) ? g_cfg.show_in_wvw
                 : (session.mode == core::GameMode::PvE) ? g_cfg.show_in_pve
                 : false;   // PvP
    if (!mode_on) { reset_smoothing(); return 0; }

    // Head height (feet->top-of-head) for head-anchored styles. When race-fit is
    // on, derive it from the player's race; otherwise fall back to per-style config.
    bool  fit = g_cfg.fit_height_to_race;
    float head_h = fit ? plugin::fitted_head_height(g_cfg, session.race) : 0.0f;

    ImVec2 sz = ImGui::GetIO().DisplaySize;

    // Distance fade: subtle when the camera is close (solo), full when zoomed out.
    float dist = core::length(cam.position - avatar.position);
    float fade_mul = 1.0f;
    if (g_cfg.fade_enabled) {
        float t = (dist - g_cfg.fade_near) / (g_cfg.fade_far - g_cfg.fade_near);
        if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
        fade_mul = 0.30f + 0.70f * t;
    }
    unsigned rgba = faded_rgba(fade_mul);

    plugin::Outline ol;
    if (g_cfg.outline && g_cfg.outline_width > 0.0f) {
        ol.rgba = IM_COL32((int)(g_cfg.outline_color[0] * 255), (int)(g_cfg.outline_color[1] * 255),
                           (int)(g_cfg.outline_color[2] * 255), (int)(g_cfg.opacity * fade_mul * 255));
        ol.width = g_cfg.outline_width;
    }

    core::ScreenPoint feet = core::world_to_screen(avatar.position, cam, sz.x, sz.y);

    // Off-screen (behind or past the edge): draw an edge arrow toward the player.
    if (feet.behind || !feet.on_screen) {
        reset_smoothing();
        if (g_cfg.offscreen_arrow) {
            core::Vec3 right = core::normalized(core::cross(cam.up, cam.front));
            core::Vec3 to = avatar.position - cam.position;
            float dx = core::dot(to, right);
            float dy = -core::dot(to, cam.up);         // screen y grows downward
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 1e-4f) { dx = 0.0f; dy = -1.0f; } else { dx /= len; dy /= len; }
            float cxs = sz.x * 0.5f, cys = sz.y * 0.5f;
            float reach = sz.x > sz.y ? sz.x : sz.y;
            core::EdgePoint e = core::clamp_to_edge(cxs + dx * reach, cys + dy * reach,
                                                    sz.x, sz.y, g_cfg.arrow_size + 14.0f);
            plugin::draw_arrow(e.x, e.y, e.angle_rad, g_cfg.arrow_size, rgba, ol);
        }
        return 0;
    }

    float dt = ImGui::GetIO().DeltaTime;

    // Anchor to smooth: feet for most styles, the head for the standalone chevron.
    float ax = feet.x, ay = feet.y;
    if (g_cfg.style == plugin::MarkerStyle::Chevron) {
        float choff = fit ? head_h : g_cfg.head_offset;
        core::Vec3 hw = avatar.position + core::Vec3{0.0f, choff, 0.0f};
        core::ScreenPoint h = core::world_to_screen(hw, cam, sz.x, sz.y);
        if (!h.behind) { ax = h.x; ay = h.y; }
    }
    float sx = g_fx.filter(ax, dt);
    float sy = g_fy.filter(ay, dt);
    float ox = sx - feet.x, oy = sy - feet.y;   // de-jitter offset (feet-anchored styles)

    switch (g_cfg.style) {
        case plugin::MarkerStyle::GroundRing:
        case plugin::MarkerStyle::RingPip:
        case plugin::MarkerStyle::RingChevron: {
            const int N = 40;
            ImVec2 pts[N];
            if (build_ground_ring(avatar, cam, pts, N, g_cfg.ring_radius, sz.x, sz.y, ox, oy)) {
                plugin::draw_ground_ring(pts, N, rgba, 2.5f, ol);
                if (g_cfg.style == plugin::MarkerStyle::RingPip) {
                    float pip_h = fit ? 0.5f * head_h : 1.2f;
                    core::ScreenPoint hp = core::world_to_screen(
                        avatar.position + core::Vec3{0.0f, pip_h, 0.0f}, cam, sz.x, sz.y);
                    if (!hp.behind) plugin::draw_pip(hp.x + ox, hp.y + oy, 4.0f, rgba, ol);
                } else if (g_cfg.style == plugin::MarkerStyle::RingChevron) {
                    float rc_h = fit ? head_h : 2.4f;
                    core::ScreenPoint hp = core::world_to_screen(
                        avatar.position + core::Vec3{0.0f, rc_h, 0.0f}, cam, sz.x, sz.y);
                    float hx = hp.behind ? sx : hp.x + ox;
                    float hy = hp.behind ? (sy - 60.0f) : hp.y + oy;
                    plugin::draw_chevron(hx, hy, g_cfg.chevron_size, rgba, ol);
                }
            }
            break;
        }
        case plugin::MarkerStyle::SilhouetteGlow: {
            float focal = (sz.y * 0.5f) / std::tan(cam.fov_y * 0.5f);
            float d = dist < 0.5f ? 0.5f : dist;
            float body_h = fit ? head_h : g_cfg.char_height;
            float full_px = focal * body_h / d;              // side-on height
            if (full_px < 22.0f) full_px = 22.0f;
            // Collapse height as the camera looks down (front.y ~1 overhead); width
            // stays constant so the capsule flattens to a disc.
            float side = 1.0f - std::fabs(cam.front.y);
            if (side < 0.0f) side = 0.0f; if (side > 1.0f) side = 1.0f;
            float body_px = full_px * side;
            float min_h = full_px * 0.22f;
            if (body_px < min_h) body_px = min_h;
            float width_px = full_px * 0.40f * g_cfg.glow_width;
            float sh = g_fh.filter(body_px, dt);
            plugin::draw_silhouette_glow(sx, sy - sh * 0.5f, sh, width_px,
                                         rgba, g_cfg.glow_amount, ol);
            break;
        }
        case plugin::MarkerStyle::Chevron: {
            plugin::draw_chevron(sx, sy, g_cfg.chevron_size, rgba, ol);
            break;
        }
        case plugin::MarkerStyle::Beam: {
            core::ScreenPoint top = core::world_to_screen(
                avatar.position + core::Vec3{0.0f, g_cfg.beam_height, 0.0f}, cam, sz.x, sz.y);
            float top_y = top.behind ? (sy - 200.0f) : (top.y + oy);
            plugin::draw_beam(sx, sy, top_y, g_cfg.beam_width, rgba, ol);
            break;
        }
    }
    return 0;
}

static uintptr_t options_cb() { plugin::draw_options(g_cfg, g_race); return 0; }

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
    // Ensure the named MumbleLink section exists early (native Windows needs a
    // consumer to create it before GW2 will write); harmless under Proton.
    g_reader.ensure_link_object();
    return (void*)mod_init;
}

extern "C" __declspec(dllexport)
void* get_release_addr() { return (void*)mod_release; }

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) g_self = hinst;
    return TRUE;
}
