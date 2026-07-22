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
};

void load_config(Config& c, const char* path);
void save_config(const Config& c, const char* path);
void draw_options(Config& c);    // arcdps options_end callback body

}
