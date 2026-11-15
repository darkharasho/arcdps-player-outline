# Update Notification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The plugin tells the user in its options panel when a newer GitHub release exists; it never downloads anything.

**Architecture:** Pure version parsing/comparison in `src/core/version.hpp` (unit-tested natively). A Windows-only `src/plugin/update_check.{hpp,cpp}` runs one WinHTTP request on a detached thread and publishes a mutex-guarded status. `dllmain.cpp` starts it at init and draws the status in the options panel.

**Tech Stack:** C++17, MinGW-w64 cross-compile, WinHTTP, Dear ImGui (arcdps's context), doctest.

**Spec:** `docs/superpowers/specs/2026-11-14-update-notify-design.md`

## Global Constraints

- Request: GET `https://github.com/darkharasho/arcdps-player-outline/releases/latest`, redirects off, read the `Location` header. Not api.github.com.
- User agent: `arcdps_player_outline/<version>`. Nothing else identifying is sent.
- Timeout 10 s. Every failure is silent apart from a dim "update check failed" line in the options panel.
- The thread is detached and touches only the guarded status; plugin unload never waits on the network.
- One check per game launch, only when `check_updates` is set (default on, saved in the ini).
- No download, no install, nothing drawn outside the options panel.
- No new third-party code; link `winhttp`.
- Native tests must keep passing: `./build-native/core_tests`. Both DLL variants must build with no warnings (`build-win`, and `build-dev` with `-DPLAYER_OUTLINE_SPIKE=ON`).

## Review Focus

- Dev build version like `0.4.3-2-gabc123` vs latest `v0.4.3`: no notice (covered in Task 1 tests).
- `Location` with a trailing slash, a query string, or no `/releases/tag/` part: tag extracted or "no update", never a crash (Task 1 tests).
- Garbage or empty version on either side (`0.0.0-dev`, `latest`, `v1`): no notice (Task 1 tests).
- No network / DNS failure / GitHub 429: status `Failed`, game unaffected (Task 2, manual check with the network off is optional; code path review required).
- Plugin unloaded while the request is in flight: no use of freed state. The status must live in static storage and the thread must capture nothing owned by the plugin (Task 2 code review).

---

### Task 1: Version parsing and comparison

**Files:**
- Create: `src/core/version.hpp`
- Create: `tests/core/test_version.cpp`
- Modify: `CMakeLists.txt` (add the test file to `core_tests`)

**Interfaces:**
- Produces, in namespace `core`:
  - `bool parse_release_tag(const char* location, char* out, size_t out_size);` copies the text after `/releases/tag/` up to the first `/`, `?`, `#` or end into `out`; returns false if absent, empty or too long for `out`.
  - `bool parse_version(const char* s, int out[3]);` accepts an optional leading `v`, then `major.minor.patch`; anything after the patch digits is ignored; returns false otherwise.
  - `bool is_newer(const char* remote, const char* local);` true only if both parse and remote > local.

- [ ] **Step 1: Write the failing tests** in `tests/core/test_version.cpp` (doctest, same style as `tests/core/test_config.cpp`):

```cpp
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
```

- [ ] **Step 2: Add the file to `core_tests` in `CMakeLists.txt`, build, and confirm it fails to compile** (`core/version.hpp` missing).

Run: `cmake -S . -B build-native && cmake --build build-native`

- [ ] **Step 3: Implement the three functions as `inline` in `src/core/version.hpp`.** No Windows headers, no allocation.

- [ ] **Step 4: Run the tests.**

Run: `cmake --build build-native && ./build-native/core_tests`
Expected: `Status: SUCCESS!`

- [ ] **Step 5: Commit** `feat: release tag parsing and version comparison`.

### Task 2: Update check, config flag and options-panel notice

**Files:**
- Create: `src/plugin/update_check.hpp`, `src/plugin/update_check.cpp`
- Modify: `src/plugin/config.hpp`, `src/plugin/config_io.cpp`, `src/plugin/dllmain.cpp`, `CMakeLists.txt`, `tests/core/test_config.cpp`, `README.md`

**Interfaces:**
- Consumes: `core::parse_release_tag`, `core::is_newer` from Task 1.
- Produces, in namespace `plugin`:

```cpp
enum class UpdateState { Idle, Checking, UpToDate, Available, Failed };
struct UpdateStatus { UpdateState state = UpdateState::Idle; char tag[32] = ""; };
void start_update_check(const char* current_version);   // no-op if already started
UpdateStatus update_status();                           // thread-safe copy
```

- [ ] **Step 1: Config flag.** Add `bool check_updates = true;` to `plugin::Config`; save/load it as ini key `check_updates`. Extend the round-trip test in `tests/core/test_config.cpp` to set it to `false`, save, load, and `CHECK_FALSE(loaded.check_updates)`; also check a config loaded from an ini without the key keeps `true`. Run `./build-native/core_tests`: SUCCESS.

- [ ] **Step 2: Implement `update_check.cpp`.** `start_update_check` sets state `Checking` and spawns a detached `std::thread` that copies the version string by value. The thread: `WinHttpOpen` (user agent per Global Constraints) → `WinHttpSetTimeouts` 10000 ms each → `WinHttpConnect` `github.com:443` → `WinHttpOpenRequest` GET `/darkharasho/arcdps-player-outline/releases/latest` with `WINHTTP_FLAG_SECURE` → set `WINHTTP_OPTION_REDIRECT_POLICY` to `WINHTTP_OPTION_REDIRECT_POLICY_NEVER` → send, receive → `WinHttpQueryHeaders(WINHTTP_QUERY_LOCATION)` → convert to UTF-8 → `parse_release_tag` → `is_newer` → `Available` (with tag) or `UpToDate`. Any failed call or missing header → `Failed`. Close all handles on every path. Status and its `std::mutex` are function-local statics or file statics; the thread captures nothing else.

- [ ] **Step 3: Build wiring.** Add `src/plugin/update_check.cpp` to the DLL target and `winhttp` to `target_link_libraries` in `CMakeLists.txt`.

- [ ] **Step 4: Plugin wiring in `dllmain.cpp`.** In `mod_init`, after `load_config`: `if (g_cfg.check_updates) plugin::start_update_check(kBuild);`. At the top of `options_cb`, before `draw_options`:
  - `Available`: yellow text `Update available: <tag> (you have <kBuild>)`, then the text `github.com/darkharasho/arcdps-player-outline/releases` and a `Copy link` button that calls `ImGui::SetClipboardText` with the full `https://` URL.
  - `Failed`: `ImGui::TextDisabled("Update check failed")`.
  - Other states: nothing.
  - Then `ImGui::Checkbox("Check for updates on startup", &g_cfg.check_updates);` and a separator.

- [ ] **Step 5: Build both DLL variants with no warnings.**

Run: `cmake --build build-win && cmake --build build-dev`
Expected: `Built target arcdps_player_outline` twice, no warnings or errors. Also `x86_64-w64-mingw32-objdump -p build-win/arcdps_player_outline.dll | grep -i winhttp` shows the import.

- [ ] **Step 6: Manual check (needs the user, in-game).** CMake defines `PLUGIN_VERSION` from the git tag, so temporarily edit `kBuild` in `dllmain.cpp` to `"0.0.1"`, deploy with `./scripts/deploy.sh`, and have the user confirm the notice and the Copy link button. Revert the edit and rebuild before committing.

- [ ] **Step 7: README.** Add a Features bullet and one Usage line for the update notice, stating it only notifies and makes one anonymous request to github.com per launch, with the checkbox to turn it off.

- [ ] **Step 8: Commit** `feat: notify in the options panel when a newer release exists`.
