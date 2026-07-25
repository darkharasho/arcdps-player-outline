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
    MarkerStyle style   = MarkerStyle::GroundRing;
    float       color[3] = {0.16f, 0.92f, 0.78f};   // teal
    float       opacity  = 0.90f;                   // 0..1

    // Outline (contrasting border around drawn elements)
    bool        outline = true;
    float       outline_color[3] = {1.0f, 1.0f, 1.0f};  // white
    float       outline_width = 2.0f;                   // px

    // Ground ring / Ring + pip
    float ring_radius = 0.6f;    // world radius in meters

    // Silhouette glow
    float glow_width  = 1.0f;    // capsule width multiplier
    float glow_amount = 0.5f;    // outer-glow strength (toned down by default)
    float char_height = 1.9f;    // meters feet->head; sizes the aura

    // Height fit: derive head-anchored positions from the player's race.
    bool  fit_height_to_race = true;
    float race_head_m[5] = {1.00f, 2.20f, 1.85f, 2.55f, 1.80f}; // Asura,Charr,Human,Norn,Sylvari
    float height_nudge   = 0.0f;  // meters, added on top of the fitted height

    // Chevron
    float chevron_size = 24.0f;  // px
    float head_offset  = 2.2f;   // meters above feet

    // Beam
    float beam_height = 3.0f;    // meters
    float beam_width  = 12.0f;   // px

    // Behavior
    bool  fade_enabled = true;   // fade when the camera is zoomed in close (solo)
    float fade_near    = 3.0f;   // meters: at/under -> most faded
    float fade_far     = 14.0f;  // meters: at/over  -> full opacity
    bool  offscreen_arrow = true;// edge arrow pointing to you when off-screen
    float arrow_size   = 18.0f;  // px
};

// Fitted head height (meters, feet->top-of-head) for a race, incl. the nudge.
// Unknown race resolves to the Human entry.
float fitted_head_height(const Config& c, core::GameRace race);

void load_config(Config& c, const char* path);
void save_config(const Config& c, const char* path);
void draw_options(Config& c, core::GameRace detected);   // arcdps options_end callback body

}
