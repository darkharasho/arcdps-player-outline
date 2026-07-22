#include "doctest.h"
#include "vec3.hpp"
using core::Vec3;

TEST_CASE("vec3 dot and cross") {
    Vec3 a{1,0,0}, b{0,1,0};
    CHECK(dot(a,b) == doctest::Approx(0.0f));
    Vec3 c = cross(a,b);
    CHECK(c.x == doctest::Approx(0.0f));
    CHECK(c.y == doctest::Approx(0.0f));
    CHECK(c.z == doctest::Approx(1.0f));
    CHECK(length(Vec3{3,4,0}) == doctest::Approx(5.0f));
    Vec3 n = normalized(Vec3{0,3,0});
    CHECK(n.y == doctest::Approx(1.0f));
}
