#pragma once
#include "camera.hpp"
namespace plugin {

void draw_ground_ring(const core::ScreenPoint& feet, float radius_px, unsigned rgba);

// Soft aura hugging the character. (cx, cy) is the body center in screen space,
// body_px is the character's on-screen height (feet->head pixel gap). width_scale
// widens/narrows the capsule; glow controls outer-glow strength (0..1+).
void draw_silhouette_glow(float cx, float cy, float body_px, unsigned rgba,
                          float width_scale, float glow);

// Downward-pointing chevron centered at (cx, cy), tip below, drawn above head.
void draw_chevron(float cx, float cy, float size_px, unsigned rgba);

// Small diamond marking a dropped rally point at (cx, cy).
void draw_rally_marker(float cx, float cy, float size_px, unsigned rgba);

}
