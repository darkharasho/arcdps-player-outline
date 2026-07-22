#include "marker.hpp"
#include "imgui.h"
namespace plugin {

void draw_ground_ring(const core::ScreenPoint& feet, float radius_px, unsigned rgba) {
    if (feet.behind) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddCircle(ImVec2(feet.x, feet.y), radius_px, rgba, 48, 3.0f);
    dl->AddCircleFilled(ImVec2(feet.x, feet.y), 3.0f, rgba, 12);
}

static unsigned with_alpha(unsigned rgba, float mul) {
    int r = rgba & 0xFF, g = (rgba >> 8) & 0xFF, b = (rgba >> 16) & 0xFF, a = (rgba >> 24) & 0xFF;
    int na = (int)(a * mul);
    if (na < 0) na = 0; if (na > 255) na = 255;
    return IM_COL32(r, g, b, na);
}

void draw_silhouette_glow(float cx, float cy, float body_px, unsigned rgba,
                          float width_scale, float glow) {
    if (body_px < 6.0f) body_px = 6.0f;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    float w      = body_px * 0.42f * width_scale;   // capsule width
    float half_w = w * 0.5f;
    float half_h = body_px * 0.5f;
    float top    = cy - half_h, bot = cy + half_h;

    // Soft outer glow: a few progressively larger, fainter rounded capsules.
    const int layers = 4;
    for (int i = layers; i >= 1; --i) {
        float pad = i * (body_px * 0.05f + 3.0f);
        float rr  = half_w + pad;
        dl->AddRectFilled(ImVec2(cx - half_w - pad, top - pad),
                          ImVec2(cx + half_w + pad, bot + pad),
                          with_alpha(rgba, 0.06f * glow), rr);
    }
    // Aura fill.
    dl->AddRectFilled(ImVec2(cx - half_w, top), ImVec2(cx + half_w, bot),
                      with_alpha(rgba, 0.24f), half_w);
    // Bright rim so the shape stays crisp against busy backgrounds.
    dl->AddRect(ImVec2(cx - half_w, top), ImVec2(cx + half_w, bot),
                with_alpha(rgba, 1.0f), half_w, 0, 2.5f);
}

void draw_chevron(float cx, float cy, float size_px, unsigned rgba) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float half = size_px * 0.5f;
    ImVec2 tip(cx, cy + half);            // points down at the character
    ImVec2 l(cx - half, cy - half);
    ImVec2 r(cx + half, cy - half);
    // soft drop-shadow/glow underlay
    dl->AddTriangleFilled(ImVec2(tip.x, tip.y + 2), ImVec2(l.x - 2, l.y - 2),
                          ImVec2(r.x + 2, r.y - 2), with_alpha(rgba, 0.25f));
    dl->AddTriangleFilled(tip, l, r, with_alpha(rgba, 1.0f));
}

}
