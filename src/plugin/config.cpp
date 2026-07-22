#include "config.hpp"
#include "imgui.h"
#include <cstdio>
#include <cstring>

namespace plugin {

void save_config(const Config& c, const char* path) {
    FILE* f = std::fopen(path, "w");
    if (!f) return;
    std::fprintf(f, "enabled=%d\n", c.enabled ? 1 : 0);
    std::fprintf(f, "style=%d\n", (int)c.style);
    std::fprintf(f, "r=%.4f\ng=%.4f\nb=%.4f\n", c.color[0], c.color[1], c.color[2]);
    std::fprintf(f, "opacity=%.4f\n", c.opacity);
    std::fprintf(f, "ring_radius=%.4f\n", c.ring_radius);
    std::fprintf(f, "glow_width=%.4f\nglow_amount=%.4f\nchar_height=%.4f\n",
                 c.glow_width, c.glow_amount, c.char_height);
    std::fprintf(f, "chevron_size=%.4f\nhead_offset=%.4f\n", c.chevron_size, c.head_offset);
    std::fprintf(f, "beam_height=%.4f\nbeam_width=%.4f\n", c.beam_height, c.beam_width);
    std::fclose(f);
}

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Keep loaded values inside their slider ranges. Guards against garbage/stale
// ini values (e.g. an old pixel radius reused after the field became meters).
static void sanitize(Config& c) {
    if ((int)c.style < 0 || (int)c.style > 4) c.style = MarkerStyle::GroundRing;
    for (int i = 0; i < 3; ++i) c.color[i] = clampf(c.color[i], 0.0f, 1.0f);
    c.opacity      = clampf(c.opacity, 0.05f, 1.0f);
    c.ring_radius  = clampf(c.ring_radius, 0.2f, 2.5f);
    c.glow_width   = clampf(c.glow_width, 0.4f, 2.0f);
    c.glow_amount  = clampf(c.glow_amount, 0.0f, 2.0f);
    c.char_height  = clampf(c.char_height, 0.8f, 3.2f);
    c.chevron_size = clampf(c.chevron_size, 8.0f, 80.0f);
    c.head_offset  = clampf(c.head_offset, 0.0f, 3.5f);
    c.beam_height  = clampf(c.beam_height, 0.5f, 6.0f);
    c.beam_width   = clampf(c.beam_width, 2.0f, 40.0f);
}

void load_config(Config& c, const char* path) {
    FILE* f = std::fopen(path, "r");
    if (!f) return;
    char key[32];
    float v;
    while (std::fscanf(f, " %31[^=]=%f", key, &v) == 2) {
        if      (!std::strcmp(key, "enabled"))      c.enabled = (v != 0);
        else if (!std::strcmp(key, "style"))        c.style = (MarkerStyle)(int)v;
        else if (!std::strcmp(key, "r"))            c.color[0] = v;
        else if (!std::strcmp(key, "g"))            c.color[1] = v;
        else if (!std::strcmp(key, "b"))            c.color[2] = v;
        else if (!std::strcmp(key, "opacity"))      c.opacity = v;
        else if (!std::strcmp(key, "ring_radius"))  c.ring_radius = v;
        else if (!std::strcmp(key, "glow_width"))   c.glow_width = v;
        else if (!std::strcmp(key, "glow_amount"))  c.glow_amount = v;
        else if (!std::strcmp(key, "char_height"))  c.char_height = v;
        else if (!std::strcmp(key, "chevron_size")) c.chevron_size = v;
        else if (!std::strcmp(key, "head_offset"))  c.head_offset = v;
        else if (!std::strcmp(key, "beam_height"))  c.beam_height = v;
        else if (!std::strcmp(key, "beam_width"))   c.beam_width = v;
    }
    std::fclose(f);
    sanitize(c);
}

void draw_options(Config& c) {
    ImGui::Checkbox("Show self marker", &c.enabled);

    const char* styles[] = { "Ground ring", "Silhouette glow", "Chevron (overhead)",
                             "Beam", "Ring + pip" };
    int s = (int)c.style;
    if (ImGui::Combo("Style", &s, styles, 5)) c.style = (MarkerStyle)s;

    ImGui::ColorEdit3("Color", c.color);
    ImGui::SliderFloat("Opacity", &c.opacity, 0.05f, 1.0f, "%.2f");

    ImGui::Separator();
    switch (c.style) {
        case MarkerStyle::GroundRing:
        case MarkerStyle::RingPip:
            ImGui::SliderFloat("Ring size (m)", &c.ring_radius, 0.2f, 2.5f, "%.2f");
            break;
        case MarkerStyle::SilhouetteGlow:
            ImGui::SliderFloat("Width", &c.glow_width, 0.4f, 2.0f, "%.2f");
            ImGui::SliderFloat("Glow", &c.glow_amount, 0.0f, 2.0f, "%.2f");
            ImGui::SliderFloat("Height fit (m)", &c.char_height, 0.8f, 3.2f, "%.2f");
            break;
        case MarkerStyle::Chevron:
            ImGui::SliderFloat("Chevron size", &c.chevron_size, 8.0f, 80.0f, "%.0f px");
            ImGui::SliderFloat("Height above (m)", &c.head_offset, 0.0f, 3.5f, "%.2f");
            break;
        case MarkerStyle::Beam:
            ImGui::SliderFloat("Beam height (m)", &c.beam_height, 0.5f, 6.0f, "%.2f");
            ImGui::SliderFloat("Beam width", &c.beam_width, 2.0f, 40.0f, "%.0f px");
            break;
    }
}

}
