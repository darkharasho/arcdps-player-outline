#include "doctest.h"
#include "version.hpp"
#include <string>

TEST_CASE("parse_release_tag") {
    char t[32];
    CHECK(core::parse_release_tag("https://github.com/o/r/releases/tag/v0.4.4", t, sizeof t));
    CHECK(std::string(t) == "v0.4.4");
    CHECK(core::parse_release_tag("https://github.com/o/r/releases/tag/v1.2.3/", t, sizeof t));
    CHECK(std::string(t) == "v1.2.3");
    CHECK(core::parse_release_tag("/releases/tag/v1.2.3?x=1", t, sizeof t));
    CHECK(std::string(t) == "v1.2.3");
    CHECK_FALSE(core::parse_release_tag("https://github.com/o/r/releases", t, sizeof t));
    CHECK_FALSE(core::parse_release_tag("https://github.com/o/r/releases/tag/", t, sizeof t));
    CHECK_FALSE(core::parse_release_tag(nullptr, t, sizeof t));
    char tiny[4];
    CHECK_FALSE(core::parse_release_tag("/releases/tag/v0.4.4", tiny, sizeof tiny));
}

TEST_CASE("is_newer") {
    CHECK(core::is_newer("v0.4.4", "0.4.3"));
    CHECK(core::is_newer("v0.10.0", "0.9.9"));
    CHECK(core::is_newer("v1.0.0", "0.99.99"));
    CHECK_FALSE(core::is_newer("v0.4.3", "0.4.3"));
    CHECK_FALSE(core::is_newer("v0.4.3", "0.4.3-2-gabc123"));
    CHECK(core::is_newer("v0.4.4", "0.4.3-2-gabc123"));
    CHECK_FALSE(core::is_newer("v0.4.2", "0.4.3"));
    CHECK_FALSE(core::is_newer("latest", "0.4.3"));
    CHECK_FALSE(core::is_newer("v1", "0.4.3"));
    CHECK_FALSE(core::is_newer("v0.4.4", ""));
    CHECK_FALSE(core::is_newer(nullptr, "0.4.3"));
}
