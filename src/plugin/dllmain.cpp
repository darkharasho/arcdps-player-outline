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

#ifndef PLUGIN_VERSION
#define PLUGIN_VERSION "0.0.0-dev"
#endif

// The spike build is a wholly separate addon: its own file name, arcdps sig and
// ini, so it can sit in the addons folder beside an installed release build
// without arcdps rejecting it as a duplicate or the two sharing settings.
#include "replay_tint.hpp"
#ifdef PLAYER_OUTLINE_SPIKE
#define PO_MOD_NAME "player_outline_dev"
#define PO_INI_NAME "arcdps_player_outline_dev.ini"
#define PO_SIG      0x504F4C45
#else
#define PO_MOD_NAME "player_outline"
#define PO_INI_NAME "arcdps_player_outline.ini"
#define PO_SIG      0x504F4C4E
#endif

static arcdps_exports g_arc{};
static const char* kName  = PO_MOD_NAME;
static const char* kBuild = PLUGIN_VERSION;   // injected from the git tag at build time
static plugin::MumbleReader g_reader;
static plugin::Config g_cfg;
static core::GameRace g_race = core::GameRace::Unknown;   // last sampled race (for options UI)
static core::OneEuro g_fx, g_fy, g_fh;   // smooth anchor x/y and body height (px)
static HMODULE g_self = nullptr;
static char g_ini[MAX_PATH] = {0};
static void* g_swapchain = nullptr;

static void reset_smoothing() { g_fx.reset(); g_fy.reset(); g_fh.reset(); }

static void resolve_ini_path() {
    DWORD n = GetModuleFileNameA(g_self, g_ini, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) { g_ini[0] = 0; return; }   // truncated/failed
    g_ini[n] = 0;                                            // guarantee terminated
    char* slash = std::strrchr(g_ini, '\\');
    if (!slash) slash = std::strrchr(g_ini, '/');
    char* tail = slash ? slash + 1 : g_ini;
    std::strncpy(tail, PO_INI_NAME,
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
    // The character outline runs with the marker switched off too, so the
    // marker's "enabled" gate comes after the outline block below.
    if (!not_charsel_or_loading || hide) {
        reset_smoothing();
        return 0;
    }

    core::AvatarState avatar; core::CameraState cam; plugin::SessionInfo session;
    if (!g_reader.sample(avatar, cam, session)) { reset_smoothing(); return 0; }
    g_race = session.race;   // cache for the options panel to highlight

    // Character outline. Runs on the render thread, same as the draw hooks, so
    // plain assignments are enough to hand it this frame's camera and avatar.
    {
        ImVec2 ssz = ImGui::GetIO().DisplaySize;
#ifndef PLAYER_OUTLINE_SPIKE
        // Settings come from the options panel. Like the marker, off in PvP.
        const bool pvp = session.mode != core::GameMode::WvW && session.mode != core::GameMode::PvE;
        *spike::outline_flag() = g_cfg.body_outline && !pvp;
        *spike::thickness() = g_cfg.body_thickness;
        *spike::fill() = g_cfg.body_fill;
        for (int i = 0; i < 3; ++i) spike::outline_rgb()[i] = g_cfg.body_color[i];
#endif
        spike::set_mounted(session.mounted);
        spike::set_frame(avatar.position, cam, ssz.x, ssz.y);
    }
#ifdef PLAYER_OUTLINE_SPIKE
    {
        // Mouse-driven controls: the probe does no keyboard polling at all.
        ImGui::SetNextWindowPos(ImVec2(12.0f, 36.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("player outline probe 3r", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing)) {
            ImGui::Checkbox("Outline", spike::outline_flag());
            ImGui::ColorEdit3("Colour", spike::outline_rgb(), ImGuiColorEditFlags_NoInputs);
            ImGui::SetNextItemWidth(180.0f);
            ImGui::SliderFloat("Thickness (px)", spike::thickness(), 1.0f, 8.0f, "%.0f", ImGuiSliderFlags_NoInput);
            ImGui::SetNextItemWidth(180.0f);
            ImGui::SliderFloat("Fill", spike::fill(), 0.0f, 0.6f, "%.2f", ImGuiSliderFlags_NoInput);
            ImGui::SetNextItemWidth(180.0f);
            ImGui::SliderFloat("Radius (m)", spike::radius(), 0.15f, 1.5f, "%.2f", ImGuiSliderFlags_NoInput);
            ImGui::SetNextItemWidth(180.0f);
            ImGui::SliderFloat("Height (m)", spike::height(), 0.5f, 4.0f, "%.2f", ImGuiSliderFlags_NoInput);
            ImGui::SetNextItemWidth(180.0f);
            ImGui::SliderFloat("Mask margin (m)", spike::margin(), 0.0f, 3.0f, "%.1f", ImGuiSliderFlags_NoInput);
            ImGui::SetNextItemWidth(180.0f);
            ImGui::SliderFloat("Learn at % inside", spike::learn_pct(), 20.0f, 100.0f, "%.0f", ImGuiSliderFlags_NoInput);
            ImGui::Checkbox("Use model position", spike::use_transforms());
            ImGui::Checkbox("Body and armour only", spike::body_only());
            ImGui::SetNextItemWidth(180.0f);
            ImGui::SliderFloat("Model distance (m)", spike::transform_radius(), 0.1f, 2.0f, "%.2f", ImGuiSliderFlags_NoInput);
            ImGui::Checkbox("Include rigid pieces", spike::rigid_pieces());
            ImGui::Checkbox("Lock to my gear", spike::gear_lock());
            ImGui::Checkbox("Nearest copy only", spike::nearest_only());
            ImGui::SameLine(); if (ImGui::Button("Forget gear")) spike::forget_gear();
            ImGui::SetNextItemWidth(180.0f);
            ImGui::SliderFloat("Known gear distance (m)", spike::gear_radius(), 0.5f, 6.0f, "%.1f", ImGuiSliderFlags_NoInput);
            ImGui::SetNextItemWidth(180.0f);
            ImGui::SliderFloat("Learn when alone within (m)", spike::learn_range(), 1.0f, 8.0f, "%.1f", ImGuiSliderFlags_NoInput);
            if (ImGui::Button("Log draws")) spike::log_learned();
            ImGui::SameLine(); ImGui::Text("logged %d", spike::detail_count());
            ImGui::TextUnformatted(spike::dump_status());
            ImGui::TextUnformatted(spike::last_summary());
        }
        ImGui::End();
    }
#endif
    if (!g_cfg.enabled) { reset_smoothing(); return 0; }

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

    // Head-anchored heights (feet->element, meters). Pip and chevron each resolve
    // their own base (race height when fit is on, else a manual base) + own nudge.
    bool  fit = g_cfg.fit_height_to_race;
    float pip_h     = plugin::effective_pip_height(g_cfg, session.race);
    float chevron_h = plugin::effective_chevron_height(g_cfg, session.race);

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

    // Smooth the feet anchor once; head-anchored elements derive their screen
    // position from world_to_screen + this de-jitter offset.
    float sx = g_fx.filter(feet.x, dt);
    float sy = g_fy.filter(feet.y, dt);
    float ox = sx - feet.x, oy = sy - feet.y;

    // Draw each enabled element back-to-front: ring, glow, beam, pip, chevron.
    if (g_cfg.show_ring) {
        const int N = 40;
        ImVec2 pts[N];
        if (build_ground_ring(avatar, cam, pts, N, g_cfg.ring_radius, sz.x, sz.y, ox, oy))
            plugin::draw_ground_ring(pts, N, rgba, 2.5f, ol);
    }

    if (g_cfg.show_glow) {
        float focal = (sz.y * 0.5f) / std::tan(cam.fov_y * 0.5f);
        float d = dist < 0.5f ? 0.5f : dist;
        float body_h = fit ? plugin::fitted_head_height(g_cfg, session.race)
                           : g_cfg.char_height;
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
    }

    if (g_cfg.show_beam) {
        core::ScreenPoint top = core::world_to_screen(
            avatar.position + core::Vec3{0.0f, g_cfg.beam_height, 0.0f}, cam, sz.x, sz.y);
        float top_y = top.behind ? (sy - 200.0f) : (top.y + oy);
        plugin::draw_beam(sx, sy, top_y, g_cfg.beam_width, rgba, ol);
    }

    if (g_cfg.show_pip) {
        core::ScreenPoint hp = core::world_to_screen(
            avatar.position + core::Vec3{0.0f, pip_h, 0.0f}, cam, sz.x, sz.y);
        if (!hp.behind) plugin::draw_pip(hp.x + ox, hp.y + oy, 4.0f, rgba, ol);
    }

    if (g_cfg.show_chevron) {
        core::ScreenPoint hp = core::world_to_screen(
            avatar.position + core::Vec3{0.0f, chevron_h, 0.0f}, cam, sz.x, sz.y);
        float hx = hp.behind ? sx : hp.x + ox;
        float hy = hp.behind ? (sy - 60.0f) : hp.y + oy;
        plugin::draw_chevron(hx, hy, g_cfg.chevron_size, rgba, ol);
    }
    return 0;
}

static uintptr_t options_cb() {
    plugin::draw_options(g_cfg, g_race);
    ImGui::Separator();
    ImGui::Checkbox("Character outline (experimental)", &g_cfg.body_outline);
    if (g_cfg.body_outline) {
        ImGui::ColorEdit3("Character outline color", g_cfg.body_color);
        ImGui::SliderFloat("Character outline thickness", &g_cfg.body_thickness, 1.0f, 8.0f, "%.0f px");
        ImGui::SliderFloat("Character fill", &g_cfg.body_fill, 0.0f, 0.6f, "%.2f");
        if (ImGui::Button("Re-learn my gear")) spike::forget_gear();
        ImGui::TextDisabled("Learns your gear while no other player is within a few metres.");
    }
    return 0;
}

static arcdps_exports* mod_init() {
    resolve_ini_path();
    if (g_ini[0]) plugin::load_config(g_cfg, g_ini);
#ifndef PLAYER_OUTLINE_SPIKE
    spike::install(g_swapchain, nullptr);   // no log file in release builds
#else
    {
        char log[MAX_PATH];
        std::snprintf(log, MAX_PATH, "%s", g_ini);
        char* dot = std::strrchr(log, '.');
        if (dot) std::strncpy(dot, "_scan.log", (size_t)(log + MAX_PATH - dot - 1));
        spike::install(g_swapchain, log);
    }
#endif
    g_arc.size = sizeof(arcdps_exports);
    g_arc.sig = PO_SIG;
    g_arc.imguivers = IMGUI_VERSION_NUM;
    g_arc.out_name = kName;
    g_arc.out_build = kBuild;
    g_arc.imgui = (void*)imgui_cb;
    g_arc.options_end = (void*)options_cb;
    return &g_arc;
}
static uintptr_t mod_release() {
    spike::shutdown();
    if (g_ini[0]) plugin::save_config(g_cfg, g_ini);
    return 0;
}

extern "C" __declspec(dllexport)
void* get_init_addr(char* /*arcversion*/, void* imguictx, void* id3dptr,
                    HMODULE /*arcdll*/, void* mallocfn, void* freefn,
                    uint32_t /*d3dversion*/) {
    g_swapchain = id3dptr;
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
