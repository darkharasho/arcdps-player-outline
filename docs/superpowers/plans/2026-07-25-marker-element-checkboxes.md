# Checkbox-based Marker Elements Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the single marker-style dropdown with five independent element toggles (ground ring, silhouette glow, overhead chevron, beam, head pip) that can be combined freely.

**Architecture:** Swap the `MarkerStyle style` enum field for five `show_*` bools in Config. The renderer draws each enabled element in a fixed z-order instead of switching on one style, smoothing the feet anchor once and deriving head-element positions from world→screen + the smoothed offset. The options UI becomes a checkbox block with per-element settings revealed when checked. Old `style=N` inis migrate to the equivalent element set on load.

**Tech Stack:** C++17, CMake, doctest (native tests), MinGW-w64 cross-compile (Windows DLL), Dear ImGui (options UI).

## Global Constraints

- Native tests build/run: `cmake --build build-native && ./build-native/core_tests` (build-native already configured; reconfigure with `cmake -S . -B build-native` only if needed).
- Windows DLL builds: `cmake --build build-win` (mingw toolchain; reconfigure `cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake` only if needed).
- The five element bools are exactly: `show_ring`, `show_glow`, `show_chevron`, `show_beam`, `show_pip`. Defaults: `show_ring = true`, all others `false`.
- Layering order (back to front): ground ring → silhouette glow → beam → pip → chevron.
- Any subset of elements (including none) is valid; unchecking all draws nothing. The existing `enabled` / mode / map-open / unfocused / off-screen gates are unchanged and still short-circuit first.
- The `MarkerStyle` enum stays in `config.hpp` for load-migration mapping ONLY; no runtime code branches on it.
- Migration mapping (legacy `style=N` → elements): 0 GroundRing→ring; 1 SilhouetteGlow→glow; 2 Chevron→chevron; 3 Beam→beam; 4 RingPip→ring+pip; 5 RingChevron→ring+chevron. Explicit `show_*` keys always win over `style`.
- Color / Opacity / Outline stay global (shared across all elements) — do not make them per-element.
- Per-element visual settings (ring_radius, glow_width, glow_amount, char_height, chevron_size, beam_height, beam_width) and the pip/chevron height adjusters are unchanged in meaning.

---

## File Structure

- **Modify `src/plugin/config.hpp`** — replace the `style` field with five bools; keep the `MarkerStyle` enum.
- **Modify `src/plugin/config_io.cpp`** — save the five bools; load them + migrate legacy `style`; drop the `style` sanitize check.
- **Modify `src/plugin/dllmain.cpp`** — replace the `switch (style)` render block with per-element draws and unified feet-anchor smoothing.
- **Modify `src/plugin/config.cpp`** — replace the Style combo + per-style switch in `draw_options` with checkboxes + per-element settings.
- **Modify `tests/core/test_config.cpp`** — add round-trip, migration, precedence, and default tests.

---

## Task 1: Config fields, persistence, and legacy migration

Pure config layer — natively testable. After this task the DLL build is EXPECTED to break (dllmain.cpp and config.cpp still reference `c.style`); that is fixed in Tasks 2-3. Only the native `core_tests` must pass here.

**Files:**
- Modify: `src/plugin/config.hpp:19` (field), `:28` (comment)
- Modify: `src/plugin/config_io.cpp` (save line 13; load loop + migration; sanitize line 48)
- Test: `tests/core/test_config.cpp`

**Interfaces:**
- Consumes: existing `Config`, `MarkerStyle` enum, `save_config`/`load_config`.
- Produces: `Config` fields `bool show_ring, show_glow, show_chevron, show_beam, show_pip` (defaults `show_ring=true`, rest false); the runtime `style` field is removed. Ini keys `show_ring/show_glow/show_chevron/show_beam/show_pip`. Legacy `style=N` migration on load.

- [ ] **Step 1: Replace the `style` field in `config.hpp`**

Replace this line (config.hpp:19):

```cpp
    MarkerStyle style   = MarkerStyle::GroundRing;
```

with:

```cpp
    // Marker elements — any combination may be drawn at once.
    bool        show_ring    = true;    // ground ring (default on)
    bool        show_glow    = false;   // silhouette glow
    bool        show_chevron = false;   // overhead chevron
    bool        show_beam    = false;   // vertical beam
    bool        show_pip     = false;   // head pip
```

Leave the `MarkerStyle` enum (lines 5-12) in place — it is used for load migration. Update the now-misleading comment at config.hpp:28 from `// Ground ring / Ring + pip` to `// Ground ring`.

- [ ] **Step 2: Write the failing config tests**

Add these cases to `tests/core/test_config.cpp` (the file already `#include`s `"doctest.h"`, `"config.hpp"`, `<cstdio>`, and has `using namespace plugin;`):

```cpp
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
```

- [ ] **Step 3: Run tests to confirm they fail**

Run: `cmake --build build-native && ./build-native/core_tests`
Expected: FAIL — `Config` has no `show_*` members yet (compile error), or once the struct is edited, migration/round-trip assertions fail.

- [ ] **Step 4: Update `save_config` in `config_io.cpp`**

Replace the style line (config_io.cpp:13):

```cpp
    std::fprintf(f, "style=%d\n", (int)c.style);
```

with:

```cpp
    std::fprintf(f, "show_ring=%d\nshow_glow=%d\nshow_chevron=%d\nshow_beam=%d\nshow_pip=%d\n",
                 c.show_ring ? 1 : 0, c.show_glow ? 1 : 0, c.show_chevron ? 1 : 0,
                 c.show_beam ? 1 : 0, c.show_pip ? 1 : 0);
```

- [ ] **Step 5: Update `load_config` — parse toggles, track migration state**

In `load_config`, add two locals just before the `while` loop (after `float v;`):

```cpp
    int  old_style = -1;    // legacy MarkerStyle value, if seen
    bool saw_show  = false; // any explicit show_* key present?
```

Replace the `style` parse branch (config_io.cpp:82):

```cpp
        else if (!std::strcmp(key, "style"))        c.style = (MarkerStyle)(int)v;
```

with:

```cpp
        else if (!std::strcmp(key, "style"))        old_style = (int)v;
        else if (!std::strcmp(key, "show_ring"))    { c.show_ring = (v != 0);    saw_show = true; }
        else if (!std::strcmp(key, "show_glow"))    { c.show_glow = (v != 0);    saw_show = true; }
        else if (!std::strcmp(key, "show_chevron")) { c.show_chevron = (v != 0); saw_show = true; }
        else if (!std::strcmp(key, "show_beam"))    { c.show_beam = (v != 0);    saw_show = true; }
        else if (!std::strcmp(key, "show_pip"))     { c.show_pip = (v != 0);     saw_show = true; }
```

- [ ] **Step 6: Add the migration block after the parse loop**

In `load_config`, after `std::fclose(f);` and BEFORE `sanitize(c);`, insert:

```cpp
    // Migrate a legacy single-style config only when no explicit element toggles
    // were present — explicit show_* keys always win.
    if (!saw_show && old_style >= 0) {
        c.show_ring = c.show_glow = c.show_chevron = c.show_beam = c.show_pip = false;
        switch (old_style) {
            case 0: c.show_ring = true; break;                        // GroundRing
            case 1: c.show_glow = true; break;                        // SilhouetteGlow
            case 2: c.show_chevron = true; break;                     // Chevron
            case 3: c.show_beam = true; break;                        // Beam
            case 4: c.show_ring = true; c.show_pip = true; break;     // RingPip
            case 5: c.show_ring = true; c.show_chevron = true; break; // RingChevron
            default: c.show_ring = true; break;                       // unknown -> ring
        }
    }
```

- [ ] **Step 7: Drop the `style` check from `sanitize`**

Remove this line (config_io.cpp:48):

```cpp
    if ((int)c.style < 0 || (int)c.style > 5) c.style = MarkerStyle::GroundRing;
```

(The bools need no range check.)

- [ ] **Step 8: Run tests to confirm they pass**

Run: `cmake --build build-native && ./build-native/core_tests`
Expected: PASS — all four new cases plus the prior suite (should total 33 cases).

- [ ] **Step 9: Confirm the DLL break is only the expected `style` references**

Run: `cmake --build build-win`
Expected: FAIL, and the ONLY errors reference `c.style` / `MarkerStyle::` in `dllmain.cpp` and `config.cpp`. This is intended and fixed in Tasks 2-3. Note this in your report; do not edit dllmain.cpp or config.cpp in this task.

- [ ] **Step 10: Commit**

```bash
git add src/plugin/config.hpp src/plugin/config_io.cpp tests/core/test_config.cpp
git commit -m "feat: replace marker style enum with five show_* element toggles + migration"
```

---

## Task 2: Renderer — draw each enabled element

`dllmain.cpp` is Windows/ImGui-coupled (no native test). Verification: `dllmain.cpp` itself compiles cleanly. The full DLL link still fails because `config.cpp`'s `draw_options` references `c.style` — that is Task 3's scope, NOT a defect here.

**Files:**
- Modify: `src/plugin/dllmain.cpp` — the smoothing-anchor block (currently ~133-142) and the `switch (g_cfg.style)` render block (currently ~144-197)

**Interfaces:**
- Consumes: `g_cfg.show_ring/show_glow/show_chevron/show_beam/show_pip`; existing `plugin::effective_pip_height`, `plugin::effective_chevron_height`, `plugin::fitted_head_height`, `build_ground_ring`, and the `plugin::draw_*` functions.
- Produces: no new interface.

- [ ] **Step 1: Replace the smoothing-anchor block**

Replace this block (the standalone-chevron head-smoothing special case, dllmain.cpp ~132-142):

```cpp
    // Anchor to smooth: feet for most styles, the head for the standalone chevron.
    float ax = feet.x, ay = feet.y;
    if (g_cfg.style == plugin::MarkerStyle::Chevron) {
        core::Vec3 hw = avatar.position + core::Vec3{0.0f, chevron_h, 0.0f};
        core::ScreenPoint h = core::world_to_screen(hw, cam, sz.x, sz.y);
        if (!h.behind) { ax = h.x; ay = h.y; }
    }
    float sx = g_fx.filter(ax, dt);
    float sy = g_fy.filter(ay, dt);
    float ox = sx - feet.x, oy = sy - feet.y;   // de-jitter offset (feet-anchored styles)
```

with (smooth the feet once; head elements derive from world→screen + offset):

```cpp
    // Smooth the feet anchor once; head-anchored elements derive their screen
    // position from world_to_screen + this de-jitter offset.
    float sx = g_fx.filter(feet.x, dt);
    float sy = g_fy.filter(feet.y, dt);
    float ox = sx - feet.x, oy = sy - feet.y;
```

- [ ] **Step 2: Replace the whole `switch (g_cfg.style)` block with per-element draws**

Replace the entire `switch (g_cfg.style) { ... }` block (dllmain.cpp ~144-197) with:

```cpp
    // Draw each enabled element back-to-front: ring, glow, beam, pip, chevron.
    if (g_cfg.show_ring) {
        const int N = 40;
        ImVec2 pts[N];
        if (build_ground_ring(avatar, cam, pts, N, g_cfg.ring_radius, sz.x, sz.y, ox, oy))
            plugin::draw_ground_ring(pts, N, rgba, 2.5f, ol);
    }

    if (g_cfg.show_glow) {
        float focal = (sz.y * 0.5f) / std::tan(cam.fov_y * 0.5f);
        float d = dist < 0.5f ? 0.5f : dist;
        float body_h = fit ? plugin::fitted_head_height(g_cfg, session.race)
                           : g_cfg.char_height;
        float full_px = focal * body_h / d;              // side-on height
        if (full_px < 22.0f) full_px = 22.0f;
        // Collapse height as the camera looks down (front.y ~1 overhead); width
        // stays constant so the capsule flattens to a disc.
        float side = 1.0f - std::fabs(cam.front.y);
        if (side < 0.0f) side = 0.0f; if (side > 1.0f) side = 1.0f;
        float body_px = full_px * side;
        float min_h = full_px * 0.22f;
        if (body_px < min_h) body_px = min_h;
        float width_px = full_px * 0.40f * g_cfg.glow_width;
        float sh = g_fh.filter(body_px, dt);
        plugin::draw_silhouette_glow(sx, sy - sh * 0.5f, sh, width_px,
                                     rgba, g_cfg.glow_amount, ol);
    }

    if (g_cfg.show_beam) {
        core::ScreenPoint top = core::world_to_screen(
            avatar.position + core::Vec3{0.0f, g_cfg.beam_height, 0.0f}, cam, sz.x, sz.y);
        float top_y = top.behind ? (sy - 200.0f) : (top.y + oy);
        plugin::draw_beam(sx, sy, top_y, g_cfg.beam_width, rgba, ol);
    }

    if (g_cfg.show_pip) {
        core::ScreenPoint hp = core::world_to_screen(
            avatar.position + core::Vec3{0.0f, pip_h, 0.0f}, cam, sz.x, sz.y);
        if (!hp.behind) plugin::draw_pip(hp.x + ox, hp.y + oy, 4.0f, rgba, ol);
    }

    if (g_cfg.show_chevron) {
        core::ScreenPoint hp = core::world_to_screen(
            avatar.position + core::Vec3{0.0f, chevron_h, 0.0f}, cam, sz.x, sz.y);
        float hx = hp.behind ? sx : hp.x + ox;
        float hy = hp.behind ? (sy - 60.0f) : hp.y + oy;
        plugin::draw_chevron(hx, hy, g_cfg.chevron_size, rgba, ol);
    }
```

Note: `pip_h` and `chevron_h` (computed near dllmain.cpp:88-89) and `fit` (dllmain.cpp:87) remain in use — do not remove them. No `MarkerStyle` reference may remain in `dllmain.cpp` after this edit.

- [ ] **Step 3: Confirm `dllmain.cpp` compiles in isolation**

Run: `cmake --build build-win` and inspect the errors. Expected: `dllmain.cpp` compiles with no errors/warnings; the link/build fails ONLY on `config.cpp` referencing `c.style` (Task 3 scope). If you want to confirm dllmain in isolation, building the `dllmain.cpp.obj` object target succeeds. Report the exact remaining error(s) and that they are all in config.cpp.

- [ ] **Step 4: Confirm native tests still pass (unchanged core)**

Run: `cmake --build build-native && ./build-native/core_tests`
Expected: PASS (you did not touch core/config).

- [ ] **Step 5: Commit**

```bash
git add src/plugin/dllmain.cpp
git commit -m "feat: render each enabled marker element independently in fixed z-order"
```

---

## Task 3: Options UI — element checkboxes + per-element settings

Completes the feature and restores a clean full DLL build.

**Files:**
- Modify: `src/plugin/config.cpp` — the Style combo (lines 17-20), the per-style `switch` (lines 30-53), and the nudge/readout lines (80-87)

**Interfaces:**
- Consumes: `g_cfg` element bools; `plugin::effective_pip_height`, `plugin::effective_chevron_height`.
- Produces: no new interface.

- [ ] **Step 1: Replace the Style combo with element checkboxes**

Replace these lines (config.cpp:17-20):

```cpp
    const char* styles[] = { "Ground ring", "Silhouette glow", "Chevron (overhead)",
                             "Beam", "Ring + pip", "Ring + chevron" };
    int s = (int)c.style;
    if (ImGui::Combo("Style", &s, styles, 6)) c.style = (MarkerStyle)s;
```

with:

```cpp
    ImGui::TextUnformatted("Marker elements");
    ImGui::Checkbox("Ground ring",      &c.show_ring);
    ImGui::Checkbox("Silhouette glow",  &c.show_glow);
    ImGui::Checkbox("Overhead chevron", &c.show_chevron);
    ImGui::Checkbox("Beam",             &c.show_beam);
    ImGui::Checkbox("Head pip",         &c.show_pip);
```

- [ ] **Step 2: Replace the per-style switch with per-element settings**

Replace this block (config.cpp:30-53, the `ImGui::Separator();` + `switch (c.style) { ... }`):

```cpp
    ImGui::Separator();
    switch (c.style) {
        case MarkerStyle::GroundRing:
        case MarkerStyle::RingPip:
        case MarkerStyle::RingChevron:
            ImGui::SliderFloat("Ring size (m)", &c.ring_radius, 0.2f, 2.5f, "%.2f");
            if (c.style == MarkerStyle::RingChevron)
                ImGui::SliderFloat("Chevron size", &c.chevron_size, 8.0f, 80.0f, "%.0f px");
            break;
        case MarkerStyle::SilhouetteGlow:
            ImGui::SliderFloat("Width", &c.glow_width, 0.4f, 2.0f, "%.2f");
            ImGui::SliderFloat("Glow", &c.glow_amount, 0.0f, 2.0f, "%.2f");
            if (!c.fit_height_to_race)
                ImGui::SliderFloat("Height fit (m)", &c.char_height, 0.8f, 3.2f, "%.2f");
            break;
        case MarkerStyle::Chevron:
            ImGui::SliderFloat("Chevron size", &c.chevron_size, 8.0f, 80.0f, "%.0f px");
            break;
        case MarkerStyle::Beam:
            ImGui::SliderFloat("Beam height (m)", &c.beam_height, 0.5f, 6.0f, "%.2f");
            ImGui::SliderFloat("Beam width", &c.beam_width, 2.0f, 40.0f, "%.0f px");
            break;
        default: break;
    }
```

with (each element's settings show only when checked; labels are unique to avoid ImGui ID collisions):

```cpp
    ImGui::Separator();
    if (c.show_ring)
        ImGui::SliderFloat("Ring size (m)", &c.ring_radius, 0.2f, 2.5f, "%.2f");
    if (c.show_glow) {
        ImGui::SliderFloat("Glow width",  &c.glow_width,  0.4f, 2.0f, "%.2f");
        ImGui::SliderFloat("Glow amount", &c.glow_amount, 0.0f, 2.0f, "%.2f");
        if (!c.fit_height_to_race)
            ImGui::SliderFloat("Glow height (m)", &c.char_height, 0.8f, 3.2f, "%.2f");
    }
    if (c.show_chevron)
        ImGui::SliderFloat("Chevron size", &c.chevron_size, 8.0f, 80.0f, "%.0f px");
    if (c.show_beam) {
        ImGui::SliderFloat("Beam height (m)", &c.beam_height, 0.5f, 6.0f, "%.2f");
        ImGui::SliderFloat("Beam width",      &c.beam_width,  2.0f, 40.0f, "%.0f px");
    }
```

- [ ] **Step 3: Gate the nudges and readout on the enabled head elements**

Replace this block (config.cpp:80-87):

```cpp
    // Per-type adjusters apply in both modes.
    ImGui::SliderFloat("Pip nudge (m)",     &c.pip_nudge,     -0.5f, 1.5f, "%+.2f");
    ImGui::SliderFloat("Chevron nudge (m)", &c.chevron_nudge, -0.5f, 1.5f, "%+.2f");

    // Live readout of the resulting height for the detected race (Unknown -> Human).
    ImGui::TextDisabled("Pip -> %.2f m    Chevron -> %.2f m",
                        effective_pip_height(c, detected),
                        effective_chevron_height(c, detected));
```

with:

```cpp
    // Per-type adjusters — show each only when its head element is enabled.
    if (c.show_pip)
        ImGui::SliderFloat("Pip nudge (m)",     &c.pip_nudge,     -0.5f, 1.5f, "%+.2f");
    if (c.show_chevron)
        ImGui::SliderFloat("Chevron nudge (m)", &c.chevron_nudge, -0.5f, 1.5f, "%+.2f");

    // Live readout of the resulting height for the detected race (Unknown -> Human).
    if (c.show_pip)
        ImGui::TextDisabled("Pip -> %.2f m", effective_pip_height(c, detected));
    if (c.show_chevron)
        ImGui::TextDisabled("Chevron -> %.2f m", effective_chevron_height(c, detected));
```

(The `Fit height to race` checkbox, race table, and manual `Pip base`/`Chevron base` sliders on config.cpp:55-78 are UNCHANGED — the fit setting still feeds the glow body height, so it stays visible regardless of head-element toggles.)

- [ ] **Step 4: Confirm no `MarkerStyle`/`c.style` reference remains in config.cpp**

There should be no remaining `c.style` or `MarkerStyle::` usage anywhere in `config.cpp`.

- [ ] **Step 5: Build the full DLL**

Run: `cmake --build build-win`
Expected: clean link → `build-win/arcdps_player_outline.dll`, no errors or unused-variable warnings.

- [ ] **Step 6: Confirm native tests still green**

Run: `cmake --build build-native && ./build-native/core_tests`
Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add src/plugin/config.cpp
git commit -m "feat: marker-element checkboxes with per-element settings in options UI"
```

---

## In-game verification (darkharasho, Bazzite/Proton)

After Task 3, load the DLL in GW2 and confirm:
- Each checkbox toggles exactly its element; combinations render together (ring under, chevron on top, pip/glow/beam as expected).
- Enabling multiple head elements (pip + chevron) shows both at their independent nudge heights, no jitter.
- Unchecking all elements draws nothing; re-checking restores.
- An existing config (from before this change) loads with the equivalent element(s) enabled — e.g. an old "Ring + chevron" config comes up with Ground ring + Overhead chevron checked.

---

## Self-Review Notes

- **Spec coverage:** five bools + defaults (Task 1 Step 1); save/load/migration/precedence (Task 1 Steps 4-6) with tests (Step 2); sanitize style-check dropped (Step 7); renderer per-element draws in fixed z-order with unified feet smoothing (Task 2); UI checkboxes + per-element settings + gated nudges/readout, global color/opacity/outline untouched (Task 3); enum retained for migration only (Task 1 Step 1 keeps it, no runtime branch). All covered.
- **Placeholder scan:** no TBD/TODO; every step carries concrete code.
- **Type consistency:** field names `show_ring/show_glow/show_chevron/show_beam/show_pip` identical across config.hpp, config_io.cpp, dllmain.cpp, config.cpp, and tests. Migration case numbers match the `MarkerStyle` enum values (0-5). `pip_h`/`chevron_h`/`fit` locals in dllmain.cpp are defined earlier (unchanged) and still consumed by Task 2's block.
- **Build-break sequencing:** Task 1 intentionally breaks the DLL (style refs in dllmain+config); Task 2 fixes dllmain but the link still fails on config.cpp; Task 3 restores a clean DLL. Each task's native tests stay green.
