#pragma once
#include "vec3.hpp"
#include "mumble_data.hpp"
namespace core {
struct Mat4 { float m[16]{}; };   // column-major
Mat4 look_at(Vec3 eye, Vec3 front, Vec3 up);
Mat4 perspective(float fov_y, float aspect, float znear, float zfar);
struct ScreenPoint { float x{}, y{}; bool on_screen{false}; bool behind{false}; };
ScreenPoint world_to_screen(Vec3 world, const CameraState& cam,
                            float screen_w, float screen_h);
}
