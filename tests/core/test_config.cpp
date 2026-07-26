#include "doctest.h"
#include "config.hpp"
#include <cstdio>

using namespace plugin;

TEST_CASE("config save/load round-trips stable fields") {
    const char* path = "test_config_roundtrip.ini";
    Config a;
    a.enabled = false;
    a.show_ring = false; a.show_chevron = true;
    a.opacity = 0.42f;
    a.color[0] = 0.10f; a.color[1] = 0.20f; a.color[2] = 0.30f;
    save_config(a, path);

    Config b;                 // defaults, then loaded over
    load_config(b, path);
    std::remove(path);

    CHECK(b.enabled == false);
    CHECK(b.show_ring == false);
    CHECK(b.show_chevron == true);
    CHECK(b.opacity == doctest::Approx(0.42f));
    CHECK(b.color[0] == doctest::Approx(0.10f));
    CHECK(b.color[2] == doctest::Approx(0.30f));
}

TEST_CASE("effective heights add per-type nudge to race base when fit is on") {
    Config c;                        // defaults; fit_height_to_race = true
    c.fit_height_to_race = true;
    c.pip_nudge = 0.20f;
    c.chevron_nudge = 0.45f;
    // Human index 2 default = 2.05
    CHECK(effective_pip_height(c, core::GameRace::Human)
          == doctest::Approx(2.05f + 0.20f));
    CHECK(effective_chevron_height(c, core::GameRace::Human)
          == doctest::Approx(2.05f + 0.45f));
    // Unknown resolves to Human.
    CHECK(effective_chevron_height(c, core::GameRace::Unknown)
          == doctest::Approx(2.05f + 0.45f));
    // Norn index 3 default = 2.75
    CHECK(effective_pip_height(c, core::GameRace::Norn)
          == doctest::Approx(2.75f + 0.20f));
}

TEST_CASE("effective heights use the manual base when fit is off") {
    Config c;
    c.fit_height_to_race = false;
    c.pip_manual_m = 1.80f;   c.pip_nudge = 0.20f;
    c.chevron_manual_m = 2.40f; c.chevron_nudge = 0.45f;
    CHECK(effective_pip_height(c, core::GameRace::Norn)
          == doctest::Approx(1.80f + 0.20f));      // race ignored when fit off
    CHECK(effective_chevron_height(c, core::GameRace::Human)
          == doctest::Approx(2.40f + 0.45f));
}

TEST_CASE("new per-type fields round-trip through save/load") {
    const char* path = "test_config_pertype.ini";
    Config a;
    a.pip_nudge = 0.33f; a.chevron_nudge = 0.66f;
    a.pip_manual_m = 1.55f; a.chevron_manual_m = 2.66f;
    a.race_head_m[3] = 2.90f;   // Norn
    save_config(a, path);
    Config b;
    load_config(b, path);
    std::remove(path);
    CHECK(b.pip_nudge == doctest::Approx(0.33f));
    CHECK(b.chevron_nudge == doctest::Approx(0.66f));
    CHECK(b.pip_manual_m == doctest::Approx(1.55f));
    CHECK(b.chevron_manual_m == doctest::Approx(2.66f));
    CHECK(b.race_head_m[3] == doctest::Approx(2.90f));
}

TEST_CASE("load_config ignores stale height_nudge/head_offset keys without crashing") {
    const char* path = "test_config_stalekeys.ini";
    std::FILE* f = std::fopen(path, "w");
    REQUIRE(f != nullptr);
    std::fprintf(f, "enabled=1\n");
    std::fprintf(f, "height_nudge=0.5\n");
    std::fprintf(f, "head_offset=2.1\n");
    std::fprintf(f, "pip_nudge=0.30\n");
    std::fprintf(f, "chevron_manual_m=2.40\n");
    std::fclose(f);

    Config c;
    load_config(c, path);
    std::remove(path);

    CHECK(c.enabled == true);
    CHECK(c.pip_nudge == doctest::Approx(0.30f));
    CHECK(c.chevron_manual_m == doctest::Approx(2.40f));
}

TEST_CASE("show_* element toggles round-trip") {
    const char* path = "test_config_elements.ini";
    Config a;
    a.show_ring = false; a.show_glow = true; a.show_chevron = true;
    a.show_beam = false; a.show_pip = true;
    save_config(a, path);
    Config b;
    load_config(b, path);
    std::remove(path);
    CHECK(b.show_ring == false);
    CHECK(b.show_glow == true);
    CHECK(b.show_chevron == true);
    CHECK(b.show_beam == false);
    CHECK(b.show_pip == true);
}

TEST_CASE("legacy style migrates to element set when no show_* keys present") {
    const char* path = "test_config_migrate.ini";
    FILE* f = std::fopen(path, "w");
    std::fprintf(f, "enabled=1\nstyle=4\n");   // 4 = RingPip -> ring + pip
    std::fclose(f);
    Config c;
    load_config(c, path);
    std::remove(path);
    CHECK(c.show_ring == true);
    CHECK(c.show_pip == true);
    CHECK(c.show_glow == false);
    CHECK(c.show_chevron == false);
    CHECK(c.show_beam == false);
}

TEST_CASE("explicit show_* keys win over a legacy style key") {
    const char* path = "test_config_precedence.ini";
    FILE* f = std::fopen(path, "w");
    // style=1 (glow) would migrate to glow-only, but explicit show_* must win.
    std::fprintf(f, "style=1\nshow_ring=1\nshow_glow=0\nshow_beam=1\n");
    std::fclose(f);
    Config c;
    load_config(c, path);
    std::remove(path);
    CHECK(c.show_ring == true);
    CHECK(c.show_glow == false);   // NOT migrated on from style=1
    CHECK(c.show_beam == true);
}

TEST_CASE("empty ini leaves default element set (ring on)") {
    const char* path = "test_config_empty.ini";
    FILE* f = std::fopen(path, "w");
    std::fclose(f);   // empty file
    Config c;
    load_config(c, path);
    std::remove(path);
    CHECK(c.show_ring == true);
    CHECK(c.show_glow == false);
    CHECK(c.show_pip == false);
}
