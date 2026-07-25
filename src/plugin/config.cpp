#include "config.hpp"
#include "imgui.h"
#include <cstdio>
#include <cstring>

namespace plugin {

void save_config(const Config& c, const char* path) {
    FILE* f = std::fopen(path, "w");
    if (!f) return;
    std::fprintf(f, "enabled=%d\n", c.enabled ? 1 : 0);
    std::fprintf(f, "show_in_pve=%d\nshow_in_wvw=%d\n",
                 c.show_in_pve ? 1 : 0, c.show_in_wvw ? 1 : 0);
    std::fprintf(f, "style=%d\n", (int)c.style);
    std::fprintf(f, "r=%.4f\ng=%.4f\nb=%.4f\n", c.color[0], c.color[1], c.color[2]);
    std::fprintf(f, "opacity=%.4f\n", c.opacity);
    std::fprintf(f, "outline=%d\noc_r=%.4f\noc_g=%.4f\noc_b=%.4f\noutline_width=%.4f\n",
                 c.outline ? 1 : 0, c.outline_color[0], c.outline_color[1],
                 c.outline_color[2], c.outline_width);
    std::fprintf(f, "ring_radius=%.4f\n", c.ring_radius);
    std::fprintf(f, "glow_width=%.4f\nglow_amount=%.4f\nchar_height=%.4f\n",
                 c.glow_width, c.glow_amount, c.char_height);
    std::fprintf(f, "fit_height_to_race=%d\nheight_nudge=%.4f\n",
                 c.fit_height_to_race ? 1 : 0, c.height_nudge);
    std::fprintf(f, "race_head_asura=%.4f\nrace_head_charr=%.4f\nrace_head_human=%.4f\n"
                    "race_head_norn=%.4f\nrace_head_sylvari=%.4f\n",
                 c.race_head_m[0], c.race_head_m[1], c.race_head_m[2],
                 c.race_head_m[3], c.race_head_m[4]);
    std::fprintf(f, "chevron_size=%.4f\nhead_offset=%.4f\n", c.chevron_size, c.head_offset);
    std::fprintf(f, "beam_height=%.4f\nbeam_width=%.4f\n", c.beam_height, c.beam_width);
    std::fprintf(f, "fade_enabled=%d\nfade_near=%.4f\nfade_far=%.4f\n",
                 c.fade_enabled ? 1 : 0, c.fade_near, c.fade_far);
    std::fprintf(f, "offscreen_arrow=%d\narrow_size=%.4f\n",
                 c.offscreen_arrow ? 1 : 0, c.arrow_size);
    std::fprintf(f, "hide_when_map_open=%d\nhide_when_unfocused=%d\n",
                 c.hide_when_map_open ? 1 : 0, c.hide_when_unfocused ? 1 : 0);
    std::fclose(f);
}

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Keep loaded values inside their slider ranges. Guards against garbage/stale
// ini values (e.g. an old pixel radius reused after the field became meters).
static void sanitize(Config& c) {
    if ((int)c.style < 0 || (int)c.style > 5) c.style = MarkerStyle::GroundRing;
    for (int i = 0; i < 3; ++i) c.color[i] = clampf(c.color[i], 0.0f, 1.0f);
    for (int i = 0; i < 3; ++i) c.outline_color[i] = clampf(c.outline_color[i], 0.0f, 1.0f);
    c.opacity       = clampf(c.opacity, 0.05f, 1.0f);
    c.outline_width = clampf(c.outline_width, 0.5f, 8.0f);
    // ring_radius changed units (px -> meters); a wildly out-of-range value is a
    // stale pixel radius, so reset to default rather than clamp to the max.
    if (c.ring_radius < 0.2f || c.ring_radius > 2.5f) c.ring_radius = 0.6f;
    c.glow_width   = clampf(c.glow_width, 0.4f, 2.0f);
    c.glow_amount  = clampf(c.glow_amount, 0.0f, 2.0f);
    c.char_height  = clampf(c.char_height, 0.8f, 3.2f);
    for (int i = 0; i < 5; ++i) c.race_head_m[i] = clampf(c.race_head_m[i], 0.5f, 3.5f);
    c.height_nudge = clampf(c.height_nudge, -1.0f, 1.0f);
    c.chevron_size = clampf(c.chevron_size, 8.0f, 80.0f);
    c.head_offset  = clampf(c.head_offset, 0.0f, 3.5f);
    c.beam_height  = clampf(c.beam_height, 0.5f, 6.0f);
    c.beam_width   = clampf(c.beam_width, 2.0f, 40.0f);
    c.fade_near    = clampf(c.fade_near, 0.5f, 30.0f);
    c.fade_far     = clampf(c.fade_far, 1.0f, 60.0f);
    if (c.fade_far < c.fade_near + 0.5f) c.fade_far = c.fade_near + 0.5f;
    c.arrow_size   = clampf(c.arrow_size, 8.0f, 48.0f);
}

void load_config(Config& c, const char* path) {
    FILE* f = std::fopen(path, "r");
    if (!f) return;
    char key[32];
    float v;
    while (std::fscanf(f, " %31[^=\n]=%f", key, &v) == 2) {
        if      (!std::strcmp(key, "enabled"))      c.enabled = (v != 0);
        else if (!std::strcmp(key, "show_in_pve"))  c.show_in_pve = (v != 0);
        else if (!std::strcmp(key, "show_in_wvw"))  c.show_in_wvw = (v != 0);
        else if (!std::strcmp(key, "style"))        c.style = (MarkerStyle)(int)v;
        else if (!std::strcmp(key, "r"))            c.color[0] = v;
        else if (!std::strcmp(key, "g"))            c.color[1] = v;
        else if (!std::strcmp(key, "b"))            c.color[2] = v;
        else if (!std::strcmp(key, "opacity"))      c.opacity = v;
        else if (!std::strcmp(key, "outline"))      c.outline = (v != 0);
        else if (!std::strcmp(key, "oc_r"))         c.outline_color[0] = v;
        else if (!std::strcmp(key, "oc_g"))         c.outline_color[1] = v;
        else if (!std::strcmp(key, "oc_b"))         c.outline_color[2] = v;
        else if (!std::strcmp(key, "outline_width")) c.outline_width = v;
        else if (!std::strcmp(key, "ring_radius"))  c.ring_radius = v;
        else if (!std::strcmp(key, "glow_width"))   c.glow_width = v;
        else if (!std::strcmp(key, "glow_amount"))  c.glow_amount = v;
        else if (!std::strcmp(key, "char_height"))  c.char_height = v;
        else if (!std::strcmp(key, "fit_height_to_race")) c.fit_height_to_race = (v != 0);
        else if (!std::strcmp(key, "height_nudge"))    c.height_nudge = v;
        else if (!std::strcmp(key, "race_head_asura"))   c.race_head_m[0] = v;
        else if (!std::strcmp(key, "race_head_charr"))   c.race_head_m[1] = v;
        else if (!std::strcmp(key, "race_head_human"))   c.race_head_m[2] = v;
        else if (!std::strcmp(key, "race_head_norn"))    c.race_head_m[3] = v;
        else if (!std::strcmp(key, "race_head_sylvari")) c.race_head_m[4] = v;
        else if (!std::strcmp(key, "chevron_size")) c.chevron_size = v;
        else if (!std::strcmp(key, "head_offset"))  c.head_offset = v;
        else if (!std::strcmp(key, "beam_height"))  c.beam_height = v;
        else if (!std::strcmp(key, "beam_width"))   c.beam_width = v;
        else if (!std::strcmp(key, "fade_enabled")) c.fade_enabled = (v != 0);
        else if (!std::strcmp(key, "fade_near"))    c.fade_near = v;
        else if (!std::strcmp(key, "fade_far"))     c.fade_far = v;
        else if (!std::strcmp(key, "offscreen_arrow")) c.offscreen_arrow = (v != 0);
        else if (!std::strcmp(key, "arrow_size"))   c.arrow_size = v;
        else if (!std::strcmp(key, "hide_when_map_open"))  c.hide_when_map_open = (v != 0);
        else if (!std::strcmp(key, "hide_when_unfocused")) c.hide_when_unfocused = (v != 0);
    }
    std::fclose(f);
    sanitize(c);
}

float fitted_head_height(const Config& c, core::GameRace race) {
    int idx = (race == core::GameRace::Unknown) ? 2 : (int)race;   // Unknown -> Human
    return c.race_head_m[idx] + c.height_nudge;
}

void draw_options(Config& c, core::GameRace detected) {
    ImGui::Checkbox("Show self marker", &c.enabled);

    ImGui::TextUnformatted("Game modes");
    ImGui::Checkbox("PvE", &c.show_in_pve);
    ImGui::SameLine();
    ImGui::Checkbox("WvW", &c.show_in_wvw);
    ImGui::SameLine();
    ImGui::TextDisabled("(PvP always off)");
    ImGui::Separator();

    const char* styles[] = { "Ground ring", "Silhouette glow", "Chevron (overhead)",
                             "Beam", "Ring + pip", "Ring + chevron" };
    int s = (int)c.style;
    if (ImGui::Combo("Style", &s, styles, 6)) c.style = (MarkerStyle)s;

    ImGui::ColorEdit3("Color", c.color);
    ImGui::SliderFloat("Opacity", &c.opacity, 0.05f, 1.0f, "%.2f");
    ImGui::Checkbox("Outline", &c.outline);
    if (c.outline) {
        ImGui::ColorEdit3("Outline color", c.outline_color);
        ImGui::SliderFloat("Outline width", &c.outline_width, 0.5f, 8.0f, "%.1f px");
    }

    ImGui::Separator();
    switch (c.style) {
        case MarkerStyle::GroundRing:
        case MarkerStyle::RingPip:
        case MarkerStyle::RingChevron:
            ImGui::SliderFloat("Ring size (m)", &c.ring_radius, 0.2f, 2.5f, "%.2f");
            if (c.style == MarkerStyle::RingChevron)
                ImGui::SliderFloat("Chevron size", &c.chevron_size, 8.0f, 80.0f, "%.0f px");
            break;
        case MarkerStyle::SilhouetteGlow:
            ImGui::SliderFloat("Width", &c.glow_width, 0.4f, 2.0f, "%.2f");
            ImGui::SliderFloat("Glow", &c.glow_amount, 0.0f, 2.0f, "%.2f");
            if (!c.fit_height_to_race)
                ImGui::SliderFloat("Height fit (m)", &c.char_height, 0.8f, 3.2f, "%.2f");
            break;
        case MarkerStyle::Chevron:
            ImGui::SliderFloat("Chevron size", &c.chevron_size, 8.0f, 80.0f, "%.0f px");
            if (!c.fit_height_to_race)
                ImGui::SliderFloat("Height above (m)", &c.head_offset, 0.0f, 3.5f, "%.2f");
            break;
        case MarkerStyle::Beam:
            ImGui::SliderFloat("Beam height (m)", &c.beam_height, 0.5f, 6.0f, "%.2f");
            ImGui::SliderFloat("Beam width", &c.beam_width, 2.0f, 40.0f, "%.0f px");
            break;
        default: break;
    }

    ImGui::Separator();
    ImGui::Checkbox("Fit height to race", &c.fit_height_to_race);
    if (c.fit_height_to_race) {
        ImGui::SliderFloat("Height nudge (m)", &c.height_nudge, -1.0f, 1.0f, "%+.2f");
        const char* race_names[5] = { "Asura", "Charr", "Human", "Norn", "Sylvari" };
        int det = (detected == core::GameRace::Unknown) ? -1 : (int)detected;
        ImGui::TextDisabled("Head height per race (m)");
        for (int i = 0; i < 5; ++i) {
            if (i == det)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.30f, 0.95f, 0.55f, 1.0f));
            ImGui::DragFloat(race_names[i], &c.race_head_m[i], 0.01f, 0.5f, 3.5f, "%.2f");
            if (i == det) {
                ImGui::PopStyleColor();
                ImGui::SameLine();
                ImGui::TextDisabled("(you)");
            }
        }
        if (det < 0) ImGui::TextDisabled("(race not detected -> using Human)");
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Behavior");
    ImGui::Checkbox("Fade when zoomed in", &c.fade_enabled);
    if (c.fade_enabled) {
        ImGui::SliderFloat("Fade near (m)", &c.fade_near, 0.5f, 30.0f, "%.1f");
        ImGui::SliderFloat("Fade far (m)", &c.fade_far, 1.0f, 60.0f, "%.1f");
        if (c.fade_far < c.fade_near + 0.5f) c.fade_far = c.fade_near + 0.5f;
    }
    ImGui::Checkbox("Off-screen arrow", &c.offscreen_arrow);
    if (c.offscreen_arrow)
        ImGui::SliderFloat("Arrow size", &c.arrow_size, 8.0f, 48.0f, "%.0f px");
    ImGui::Checkbox("Hide when map is open", &c.hide_when_map_open);
    ImGui::Checkbox("Hide when game unfocused", &c.hide_when_unfocused);
}

}
