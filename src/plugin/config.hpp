#pragma once
namespace plugin {

enum class MarkerStyle { SilhouetteGlow = 0, GroundRing = 1, Chevron = 2 };

struct Config {
    bool        enabled = true;
    MarkerStyle style   = MarkerStyle::SilhouetteGlow;
    float       color[3] = {0.0f, 1.0f, 0.78f};   // teal
    float       opacity  = 0.92f;                 // 0..1

    // Silhouette glow
    float glow_width  = 1.0f;    // capsule width multiplier
    float glow_amount = 1.0f;    // outer-glow strength
    float char_height = 1.9f;    // meters feet->head; sizes the aura

    // Ground ring
    float ring_radius = 42.0f;   // px

    // Chevron
    float chevron_size = 26.0f;  // px
    float head_offset  = 2.2f;   // meters above feet

    // Rally point (a reference position you drop with a hotkey)
    bool  rally_show   = true;         // draw the rally point + distance readout
    bool  rally_tint   = true;         // tint the self marker by distance to rally
    int   rally_key    = 0x75;         // VK code, default F6
    float rally_near   = 15.0f;        // meters: at/under -> "with group" color
    float rally_far    = 60.0f;        // meters: at/over  -> "far" color
};

void load_config(Config& c, const char* path);
void save_config(const Config& c, const char* path);
void draw_options(Config& c);    // arcdps options_end callback body

}
