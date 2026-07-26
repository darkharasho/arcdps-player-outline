#pragma once
#include "mumble_data.hpp"
namespace plugin {

enum class MarkerStyle {
    GroundRing     = 0,   // projected floor circle (circle overhead, ellipse side-on)
    SilhouetteGlow = 1,   // soft aura hugging the body
    Chevron        = 2,   // clean V above the head
    Beam           = 3,   // vertical light pillar
    RingPip        = 4,   // ground ring + floating pip
    RingChevron    = 5,   // ground ring + overhead chevron
};

struct Config {
    bool        enabled = true;
    // Per-mode gates (structured PvP is always off, not configurable).
    bool        show_in_pve = true;
    bool        show_in_wvw = true;
    // Marker elements — any combination may be drawn at once.
    bool        show_ring    = true;    // ground ring (default on)
    bool        show_glow    = false;   // silhouette glow
    bool        show_chevron = false;   // overhead chevron
    bool        show_beam    = false;   // vertical beam
    bool        show_pip     = false;   // head pip
    float       color[3] = {0.16f, 0.92f, 0.78f};   // teal
    float       opacity  = 0.90f;                   // 0..1

    // Outline (contrasting border around drawn elements)
    bool        outline = true;
    float       outline_color[3] = {1.0f, 1.0f, 1.0f};  // white
    float       outline_width = 2.0f;                   // px

    // Ground ring
    float ring_radius = 0.6f;    // world radius in meters

    // Silhouette glow
    float glow_width  = 1.0f;    // capsule width multiplier
    float glow_amount = 0.5f;    // outer-glow strength (toned down by default)
    float char_height = 1.9f;    // meters feet->head; sizes the aura

    // Height fit: derive head-anchored positions from the player's race.
    bool  fit_height_to_race = true;
    float race_head_m[5] = {1.05f, 2.35f, 2.05f, 2.75f, 2.00f}; // Asura,Charr,Human,Norn,Sylvari

    // Per-type height adjusters (meters above the resolved base height).
    float pip_nudge     = 0.20f;   // pip hover above head
    float chevron_nudge = 0.45f;   // chevron hover above head (floats higher)
    // Manual base heights, used only when fit_height_to_race is OFF.
    float pip_manual_m     = 2.20f;
    float chevron_manual_m = 2.20f;

    // Chevron
    float chevron_size = 24.0f;  // px

    // Beam
    float beam_height = 3.0f;    // meters
    float beam_width  = 12.0f;   // px

    // Behavior
    bool  fade_enabled = true;   // fade when the camera is zoomed in close (solo)
    float fade_near    = 3.0f;   // meters: at/under -> most faded
    float fade_far     = 14.0f;  // meters: at/over  -> full opacity
    bool  offscreen_arrow = true;// edge arrow pointing to you when off-screen
    float arrow_size   = 18.0f;  // px
    bool  hide_when_map_open  = true;  // hide while the full-screen map is open
    bool  hide_when_unfocused = true;  // hide while GW2 is alt-tabbed / unfocused
};

// Race head height (meters, feet->top-of-head). Unknown race resolves to Human.
// This is the plain race height with NO per-type nudge (used for the glow body).
float fitted_head_height(const Config& c, core::GameRace race);

// Effective world-height (meters above feet) for each head-anchored element:
// resolved base (race height when fit is on, else the manual base) plus the
// element's own nudge. Renderer and options UI share these.
float effective_pip_height(const Config& c, core::GameRace race);
float effective_chevron_height(const Config& c, core::GameRace race);

void load_config(Config& c, const char* path);
void save_config(const Config& c, const char* path);
void draw_options(Config& c, core::GameRace detected);   // arcdps options_end callback body

}
