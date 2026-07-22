#pragma once
#include "imgui.h"
namespace plugin {

// Projected floor ring: pts are pre-projected screen-space vertices of a world
// circle on the ground plane (circle overhead, ellipse from the side).
void draw_ground_ring(const ImVec2* pts, int n, unsigned rgba, float thickness);

// Soft aura hugging the character. (cx, cy) = body center; body_px = on-screen height.
void draw_silhouette_glow(float cx, float cy, float body_px, unsigned rgba,
                          float width_scale, float glow);

// Clean downward "V" chevron centered at (cx, cy), tip pointing down at the head.
void draw_chevron(float cx, float cy, float size_px, unsigned rgba);

// Vertical light pillar from base_y up to top_y at column x=cx.
void draw_beam(float cx, float base_y, float top_y, float width_px, unsigned rgba);

// Small glowing pip (dot + halo).
void draw_pip(float cx, float cy, float r_px, unsigned rgba);

}
