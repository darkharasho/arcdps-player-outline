#include "config.hpp"
#include "imgui.h"

namespace plugin {

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
