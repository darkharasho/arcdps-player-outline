#include "marker.hpp"
#include <cmath>

namespace plugin {

static unsigned with_alpha(unsigned rgba, float mul) {
    int r = rgba & 0xFF, g = (rgba >> 8) & 0xFF, b = (rgba >> 16) & 0xFF, a = (rgba >> 24) & 0xFF;
    int na = (int)(a * mul);
    if (na < 0) na = 0; if (na > 255) na = 255;
    return IM_COL32(r, g, b, na);
}

static bool has_outline(const Outline& ol) { return ol.width > 0.0f && (ol.rgba >> 24); }

void draw_ground_ring(const ImVec2* pts, int n, unsigned rgba, float thickness, const Outline& ol) {
    if (n < 3) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddConvexPolyFilled(pts, n, with_alpha(rgba, 0.08f));           // faint disc
    dl->AddPolyline(pts, n, with_alpha(rgba, 0.22f), ImDrawFlags_Closed, thickness + 4.0f); // glow
    if (has_outline(ol))
        dl->AddPolyline(pts, n, ol.rgba, ImDrawFlags_Closed, thickness + ol.width * 2.0f);
    dl->AddPolyline(pts, n, with_alpha(rgba, 1.0f), ImDrawFlags_Closed, thickness);         // crisp rim
}

void draw_silhouette_glow(float cx, float cy, float body_px, float width_px,
                          unsigned rgba, float glow, const Outline& ol) {
    if (body_px < 6.0f) body_px = 6.0f;
    if (width_px < 6.0f) width_px = 6.0f;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float half_w = width_px * 0.5f;
    float half_h = body_px * 0.5f;
    float top = cy - half_h, bot = cy + half_h;
    float round = half_w < half_h ? half_w : half_h;

    for (int i = 3; i >= 1; --i) {                     // soft, restrained glow
        float pad = i * (body_px * 0.035f + 2.5f);
        dl->AddRectFilled(ImVec2(cx - half_w - pad, top - pad),
                          ImVec2(cx + half_w + pad, bot + pad),
                          with_alpha(rgba, 0.045f * glow), round + pad);
    }
    dl->AddRectFilled(ImVec2(cx - half_w, top), ImVec2(cx + half_w, bot),
                      with_alpha(rgba, 0.13f), round);
    if (has_outline(ol))
        dl->AddRect(ImVec2(cx - half_w, top), ImVec2(cx + half_w, bot),
                    ol.rgba, round, 0, 2.0f + ol.width * 2.0f);
    dl->AddRect(ImVec2(cx - half_w, top), ImVec2(cx + half_w, bot),
                with_alpha(rgba, 0.9f), round, 0, 2.0f);
}

void draw_chevron(float cx, float cy, float size_px, unsigned rgba, const Outline& ol) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float w = size_px * 0.6f, h = size_px * 0.5f;
    float th = size_px * 0.18f; if (th < 3.0f) th = 3.0f;
    ImVec2 v[3] = { ImVec2(cx - w, cy - h), ImVec2(cx, cy + h), ImVec2(cx + w, cy - h) };
    dl->AddPolyline(v, 3, with_alpha(rgba, 0.22f), 0, th + 4.0f);   // soft glow
    if (has_outline(ol))
        dl->AddPolyline(v, 3, ol.rgba, 0, th + ol.width * 2.0f);
    dl->AddPolyline(v, 3, with_alpha(rgba, 1.0f), 0, th);           // crisp V
}

void draw_beam(float cx, float base_y, float top_y, float width_px, unsigned rgba, const Outline& ol) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float hw = width_px * 0.5f;
    unsigned clear  = with_alpha(rgba, 0.0f);
    unsigned body   = with_alpha(rgba, 0.45f);
    unsigned core   = with_alpha(rgba, 0.9f);
    if (has_outline(ol)) {                                   // edge rails + disc ring
        dl->AddLine(ImVec2(cx - hw, base_y), ImVec2(cx - hw, top_y), ol.rgba, ol.width);
        dl->AddLine(ImVec2(cx + hw, base_y), ImVec2(cx + hw, top_y), ol.rgba, ol.width);
        dl->AddCircle(ImVec2(cx, base_y), hw * 0.85f, ol.rgba, 24, ol.width);
    }
    dl->AddRectFilledMultiColor(ImVec2(cx - hw, top_y), ImVec2(cx + hw, base_y),
                                clear, clear, body, body);
    dl->AddRectFilledMultiColor(ImVec2(cx - hw * 0.30f, top_y), ImVec2(cx + hw * 0.30f, base_y),
                                clear, clear, core, core);
    dl->AddCircleFilled(ImVec2(cx, base_y), hw * 0.85f, with_alpha(rgba, 0.65f), 24);
}

void draw_pip(float cx, float cy, float r_px, unsigned rgba, const Outline& ol) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddCircleFilled(ImVec2(cx, cy), r_px * 2.0f, with_alpha(rgba, 0.18f), 20);  // halo
    if (has_outline(ol))
        dl->AddCircleFilled(ImVec2(cx, cy), r_px + ol.width, ol.rgba, 16);
    dl->AddCircleFilled(ImVec2(cx, cy), r_px, with_alpha(rgba, 1.0f), 16);
}

void draw_arrow(float cx, float cy, float angle_rad, float size_px, unsigned rgba, const Outline& ol) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float ca = std::cos(angle_rad), sa = std::sin(angle_rad);
    ImVec2 tip(cx + ca * size_px, cy + sa * size_px);
    float back = size_px * 0.9f, spread = size_px * 0.7f;
    ImVec2 l(cx - ca * back - sa * spread, cy - sa * back + ca * spread);
    ImVec2 r(cx - ca * back + sa * spread, cy - sa * back - ca * spread);
    dl->AddTriangleFilled(tip, l, r, with_alpha(rgba, 0.25f));           // glow underlay
    dl->AddTriangleFilled(tip, l, r, with_alpha(rgba, 0.9f));
    if (has_outline(ol))
        dl->AddTriangle(tip, l, r, ol.rgba, ol.width);                   // crisp border on top
}

}
