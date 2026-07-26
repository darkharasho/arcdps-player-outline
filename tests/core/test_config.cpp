#include "doctest.h"
#include "config.hpp"
#include <cstdio>

using namespace plugin;

TEST_CASE("config save/load round-trips stable fields") {
    const char* path = "test_config_roundtrip.ini";
    Config a;
    a.enabled = false;
    a.style   = MarkerStyle::RingChevron;
    a.opacity = 0.42f;
    a.color[0] = 0.10f; a.color[1] = 0.20f; a.color[2] = 0.30f;
    save_config(a, path);

    Config b;                 // defaults, then loaded over
    load_config(b, path);
    std::remove(path);

    CHECK(b.enabled == false);
    CHECK(b.style   == MarkerStyle::RingChevron);
    CHECK(b.opacity == doctest::Approx(0.42f));
    CHECK(b.color[0] == doctest::Approx(0.10f));
    CHECK(b.color[2] == doctest::Approx(0.30f));
}
