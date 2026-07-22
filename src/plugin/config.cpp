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
    std::fprintf(f, "glow_width=%.4f\nglow_amount=%.4f\nchar_height=%.4f\n",
                 c.glow_width, c.glow_amount, c.char_height);
    std::fprintf(f, "ring_radius=%.4f\n", c.ring_radius);
    std::fprintf(f, "chevron_size=%.4f\nhead_offset=%.4f\n", c.chevron_size, c.head_offset);
    std::fprintf(f, "rally_show=%d\nrally_tint=%d\nrally_key=%d\n",
                 c.rally_show ? 1 : 0, c.rally_tint ? 1 : 0, c.rally_key);
    std::fprintf(f, "rally_near=%.4f\nrally_far=%.4f\n", c.rally_near, c.rally_far);
    std::fclose(f);
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
        else if (!std::strcmp(key, "glow_width"))   c.glow_width = v;
        else if (!std::strcmp(key, "glow_amount"))  c.glow_amount = v;
        else if (!std::strcmp(key, "char_height"))  c.char_height = v;
        else if (!std::strcmp(key, "ring_radius"))  c.ring_radius = v;
        else if (!std::strcmp(key, "chevron_size")) c.chevron_size = v;
        else if (!std::strcmp(key, "head_offset"))  c.head_offset = v;
        else if (!std::strcmp(key, "rally_show"))   c.rally_show = (v != 0);
        else if (!std::strcmp(key, "rally_tint"))   c.rally_tint = (v != 0);
        else if (!std::strcmp(key, "rally_key"))    c.rally_key = (int)v;
        else if (!std::strcmp(key, "rally_near"))   c.rally_near = v;
        else if (!std::strcmp(key, "rally_far"))    c.rally_far = v;
    }
    std::fclose(f);
}

void draw_options(Config& c) {
    ImGui::Checkbox("Show self marker", &c.enabled);

    const char* styles[] = { "Silhouette glow", "Ground ring", "Chevron (overhead)" };
    int s = (int)c.style;
    if (ImGui::Combo("Style", &s, styles, 3)) c.style = (MarkerStyle)s;

    ImGui::ColorEdit3("Color", c.color);
    ImGui::SliderFloat("Opacity", &c.opacity, 0.05f, 1.0f, "%.2f");

    ImGui::Separator();
    switch (c.style) {
        case MarkerStyle::SilhouetteGlow:
            ImGui::SliderFloat("Width", &c.glow_width, 0.4f, 2.0f, "%.2f");
            ImGui::SliderFloat("Glow", &c.glow_amount, 0.0f, 2.5f, "%.2f");
            ImGui::SliderFloat("Height fit (m)", &c.char_height, 0.8f, 3.2f, "%.2f");
            break;
        case MarkerStyle::GroundRing:
            ImGui::SliderFloat("Ring radius", &c.ring_radius, 8.0f, 160.0f, "%.0f px");
            break;
        case MarkerStyle::Chevron:
            ImGui::SliderFloat("Chevron size", &c.chevron_size, 8.0f, 80.0f, "%.0f px");
            ImGui::SliderFloat("Height above (m)", &c.head_offset, 0.0f, 3.5f, "%.2f");
            break;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Rally point");
    ImGui::Checkbox("Show rally point + distance", &c.rally_show);
    ImGui::Checkbox("Tint marker by distance", &c.rally_tint);
    ImGui::SliderFloat("Near (green) m", &c.rally_near, 1.0f, 80.0f, "%.0f");
    ImGui::SliderFloat("Far (red) m", &c.rally_far, 5.0f, 200.0f, "%.0f");
    if (c.rally_far < c.rally_near + 1.0f) c.rally_far = c.rally_near + 1.0f;
}

}
