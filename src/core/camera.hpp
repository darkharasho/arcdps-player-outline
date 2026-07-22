#pragma once
#include "vec3.hpp"
namespace core {
struct Mat4 { float m[16]{}; };   // column-major
Mat4 look_at(Vec3 eye, Vec3 front, Vec3 up);
}
