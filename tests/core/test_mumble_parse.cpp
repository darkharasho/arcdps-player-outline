#include "doctest.h"
#include "mumble_data.hpp"
#include <cstring>
using namespace core;

TEST_CASE("parse fov from identity json") {
    const char* id = R"({"name":"Alt","fov":0.873,"uisz":1})";
    CHECK(parse_identity_fov(id, 1.0f) == doctest::Approx(0.873f));
}
TEST_CASE("parse fov falls back when missing/garbage") {
    CHECK(parse_identity_fov("", 1.222f) == doctest::Approx(1.222f));
    CHECK(parse_identity_fov("{\"name\":\"x\"}", 1.222f) == doctest::Approx(1.222f));
}
TEST_CASE("classify_map_type buckets modes; unknown falls back to PvE") {
    CHECK(classify_map_type(5)  == GameMode::PvE);   // open world
    CHECK(classify_map_type(4)  == GameMode::PvE);   // instance
    CHECK(classify_map_type(9)  == GameMode::WvW);   // eternal battlegrounds
    CHECK(classify_map_type(15) == GameMode::WvW);   // edge of the mists
    CHECK(classify_map_type(19) == GameMode::WvW);   // wvw lounge
    CHECK(classify_map_type(2)  == GameMode::PvP);   // heart of the mists
    CHECK(classify_map_type(6)  == GameMode::PvP);   // tournament
    CHECK(classify_map_type(0)  == GameMode::PvE);   // redirect/unknown
    CHECK(classify_map_type(999) == GameMode::PvE);  // out of range
}
TEST_CASE("read_map_type pulls mapType from context at offset 32") {
    LinkedMem m{};
    uint32_t mt = 12;   // red borderlands
    std::memcpy(m.context + 32, &mt, sizeof mt);
    CHECK(read_map_type(m) == 12u);
    CHECK(classify_map_type(read_map_type(m)) == GameMode::WvW);
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
