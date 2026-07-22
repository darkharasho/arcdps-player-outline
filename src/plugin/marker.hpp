#pragma once
#include "imgui.h"
namespace plugin {

// Contrasting border drawn around/under a marker's strokes. width<=0 => none.
struct Outline { unsigned rgba = 0; float width = 0.0f; };

// Projected floor ring: pts are pre-projected screen-space vertices of a world
// circle on the ground plane (circle overhead, ellipse from the side).
void draw_ground_ring(const ImVec2* pts, int n, unsigned rgba, float thickness, const Outline& ol);

// Soft aura hugging the character. (cx, cy) = center; body_px/width_px = the
// capsule's on-screen height/width (independent, so it can flatten to a disc
// when viewed overhead). glow = outer-glow strength.
void draw_silhouette_glow(float cx, float cy, float body_px, float width_px,
                          unsigned rgba, float glow, const Outline& ol);

// Clean downward "V" chevron centered at (cx, cy), tip pointing down at the head.
void draw_chevron(float cx, float cy, float size_px, unsigned rgba, const Outline& ol);

// Vertical light pillar from base_y up to top_y at column x=cx.
void draw_beam(float cx, float base_y, float top_y, float width_px, unsigned rgba, const Outline& ol);

// Small glowing pip (dot + halo).
void draw_pip(float cx, float cy, float r_px, unsigned rgba, const Outline& ol);

// Edge arrow at (cx, cy) pointing along angle_rad (toward the off-screen player).
void draw_arrow(float cx, float cy, float angle_rad, float size_px, unsigned rgba, const Outline& ol);

}
