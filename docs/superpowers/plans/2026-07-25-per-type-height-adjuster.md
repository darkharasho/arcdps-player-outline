# Per-type Height Adjuster Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the self-marker independent pip and chevron height adjusters that hover clearly above the head by default, with a live effective-height readout.

**Architecture:** Split the pure config IO/math out of the ImGui-coupled `config.cpp` into a new `config_io.cpp` so it can be unit-tested natively. Replace the single global `height_nudge` with two per-type nudges (`pip_nudge`, `chevron_nudge`), add per-type manual bases, raise the race table, and route both the renderer and the options UI through two shared helpers (`effective_pip_height`, `effective_chevron_height`).

**Tech Stack:** C++17, CMake, doctest (native tests), MinGW-w64 cross-compile (Windows DLL), Dear ImGui (options UI).

## Global Constraints

- Native tests build/run with: `cmake -S . -B build-native && cmake --build build-native && ./build-native/core_tests`.
- Windows DLL builds with: `cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake && cmake --build build-win`.
- `config_io.cpp` MUST NOT include `imgui.h` or any Windows header — it has to compile into the native `core_tests` target. Only `draw_options` (ImGui) stays in `config.cpp`.
- The chevron nudge is shared by BOTH `MarkerStyle::Chevron` and `MarkerStyle::RingChevron`. The pip nudge drives only `MarkerStyle::RingPip`.
- `char_height` (silhouette-glow body height) is out of scope — do not change its behavior.
- Race index mapping is unchanged: `Unknown -> Human (index 2)`; order is `{Asura, Charr, Human, Norn, Sylvari}`.

---

## File Structure

- **Create `src/plugin/config_io.cpp`** — `save_config`, `load_config`, `sanitize`, `clampf`, `fitted_head_height`, and the two new `effective_*` helpers. No ImGui. One responsibility: config persistence + derived heights.
- **Modify `src/plugin/config.cpp`** — keep only `draw_options` (ImGui). Remove the IO functions moved to `config_io.cpp`.
- **Modify `src/plugin/config.hpp`** — swap config fields; declare the two `effective_*` helpers.
- **Modify `src/plugin/dllmain.cpp`** — route pip/chevron placement through the helpers.
- **Create `tests/core/test_config.cpp`** — native round-trip + effective-height + sanitize tests.
- **Modify `CMakeLists.txt`** — add `config_io.cpp` to the Windows lib; add `config_io.cpp` + `test_config.cpp` and the `src/plugin` include dir to `core_tests`.

---

## Task 1: Split config IO into a natively-testable unit

Pure refactor — no behavior change. Establishes the native test harness for config.

**Files:**
- Create: `src/plugin/config_io.cpp`
- Modify: `src/plugin/config.cpp` (remove IO functions, keep `draw_options`)
- Modify: `CMakeLists.txt:26-33` (Windows lib sources), `CMakeLists.txt:44-53` (`core_tests` sources + includes)
- Test: `tests/core/test_config.cpp`

**Interfaces:**
- Consumes: existing `config.hpp` declarations (`Config`, `save_config`, `load_config`, `fitted_head_height`).
- Produces: `config_io.cpp` compiling without ImGui; `core_tests` linking it.

- [ ] **Step 1: Create `src/plugin/config_io.cpp` by moving the non-UI functions out of `config.cpp`**

Move `save_config`, `clampf`, `sanitize`, `load_config`, and `fitted_head_height` verbatim from `config.cpp` into a new file. Header block:

```cpp
#include "config.hpp"
#include <cstdio>
#include <cstring>

namespace plugin {

// ... moved: save_config, clampf, sanitize, load_config, fitted_head_height ...

}
```

- [ ] **Step 2: Trim `config.cpp` to the UI only**

`config.cpp` keeps its `#include "config.hpp"` and `#include "imgui.h"`, plus `#include <cstdio>`/`<cstring>` only if still referenced. It should now contain ONLY `draw_options` inside `namespace plugin {}`.

- [ ] **Step 3: Wire CMake — add `config_io.cpp` to the Windows lib**

In `CMakeLists.txt`, the `add_library(arcdps_player_outline SHARED ...)` source list (currently lines 26-33): add `src/plugin/config_io.cpp` next to `src/plugin/config.cpp`.

- [ ] **Step 4: Wire CMake — add config to `core_tests`**

In the `add_executable(core_tests ...)` block (currently lines 44-51) add two sources:

```cmake
        tests/core/test_config.cpp
        src/plugin/config_io.cpp
```

And extend its include dirs (currently `third_party/doctest src/core`) to also list `src/plugin`:

```cmake
    target_include_directories(core_tests PRIVATE
        third_party/doctest src/core src/plugin)
```

- [ ] **Step 5: Write the baseline round-trip test (stable fields only)**

Create `tests/core/test_config.cpp`. These fields do not change in later tasks, so this test stays valid throughout:

```cpp
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
```

- [ ] **Step 6: Build and run native tests**

Run: `cmake -S . -B build-native && cmake --build build-native && ./build-native/core_tests`
Expected: PASS, including the new `config save/load round-trips stable fields` case.

- [ ] **Step 7: Confirm the DLL still builds after the split**

Run: `cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake && cmake --build build-win`
Expected: `build-win/arcdps_player_outline.dll` builds with no errors.

- [ ] **Step 8: Commit**

```bash
git add src/plugin/config_io.cpp src/plugin/config.cpp CMakeLists.txt tests/core/test_config.cpp
git commit -m "refactor: split config IO from UI into config_io.cpp; add native config test"
```

---

## Task 2: Per-type config fields, raised defaults, and effective-height helpers

**Files:**
- Modify: `src/plugin/config.hpp:34-44` (fields), and the helper declaration (`config.hpp:59-61`)
- Modify: `src/plugin/config_io.cpp` (save/load/sanitize keys; add helpers)
- Test: `tests/core/test_config.cpp`

**Interfaces:**
- Consumes: `Config`, `core::GameRace` from `config.hpp`.
- Produces:
  - Fields: `float pip_nudge`, `float chevron_nudge`, `float pip_manual_m`, `float chevron_manual_m`; removed `height_nudge`, `head_offset`.
  - `float effective_pip_height(const Config& c, core::GameRace race)` — returns `(fit ? race_head_m[idx] : pip_manual_m) + pip_nudge`.
  - `float effective_chevron_height(const Config& c, core::GameRace race)` — returns `(fit ? race_head_m[idx] : chevron_manual_m) + chevron_nudge`.
  - `fitted_head_height(c, race)` now returns `race_head_m[idx]` with NO nudge (nudge removed).

- [ ] **Step 1: Update `config.hpp` fields**

Replace the height-fit and chevron blocks. In the height-fit block, remove `height_nudge` and add the four new fields; raise the race defaults:

```cpp
    // Height fit: derive head-anchored positions from the player's race.
    bool  fit_height_to_race = true;
    float race_head_m[5] = {1.05f, 2.35f, 2.05f, 2.75f, 2.00f}; // Asura,Charr,Human,Norn,Sylvari

    // Per-type height adjusters (meters above the resolved base height).
    float pip_nudge     = 0.20f;   // pip hover above head
    float chevron_nudge = 0.45f;   // chevron hover above head (floats higher)
    // Manual base heights, used only when fit_height_to_race is OFF.
    float pip_manual_m     = 2.20f;
    float chevron_manual_m = 2.20f;
```

In the Chevron block (lines 41-43), remove `head_offset` (its role moves to `chevron_manual_m`). Keep `chevron_size`:

```cpp
    // Chevron
    float chevron_size = 24.0f;  // px
```

- [ ] **Step 2: Update the helper declarations in `config.hpp`**

Replace the `fitted_head_height` declaration block (lines 59-61) with:

```cpp
// Race head height (meters, feet->top-of-head). Unknown race resolves to Human.
// This is the plain race height with NO per-type nudge (used for the glow body).
float fitted_head_height(const Config& c, core::GameRace race);

// Effective world-height (meters above feet) for each head-anchored element:
// resolved base (race height when fit is on, else the manual base) plus the
// element's own nudge. Renderer and options UI share these.
float effective_pip_height(const Config& c, core::GameRace race);
float effective_chevron_height(const Config& c, core::GameRace race);
```

- [ ] **Step 3: Write failing tests for the helpers**

Add to `tests/core/test_config.cpp`:

```cpp
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
```

- [ ] **Step 4: Run the tests to confirm they fail**

Run: `cmake --build build-native && ./build-native/core_tests`
Expected: FAIL — `effective_pip_height`/`effective_chevron_height` undefined and new fields not persisted.

- [ ] **Step 5: Implement the helpers and update `fitted_head_height` in `config_io.cpp`**

Replace the existing `fitted_head_height` definition with these three functions:

```cpp
static int race_index(core::GameRace race) {
    return (race == core::GameRace::Unknown) ? 2 : (int)race;   // Unknown -> Human
}

float fitted_head_height(const Config& c, core::GameRace race) {
    return c.race_head_m[race_index(race)];   // plain race height, no nudge
}

float effective_pip_height(const Config& c, core::GameRace race) {
    float base = c.fit_height_to_race ? c.race_head_m[race_index(race)] : c.pip_manual_m;
    return base + c.pip_nudge;
}

float effective_chevron_height(const Config& c, core::GameRace race) {
    float base = c.fit_height_to_race ? c.race_head_m[race_index(race)] : c.chevron_manual_m;
    return base + c.chevron_nudge;
}
```

- [ ] **Step 6: Update `save_config` keys in `config_io.cpp`**

Replace the line that writes `fit_height_to_race`/`height_nudge` and the `chevron_size`/`head_offset` line:

```cpp
    std::fprintf(f, "fit_height_to_race=%d\n", c.fit_height_to_race ? 1 : 0);
    std::fprintf(f, "pip_nudge=%.4f\nchevron_nudge=%.4f\n", c.pip_nudge, c.chevron_nudge);
    std::fprintf(f, "pip_manual_m=%.4f\nchevron_manual_m=%.4f\n",
                 c.pip_manual_m, c.chevron_manual_m);
```

And change the chevron line to drop `head_offset`:

```cpp
    std::fprintf(f, "chevron_size=%.4f\n", c.chevron_size);
```

(The `race_head_*` write block is unchanged.)

- [ ] **Step 7: Update `load_config` keys in `config_io.cpp`**

Remove the `height_nudge` and `head_offset` parse branches; add four new ones:

```cpp
        else if (!std::strcmp(key, "pip_nudge"))        c.pip_nudge = v;
        else if (!std::strcmp(key, "chevron_nudge"))    c.chevron_nudge = v;
        else if (!std::strcmp(key, "pip_manual_m"))     c.pip_manual_m = v;
        else if (!std::strcmp(key, "chevron_manual_m")) c.chevron_manual_m = v;
```

Leave `chevron_size` parsing as-is. Unknown keys (e.g. a stale `height_nudge`/`head_offset` in an old ini) are already ignored by the `%f`-based parser.

- [ ] **Step 8: Update `sanitize` in `config_io.cpp`**

Replace the `height_nudge` and `head_offset` clamp lines:

```cpp
    c.pip_nudge     = clampf(c.pip_nudge,     -0.5f, 1.5f);
    c.chevron_nudge = clampf(c.chevron_nudge, -0.5f, 1.5f);
    c.pip_manual_m     = clampf(c.pip_manual_m,     0.0f, 3.5f);
    c.chevron_manual_m = clampf(c.chevron_manual_m, 0.0f, 3.5f);
```

(Keep the existing `race_head_m[i]` clamp of `0.5..3.5`.)

- [ ] **Step 9: Run the tests to confirm they pass**

Run: `cmake --build build-native && ./build-native/core_tests`
Expected: PASS — all three new cases plus the Task 1 baseline.

- [ ] **Step 10: Commit**

```bash
git add src/plugin/config.hpp src/plugin/config_io.cpp tests/core/test_config.cpp
git commit -m "feat: per-type pip/chevron height nudges + manual bases, raised race defaults"
```

---

## Task 3: Route the renderer through the effective-height helpers

`config.cpp`/`dllmain.cpp` are Windows/ImGui-coupled, so verification is the cross-compile build (no native test). Behavior is validated in-game by darkharasho.

**Files:**
- Modify: `src/plugin/dllmain.cpp:85-88` (head-height setup), `:132-139` (standalone chevron anchor), `:152-163` (pip / ring-chevron), `:171` (glow body height)

**Interfaces:**
- Consumes: `plugin::effective_pip_height`, `plugin::effective_chevron_height`, `plugin::fitted_head_height` from `config.hpp`.
- Produces: no new public interface.

- [ ] **Step 1: Update the head-height setup block (lines 85-88)**

The old single `head_h` served pip, chevron, and glow. Remove it; compute per-element at use sites. Replace:

```cpp
    // Head height (feet->top-of-head) for head-anchored styles. When race-fit is
    // on, derive it from the player's race; otherwise fall back to per-style config.
    bool  fit = g_cfg.fit_height_to_race;
    float head_h = fit ? plugin::fitted_head_height(g_cfg, session.race) : 0.0f;
```

with:

```cpp
    // Head-anchored heights (feet->element, meters). Pip and chevron each resolve
    // their own base (race height when fit is on, else a manual base) + own nudge.
    bool  fit = g_cfg.fit_height_to_race;
    float pip_h     = plugin::effective_pip_height(g_cfg, session.race);
    float chevron_h = plugin::effective_chevron_height(g_cfg, session.race);
```

- [ ] **Step 2: Update the standalone-chevron smoothing anchor (lines 134-138)**

Replace the `choff` computation:

```cpp
    if (g_cfg.style == plugin::MarkerStyle::Chevron) {
        float choff = fit ? head_h : g_cfg.head_offset;
        core::Vec3 hw = avatar.position + core::Vec3{0.0f, choff, 0.0f};
```

with:

```cpp
    if (g_cfg.style == plugin::MarkerStyle::Chevron) {
        core::Vec3 hw = avatar.position + core::Vec3{0.0f, chevron_h, 0.0f};
```

- [ ] **Step 3: Update the Ring+pip anchor (lines 152-156)**

Replace:

```cpp
                if (g_cfg.style == plugin::MarkerStyle::RingPip) {
                    float pip_h = fit ? 0.5f * head_h : 1.2f;
                    core::ScreenPoint hp = core::world_to_screen(
                        avatar.position + core::Vec3{0.0f, pip_h, 0.0f}, cam, sz.x, sz.y);
                    if (!hp.behind) plugin::draw_pip(hp.x + ox, hp.y + oy, 4.0f, rgba, ol);
```

with (drop the local `pip_h` redeclaration — it now comes from the outer scope):

```cpp
                if (g_cfg.style == plugin::MarkerStyle::RingPip) {
                    core::ScreenPoint hp = core::world_to_screen(
                        avatar.position + core::Vec3{0.0f, pip_h, 0.0f}, cam, sz.x, sz.y);
                    if (!hp.behind) plugin::draw_pip(hp.x + ox, hp.y + oy, 4.0f, rgba, ol);
```

- [ ] **Step 4: Update the Ring+chevron anchor (lines 157-163)**

Replace:

```cpp
                } else if (g_cfg.style == plugin::MarkerStyle::RingChevron) {
                    float rc_h = fit ? head_h : 2.4f;
                    core::ScreenPoint hp = core::world_to_screen(
                        avatar.position + core::Vec3{0.0f, rc_h, 0.0f}, cam, sz.x, sz.y);
```

with:

```cpp
                } else if (g_cfg.style == plugin::MarkerStyle::RingChevron) {
                    core::ScreenPoint hp = core::world_to_screen(
                        avatar.position + core::Vec3{0.0f, chevron_h, 0.0f}, cam, sz.x, sz.y);
```

- [ ] **Step 5: Update the silhouette-glow body height (line 171)**

The glow uses the plain race height (not a nudged element height). Replace:

```cpp
            float body_h = fit ? head_h : g_cfg.char_height;
```

with:

```cpp
            float body_h = fit ? plugin::fitted_head_height(g_cfg, session.race)
                               : g_cfg.char_height;
```

- [ ] **Step 6: Confirm `fit` is still used**

`fit` is now referenced only in Step 5's ternary. That is fine (still used). If the compiler warns it is unused after your edits, inline `g_cfg.fit_height_to_race` at that call site and delete the `bool fit` line. Expected: it stays used, no change needed.

- [ ] **Step 7: Build the DLL**

Run: `cmake --build build-win`
Expected: `build-win/arcdps_player_outline.dll` builds with no errors or unused-variable warnings.

- [ ] **Step 8: Commit**

```bash
git add src/plugin/dllmain.cpp
git commit -m "feat: anchor pip and chevron via per-type effective-height helpers"
```

---

## Task 4: Options UI — per-type sliders + live readout

**Files:**
- Modify: `src/plugin/config.cpp` (`draw_options`: the fit-to-race block and the per-style Chevron case)

**Interfaces:**
- Consumes: `plugin::effective_pip_height`, `plugin::effective_chevron_height`.
- Produces: no new public interface.

- [ ] **Step 1: Remove the stale manual `head_offset` slider in the Chevron style case**

In the per-style `switch (c.style)` block, the `case MarkerStyle::Chevron:` currently reads:

```cpp
        case MarkerStyle::Chevron:
            ImGui::SliderFloat("Chevron size", &c.chevron_size, 8.0f, 80.0f, "%.0f px");
            if (!c.fit_height_to_race)
                ImGui::SliderFloat("Height above (m)", &c.head_offset, 0.0f, 3.5f, "%.2f");
            break;
```

Replace with (height is now handled uniformly in the fit block; `head_offset` no longer exists):

```cpp
        case MarkerStyle::Chevron:
            ImGui::SliderFloat("Chevron size", &c.chevron_size, 8.0f, 80.0f, "%.0f px");
            break;
```

Also remove the `char_height` line's neighbours? No — leave `SilhouetteGlow` and other cases untouched.

- [ ] **Step 2: Rewrite the "Fit height to race" block**

Replace the whole block (currently the `ImGui::Checkbox("Fit height to race", ...)` section through its closing `}`):

```cpp
    ImGui::Separator();
    ImGui::Checkbox("Fit height to race", &c.fit_height_to_race);

    const char* race_names[5] = { "Asura", "Charr", "Human", "Norn", "Sylvari" };
    int det = (detected == core::GameRace::Unknown) ? -1 : (int)detected;

    if (c.fit_height_to_race) {
        ImGui::TextDisabled("Head height per race (m)");
        for (int i = 0; i < 5; ++i) {
            if (i == det)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.30f, 0.95f, 0.55f, 1.0f));
            ImGui::DragFloat(race_names[i], &c.race_head_m[i], 0.01f, 0.5f, 3.5f, "%.2f");
            if (i == det) {
                ImGui::PopStyleColor();
                ImGui::SameLine();
                ImGui::TextDisabled("(you)");
            }
        }
        if (det < 0) ImGui::TextDisabled("(race not detected -> using Human)");
    } else {
        ImGui::TextDisabled("Manual head-anchor height (m)");
        ImGui::SliderFloat("Pip base (m)",     &c.pip_manual_m,     0.0f, 3.5f, "%.2f");
        ImGui::SliderFloat("Chevron base (m)", &c.chevron_manual_m, 0.0f, 3.5f, "%.2f");
    }

    // Per-type adjusters apply in both modes.
    ImGui::SliderFloat("Pip nudge (m)",     &c.pip_nudge,     -0.5f, 1.5f, "%+.2f");
    ImGui::SliderFloat("Chevron nudge (m)", &c.chevron_nudge, -0.5f, 1.5f, "%+.2f");

    // Live readout of the resulting height for the detected race (Unknown -> Human).
    ImGui::TextDisabled("Pip -> %.2f m    Chevron -> %.2f m",
                        effective_pip_height(c, detected),
                        effective_chevron_height(c, detected));
```

- [ ] **Step 3: Build the DLL**

Run: `cmake --build build-win`
Expected: builds with no errors. (`effective_*` are declared in `config.hpp`, already included by `config.cpp`.)

- [ ] **Step 4: Sanity-run the native tests (nothing config-IO changed, but confirm green)**

Run: `cmake --build build-native && ./build-native/core_tests`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/plugin/config.cpp
git commit -m "feat: per-type height sliders (auto/manual) + live effective-height readout"
```

---

## In-game verification (darkharasho, Bazzite/Proton)

After Task 4, load the DLL in GW2 and confirm:
- Pip and chevron each hover clearly above the head, across races.
- Moving **Pip nudge** shifts only the pip; **Chevron nudge** shifts both the standalone Chevron and the Ring+chevron's V, and not the pip.
- The readout line matches what is on screen.
- Turning **Fit height to race** off reveals the Pip/Chevron base sliders; turning it on shows the per-race table. Nudges stay visible in both.

---

## Self-Review Notes

- **Spec coverage:** per-type nudges (Task 2 fields, Task 3 routing, Task 4 UI); hover defaults + raised race table (Task 2 defaults); config field swap incl. `head_offset -> chevron_manual_m` and dropped `height_nudge` (Task 2 save/load/sanitize); manual bases when auto off + live readout (Task 4); `char_height` untouched (Task 3 Step 5 keeps it); shared helpers as single source of truth (Task 2 + Tasks 3/4 consume them). All covered.
- **Placeholder scan:** no TBD/TODO; every code step has concrete content.
- **Type consistency:** `effective_pip_height`/`effective_chevron_height`/`fitted_head_height` signatures match across Task 2 (declare/define), Task 3, and Task 4. Fields `pip_nudge`, `chevron_nudge`, `pip_manual_m`, `chevron_manual_m` named identically everywhere.
