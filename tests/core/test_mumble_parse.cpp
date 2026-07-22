#include "doctest.h"
#include "mumble_data.hpp"
using namespace core;

TEST_CASE("parse fov from identity json") {
    const char* id = R"({"name":"Alt","fov":0.873,"uisz":1})";
    CHECK(parse_identity_fov(id, 1.0f) == doctest::Approx(0.873f));
}
TEST_CASE("parse fov falls back when missing/garbage") {
    CHECK(parse_identity_fov("", 1.222f) == doctest::Approx(1.222f));
    CHECK(parse_identity_fov("{\"name\":\"x\"}", 1.222f) == doctest::Approx(1.222f));
}
TEST_CASE("avatar invalid when tick zero / position origin") {
    LinkedMem m{};
    CHECK(read_avatar(m).valid == false);
    m.uiTick = 5; m.fAvatarPosition[0]=10; m.fAvatarPosition[1]=2; m.fAvatarPosition[2]=-3;
    AvatarState a = read_avatar(m);
    CHECK(a.valid == true);
    CHECK(a.position.x == doctest::Approx(10.0f));
    CHECK(a.position.z == doctest::Approx(-3.0f));
}
