#include "marker.hpp"
#include "imgui.h"
namespace plugin {
void draw_ground_ring(const core::ScreenPoint& feet, float radius_px, unsigned rgba) {
    if (feet.behind) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddCircle(ImVec2(feet.x, feet.y), radius_px, rgba, 48, 3.0f);
    dl->AddCircleFilled(ImVec2(feet.x, feet.y), 3.0f, rgba, 12);
}
}
