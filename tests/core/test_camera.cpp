#include "doctest.h"
#include "camera.hpp"
using namespace core;

// Transform a world point by a Mat4 (w=1), return view-space Vec3 + w.
static Vec3 xf(const Mat4& M, Vec3 p, float& w) {
    w = M.m[3]*p.x + M.m[7]*p.y + M.m[11]*p.z + M.m[15];
    return {
        M.m[0]*p.x + M.m[4]*p.y + M.m[8]*p.z + M.m[12],
        M.m[1]*p.x + M.m[5]*p.y + M.m[9]*p.z + M.m[13],
        M.m[2]*p.x + M.m[6]*p.y + M.m[10]*p.z + M.m[14],
    };
}

TEST_CASE("look_at puts eye at view origin") {
    Mat4 V = look_at({5,1,5}, {0,0,1}, {0,1,0});
    float w; Vec3 v = xf(V, {5,1,5}, w);
    CHECK(v.x == doctest::Approx(0).epsilon(0.01));
    CHECK(v.y == doctest::Approx(0).epsilon(0.01));
    CHECK(v.z == doctest::Approx(0).epsilon(0.01));
}
TEST_CASE("point in front has positive forward depth") {
    Mat4 V = look_at({0,0,0}, {0,0,1}, {0,1,0});
    float w; Vec3 v = xf(V, {0,0,10}, w);
    CHECK(v.z > 0.0f);            // forward axis positive
    CHECK(v.x == doctest::Approx(0).epsilon(0.01));
}
TEST_CASE("point straight ahead projects to screen center") {
    CameraState cam; cam.position={0,0,0}; cam.front={0,0,1}; cam.fov_y=1.0f;
    ScreenPoint s = world_to_screen({0,0,10}, cam, 1920, 1080);
    CHECK_FALSE(s.behind);
    CHECK(s.on_screen);
    CHECK(s.x == doctest::Approx(960).epsilon(0.02));
    CHECK(s.y == doctest::Approx(540).epsilon(0.02));
}
TEST_CASE("point to the right lands in right half") {
    CameraState cam; cam.position={0,0,0}; cam.front={0,0,1}; cam.fov_y=1.0f;
    ScreenPoint s = world_to_screen({3,0,10}, cam, 1920, 1080);
    CHECK_FALSE(s.behind);
    CHECK(s.x > 960.0f);
}
TEST_CASE("point behind camera is flagged behind") {
    CameraState cam; cam.position={0,0,0}; cam.front={0,0,1}; cam.fov_y=1.0f;
    ScreenPoint s = world_to_screen({0,0,-10}, cam, 1920, 1080);
    CHECK(s.behind);
}
