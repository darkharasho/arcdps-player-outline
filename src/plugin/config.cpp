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

    ImGui::TextUnformatted("Marker elements");
    ImGui::Checkbox("Ground ring",      &c.show_ring);
    ImGui::Checkbox("Silhouette glow",  &c.show_glow);
    ImGui::Checkbox("Overhead chevron", &c.show_chevron);
    ImGui::Checkbox("Beam",             &c.show_beam);
    ImGui::Checkbox("Head pip",         &c.show_pip);

    ImGui::ColorEdit3("Color", c.color);
    ImGui::SliderFloat("Opacity", &c.opacity, 0.05f, 1.0f, "%.2f");
    ImGui::Checkbox("Outline", &c.outline);
    if (c.outline) {
        ImGui::ColorEdit3("Outline color", c.outline_color);
        ImGui::SliderFloat("Outline width", &c.outline_width, 0.5f, 8.0f, "%.1f px");
    }

    ImGui::Separator();
    if (c.show_ring)
        ImGui::SliderFloat("Ring size (m)", &c.ring_radius, 0.2f, 2.5f, "%.2f");
    if (c.show_glow) {
        ImGui::SliderFloat("Glow width",  &c.glow_width,  0.4f, 2.0f, "%.2f");
        ImGui::SliderFloat("Glow amount", &c.glow_amount, 0.0f, 2.0f, "%.2f");
        if (!c.fit_height_to_race)
            ImGui::SliderFloat("Glow height (m)", &c.char_height, 0.8f, 3.2f, "%.2f");
    }
    if (c.show_chevron)
        ImGui::SliderFloat("Chevron size", &c.chevron_size, 8.0f, 80.0f, "%.0f px");
    if (c.show_beam) {
        ImGui::SliderFloat("Beam height (m)", &c.beam_height, 0.5f, 6.0f, "%.2f");
        ImGui::SliderFloat("Beam width",      &c.beam_width,  2.0f, 40.0f, "%.0f px");
    }

    ImGui::Separator();
    ImGui::Checkbox("Fit height to race", &c.fit_height_to_race);

    const char* race_names[5] = { "Asura", "Charr", "Human", "Norn", "Sylvari" };
    int det = (detected == core::GameRace::Unknown) ? -1 : (int)detected;

    if (c.fit_height_to_race) {
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
    } else {
        ImGui::TextDisabled("Manual head-anchor height (m)");
        ImGui::SliderFloat("Pip base (m)",     &c.pip_manual_m,     0.0f, 3.5f, "%.2f");
        ImGui::SliderFloat("Chevron base (m)", &c.chevron_manual_m, 0.0f, 3.5f, "%.2f");
    }

    // Per-type adjusters — show each only when its head element is enabled.
    if (c.show_pip)
        ImGui::SliderFloat("Pip nudge (m)",     &c.pip_nudge,     -0.5f, 1.5f, "%+.2f");
    if (c.show_chevron)
        ImGui::SliderFloat("Chevron nudge (m)", &c.chevron_nudge, -0.5f, 1.5f, "%+.2f");

    // Live readout of the resulting height for the detected race (Unknown -> Human).
    if (c.show_pip)
        ImGui::TextDisabled("Pip -> %.2f m", effective_pip_height(c, detected));
    if (c.show_chevron)
        ImGui::TextDisabled("Chevron -> %.2f m", effective_chevron_height(c, detected));

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
