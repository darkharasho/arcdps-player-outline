# arcdps-player-outline — Self-Marker MVP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship an arcdps plugin that reads MumbleLink and draws a persistent, always-on-top screen-space marker anchored to the player's own character, so you can find yourself in a zerg.

**Architecture:** The plugin is a Windows x64 DLL cross-compiled from Linux with MinGW-w64. All camera/projection math and MumbleLink struct parsing live in a **pure, platform-independent C++ core** (`src/core/`) that compiles two ways: into the plugin (MinGW) and into a native Linux test binary (system g++ + doctest) for a real red/green TDD loop. Only the Windows-specific glue — arcdps exports, the shared-memory read, and ImGui rendering — is untestable off-game and is verified manually in GW2. The "outline" north star is delivered as an overlay-space marker (default: a ground ring at the character's feet), not a render-pipeline hook. This keeps the plugin read-only and in line with what arcdps is tolerated for.

**Tech Stack:** C++17, CMake, MinGW-w64 (`x86_64-w64-mingw32-g++`), vendored Dear ImGui (shared context from arcdps), vendored doctest (native tests only), Win32 shared-memory API for MumbleLink.

## Global Constraints

- Plugin DLL output name: `arcdps_player_outline.dll` (arcdps loads extensions matching `arcdps_*.dll`).
- Plugin `sig`: `0x504F4C4E` (arbitrary unique u32; must not collide with other loaded extensions).
- No memory reads of the GW2 process, no D3D render-pipeline modification. Overlay + MumbleLink shared memory only.
- `src/core/` MUST NOT include any Windows or ImGui header — it is compiled natively for tests. Keep it free of `<windows.h>`, `imgui.h`, and platform types.
- MumbleLink positions are in **meters**; `identity` `fov` is **vertical FoV in radians**. GW2 world is treated as left-handed, Y-up. Exact handedness sign is confirmed in-game (Task 8), not asserted as fact in unit tests — core tests assert **invariants** (front→center, right→right-half, behind→flagged), which hold under any self-consistent convention.
- Static-link the C++ runtime into the DLL (`-static -static-libgcc -static-libstdc++`) so it has no external DLL dependencies under Proton.
- Default marker anchor: character **feet** (ground ring). Head/chevron styles are selectable but not the default.
- C++17, warnings-as-errors off for vendored headers only.

---

## File Structure

```
CMakeLists.txt                     # two targets: plugin (mingw) + core_tests (native)
cmake/mingw-w64-x86_64.cmake       # cross toolchain file
third_party/doctest/doctest.h      # vendored, native tests only
third_party/imgui/                 # vendored Dear ImGui (compiled into plugin only)
third_party/arcdps/arcdps.h        # minimal vendored arcdps extension header
src/core/vec3.hpp                  # pure math: Vec3 + ops
src/core/mumble_data.hpp           # LinkedMem struct + parsed CameraState/AvatarState + identity FoV parse
src/core/mumble_data.cpp           # identity JSON fov parse impl
src/core/camera.hpp                # look-at view matrix, projection, world->screen, off-screen clamp
src/core/camera.cpp                # camera math impl
src/plugin/dllmain.cpp             # arcdps exports (get_init_addr/mod_init/mod_release), ImGui wiring
src/plugin/mumble_link.hpp/.cpp    # Win32 shared-memory reader -> fills core structs
src/plugin/marker.hpp/.cpp         # ImGui draw of marker styles from a screen-space result
src/plugin/config.hpp/.cpp         # options UI + ini load/save
tests/core/test_vec3.cpp
tests/core/test_mumble_parse.cpp
tests/core/test_camera.cpp
tests/core/doctest_main.cpp        # #define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
```

**Responsibility split:** `src/core/` = all logic that can be reasoned about and tested without the game. `src/plugin/` = Windows/ImGui/arcdps glue, thin, delegates to core. Files that change together (a struct + its parser) live together.

---

## Task 1: Build system + toolchain + native test harness

**Files:**
- Create: `CMakeLists.txt`
- Create: `cmake/mingw-w64-x86_64.cmake`
- Create: `third_party/doctest/doctest.h` (download vendored single header)
- Create: `tests/core/doctest_main.cpp`
- Create: `src/core/vec3.hpp`
- Create: `tests/core/test_vec3.cpp`

**Interfaces:**
- Produces: `struct Vec3 { float x, y, z; }` with `operator+ - `, scalar `*`, `dot`, `cross`, `length`, `normalized` — consumed by all later core tasks.
- Produces: CMake target `core_tests` (native) and `arcdps_player_outline` (mingw, added in Task 2).

- [ ] **Step 1: Vendor doctest**

Run:
```bash
mkdir -p third_party/doctest
curl -fsSL https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h -o third_party/doctest/doctest.h
test -s third_party/doctest/doctest.h && echo OK
```
Expected: `OK`

- [ ] **Step 2: Write the toolchain file**

`cmake/mingw-w64-x86_64.cmake`:
```cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER   x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER  x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

- [ ] **Step 3: Write the failing test**

`src/core/vec3.hpp`:
```cpp
#pragma once
namespace core {
struct Vec3 { float x{}, y{}, z{}; };
}
```

`tests/core/doctest_main.cpp`:
```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
```

`tests/core/test_vec3.cpp`:
```cpp
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
```

- [ ] **Step 4: Write CMakeLists.txt**

`CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.20)
project(arcdps_player_outline CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # ---- Plugin DLL target (populated in Task 2) ----
    # placeholder library added in Task 2
else()
    # ---- Native core test target ----
    add_executable(core_tests
        tests/core/doctest_main.cpp
        tests/core/test_vec3.cpp)
    target_include_directories(core_tests PRIVATE
        third_party/doctest src/core)
    enable_testing()
    add_test(NAME core_tests COMMAND core_tests)
endif()
```

- [ ] **Step 5: Run the native test to verify it FAILS to compile**

Run:
```bash
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Debug >/dev/null && cmake --build build-native 2>&1 | tail -5
```
Expected: FAIL — `dot`/`cross`/`length`/`normalized` not declared.

- [ ] **Step 6: Implement the math in vec3.hpp**

Replace `src/core/vec3.hpp`:
```cpp
#pragma once
#include <cmath>
namespace core {
struct Vec3 { float x{}, y{}, z{}; };
inline Vec3 operator+(Vec3 a, Vec3 b){ return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b){ return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline Vec3 operator*(Vec3 a, float s){ return {a.x*s,a.y*s,a.z*s}; }
inline float dot(Vec3 a, Vec3 b){ return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec3 cross(Vec3 a, Vec3 b){
    return { a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x };
}
inline float length(Vec3 a){ return std::sqrt(dot(a,a)); }
inline Vec3 normalized(Vec3 a){ float l=length(a); return l>0 ? a*(1.0f/l) : a; }
}
```

- [ ] **Step 7: Run tests to verify they PASS**

Run:
```bash
cmake --build build-native 2>&1 | tail -3 && ./build-native/core_tests
```
Expected: PASS, `1 test case ... 0 failed`.

- [ ] **Step 8: Add .gitignore entries and commit**

Run:
```bash
printf '\nbuild-native/\nbuild-win/\n*.dll\n' >> .gitignore
git add CMakeLists.txt cmake/ third_party/doctest/ src/core/vec3.hpp tests/ .gitignore
git commit -m "build: cmake + mingw toolchain + native doctest harness with Vec3"
```

---

## Task 2: Minimal arcdps plugin that loads and draws a test dot

**Files:**
- Create: `third_party/arcdps/arcdps.h`
- Create: `third_party/imgui/` (vendored ImGui sources)
- Create: `src/plugin/dllmain.cpp`
- Modify: `CMakeLists.txt` (Windows branch)

**Interfaces:**
- Consumes: nothing from core yet.
- Produces: exported `extern "C" __declspec(dllexport) void* get_init_addr(...)`; a global `bool g_show_test_dot` toggled later.

**Note:** Not unit-testable. Deliverable = DLL compiles, exports the correct symbol, and (manual) shows a dot in-game.

- [ ] **Step 1: Vendor ImGui (docking not required; use master)**

Run:
```bash
mkdir -p third_party/imgui
for f in imgui.h imconfig.h imgui.cpp imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp imgui_internal.h imstb_rectpack.h imstb_textedit.h imstb_truetype.h; do
  curl -fsSL "https://raw.githubusercontent.com/ocornut/imgui/v1.90.9/$f" -o "third_party/imgui/$f"; done
ls third_party/imgui | wc -l
```
Expected: `11`

- [ ] **Step 2: Write the minimal arcdps header**

`third_party/arcdps/arcdps.h`:
```cpp
#pragma once
#include <cstdint>
struct arcdps_exports {
    uintptr_t size;
    uint32_t sig;
    uint32_t imguivers;
    const char* out_name;
    const char* out_build;
    void* wnd_nofilter;
    void* combat;
    void* imgui;
    void* options_end;
    void* combat_local;
    void* wnd_filter;
    void* options_windows;
};
```

- [ ] **Step 3: Write dllmain.cpp**

`src/plugin/dllmain.cpp`:
```cpp
#include <windows.h>
#include <cstdint>
#include "imgui.h"
#include "arcdps.h"

static arcdps_exports g_arc{};
static const char* kName  = "player_outline";
static const char* kBuild = "0.1.0";
bool g_show_test_dot = true;

// called each frame by arcdps; not_charsel_or_loading==1 when safe to draw
static uintptr_t imgui_cb(uint32_t not_charsel_or_loading, uint32_t /*hide*/) {
    if (!not_charsel_or_loading) return 0;
    if (g_show_test_dot) {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        ImVec2 sz = ImGui::GetIO().DisplaySize;
        dl->AddCircleFilled(ImVec2(sz.x*0.5f, sz.y*0.5f), 12.0f,
                            IM_COL32(0,255,200,220), 32);
    }
    return 0;
}

static arcdps_exports* mod_init() {
    g_arc.size = sizeof(arcdps_exports);
    g_arc.sig = 0x504F4C4E;
    g_arc.imguivers = IMGUI_VERSION_NUM;
    g_arc.out_name = kName;
    g_arc.out_build = kBuild;
    g_arc.imgui = (void*)imgui_cb;
    return &g_arc;
}
static uintptr_t mod_release() { return 0; }

extern "C" __declspec(dllexport)
void* get_init_addr(char* /*arcversion*/, void* imguictx, void* /*id3dptr*/,
                    HMODULE /*arcdll*/, void* mallocfn, void* freefn,
                    uint32_t /*d3dversion*/) {
    ImGui::SetCurrentContext((ImGuiContext*)imguictx);
    ImGui::SetAllocatorFunctions(
        (void*(*)(size_t,void*))mallocfn, (void(*)(void*,void*))freefn);
    return (void*)mod_init;
}

extern "C" __declspec(dllexport)
void* get_release_addr() { return (void*)mod_release; }

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
```

- [ ] **Step 4: Add the Windows branch to CMakeLists.txt**

Replace the `if(CMAKE_SYSTEM_NAME STREQUAL "Windows")` placeholder block with:
```cmake
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    file(GLOB IMGUI_SRC third_party/imgui/*.cpp)
    add_library(arcdps_player_outline SHARED
        src/plugin/dllmain.cpp
        ${IMGUI_SRC})
    set_target_properties(arcdps_player_outline PROPERTIES
        PREFIX "" OUTPUT_NAME "arcdps_player_outline")
    target_include_directories(arcdps_player_outline PRIVATE
        third_party/imgui third_party/arcdps src/core)
    target_link_options(arcdps_player_outline PRIVATE
        -static -static-libgcc -static-libstdc++)
```
(leave the existing `else()`/native block below unchanged.)

- [ ] **Step 5: Cross-compile the DLL**

Run:
```bash
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake >/dev/null \
 && cmake --build build-win 2>&1 | tail -3
```
Expected: builds `build-win/arcdps_player_outline.dll`.

- [ ] **Step 6: Verify the export symbol exists**

Run:
```bash
x86_64-w64-mingw32-objdump -p build-win/arcdps_player_outline.dll | grep -i get_init_addr
```
Expected: `get_init_addr` listed under `[Ordinal/Name Pointer] Table`.

- [ ] **Step 7: Manual in-game check (checkpoint — user)**

Copy `build-win/arcdps_player_outline.dll` into the GW2 arcdps addons folder, launch GW2, confirm a teal dot appears at screen center. **STOP for user confirmation before proceeding.**

- [ ] **Step 8: Commit**

```bash
git add third_party/arcdps third_party/imgui src/plugin/dllmain.cpp CMakeLists.txt
git commit -m "feat: minimal arcdps plugin draws test dot via shared ImGui context"
```

---

## Task 3: MumbleLink struct + identity FoV parsing (native TDD)

**Files:**
- Create: `src/core/mumble_data.hpp`
- Create: `src/core/mumble_data.cpp`
- Create: `tests/core/test_mumble_parse.cpp`
- Modify: `CMakeLists.txt` (add sources to `core_tests`)

**Interfaces:**
- Produces: `struct LinkedMem` (raw shared-memory layout), `struct CameraState { core::Vec3 position, front; float fov_y; }`, `struct AvatarState { core::Vec3 position; bool valid; }`.
- Produces: `float parse_identity_fov(const char* utf8_identity, float fallback);` and `AvatarState read_avatar(const LinkedMem&);` `CameraState read_camera(const LinkedMem&, float fov_y);`
- Consumed by Task 5 (projection) and Task 7 (Win reader).

- [ ] **Step 1: Write the failing test**

`tests/core/test_mumble_parse.cpp`:
```cpp
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
```

- [ ] **Step 2: Write the header**

`src/core/mumble_data.hpp`:
```cpp
#pragma once
#include <cstdint>
#include "vec3.hpp"
namespace core {

// Exact GW2 MumbleLink shared-memory layout.
struct LinkedMem {
    uint32_t uiVersion;
    uint32_t uiTick;
    float fAvatarPosition[3];
    float fAvatarFront[3];
    float fAvatarTop[3];
    wchar_t name[256];
    float fCameraPosition[3];
    float fCameraFront[3];
    float fCameraTop[3];
    wchar_t identity[256];
    uint32_t context_len;
    unsigned char context[256];
    wchar_t description[2048];
};

struct AvatarState { Vec3 position{}; bool valid{false}; };
struct CameraState { Vec3 position{}; Vec3 front{}; float fov_y{1.222f}; };

float parse_identity_fov(const char* utf8_identity, float fallback);
AvatarState read_avatar(const LinkedMem& m);
CameraState read_camera(const LinkedMem& m, float fov_y);
}
```

- [ ] **Step 3: Run test to verify it FAILS**

Run: `cmake --build build-native 2>&1 | tail -5`
Expected: FAIL — undefined references / functions not defined.

- [ ] **Step 4: Implement the parser**

`src/core/mumble_data.cpp`:
```cpp
#include "mumble_data.hpp"
#include <cstdlib>
#include <cstring>
namespace core {

float parse_identity_fov(const char* id, float fallback) {
    if (!id) return fallback;
    const char* key = std::strstr(id, "\"fov\"");
    if (!key) return fallback;
    const char* colon = std::strchr(key, ':');
    if (!colon) return fallback;
    char* end = nullptr;
    float v = std::strtof(colon + 1, &end);
    if (end == colon + 1 || v <= 0.0f) return fallback;
    return v;
}

AvatarState read_avatar(const LinkedMem& m) {
    AvatarState a;
    a.position = { m.fAvatarPosition[0], m.fAvatarPosition[1], m.fAvatarPosition[2] };
    bool nonzero = a.position.x!=0 || a.position.y!=0 || a.position.z!=0;
    a.valid = (m.uiTick != 0) && nonzero;
    return a;
}

CameraState read_camera(const LinkedMem& m, float fov_y) {
    CameraState c;
    c.position = { m.fCameraPosition[0], m.fCameraPosition[1], m.fCameraPosition[2] };
    c.front    = normalized({ m.fCameraFront[0], m.fCameraFront[1], m.fCameraFront[2] });
    c.fov_y    = fov_y;
    return c;
}
}
```

- [ ] **Step 5: Add sources to core_tests and rebuild**

In `CMakeLists.txt` native branch, extend `add_executable(core_tests ...)` to include:
```cmake
        tests/core/test_mumble_parse.cpp
        src/core/mumble_data.cpp
```

Run: `cmake -S . -B build-native >/dev/null && cmake --build build-native 2>&1 | tail -3 && ./build-native/core_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/core/mumble_data.* tests/core/test_mumble_parse.cpp CMakeLists.txt
git commit -m "feat(core): MumbleLink layout + identity fov parse (native tested)"
```

---

## Task 4: Look-at view matrix (native TDD)

**Files:**
- Create: `src/core/camera.hpp`
- Create: `src/core/camera.cpp`
- Create: `tests/core/test_camera.cpp`
- Modify: `CMakeLists.txt` (add to `core_tests`)

**Interfaces:**
- Produces: `struct Mat4 { float m[16]; };` (column-major), `Mat4 look_at(Vec3 eye, Vec3 front, Vec3 up);`
- Consumed by Task 5.

- [ ] **Step 1: Write the failing test (invariants, not raw numbers)**

`tests/core/test_camera.cpp`:
```cpp
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
```

- [ ] **Step 2: Write the header**

`src/core/camera.hpp`:
```cpp
#pragma once
#include "vec3.hpp"
namespace core {
struct Mat4 { float m[16]{}; };   // column-major
Mat4 look_at(Vec3 eye, Vec3 front, Vec3 up);
}
```

- [ ] **Step 3: Run test to verify it FAILS**

Run: `cmake --build build-native 2>&1 | tail -5`
Expected: FAIL — `look_at` undefined.

- [ ] **Step 4: Implement look_at (left-handed, +Z forward in view space)**

`src/core/camera.cpp`:
```cpp
#include "camera.hpp"
namespace core {

Mat4 look_at(Vec3 eye, Vec3 front, Vec3 up) {
    Vec3 f = normalized(front);
    Vec3 r = normalized(cross(up, f));   // left-handed
    Vec3 u = cross(f, r);
    Mat4 M;
    // rows = basis; column-major storage
    M.m[0]=r.x; M.m[4]=r.y; M.m[8]=r.z;  M.m[12]=-dot(r,eye);
    M.m[1]=u.x; M.m[5]=u.y; M.m[9]=u.z;  M.m[13]=-dot(u,eye);
    M.m[2]=f.x; M.m[6]=f.y; M.m[10]=f.z; M.m[14]=-dot(f,eye);
    M.m[3]=0;   M.m[7]=0;   M.m[11]=0;   M.m[15]=1;
    return M;
}
}
```

- [ ] **Step 5: Add to core_tests, rebuild, verify PASS**

Add `tests/core/test_camera.cpp` and `src/core/camera.cpp` to `core_tests` in `CMakeLists.txt`.
Run: `cmake -S . -B build-native >/dev/null && cmake --build build-native 2>&1 | tail -3 && ./build-native/core_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/core/camera.* tests/core/test_camera.cpp CMakeLists.txt
git commit -m "feat(core): left-handed look_at view matrix (native tested)"
```

---

## Task 5: Projection + world→screen (native TDD)

**Files:**
- Modify: `src/core/camera.hpp`, `src/core/camera.cpp`
- Modify: `tests/core/test_camera.cpp`

**Interfaces:**
- Produces:
  - `Mat4 perspective(float fov_y, float aspect, float znear, float zfar);`
  - `struct ScreenPoint { float x, y; bool on_screen; bool behind; };`
  - `ScreenPoint world_to_screen(Vec3 world, const CameraState& cam, float screen_w, float screen_h);`
- Consumes: `CameraState` from Task 3, `Mat4`/`look_at` from Task 4.
- Consumed by Task 6 (clamp) and Task 8 (plugin).

Add `#include "mumble_data.hpp"` to `camera.hpp` for `CameraState`.

- [ ] **Step 1: Add failing tests**

Append to `tests/core/test_camera.cpp`:
```cpp
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
```

- [ ] **Step 2: Declare the new API in camera.hpp**

Add to `src/core/camera.hpp` (inside namespace):
```cpp
#include "mumble_data.hpp"
...
Mat4 perspective(float fov_y, float aspect, float znear, float zfar);
struct ScreenPoint { float x{}, y{}; bool on_screen{false}; bool behind{false}; };
ScreenPoint world_to_screen(Vec3 world, const CameraState& cam,
                            float screen_w, float screen_h);
```

- [ ] **Step 3: Run to verify FAIL**

Run: `cmake --build build-native 2>&1 | tail -5`
Expected: FAIL — `perspective`/`world_to_screen` undefined.

- [ ] **Step 4: Implement**

Add to `src/core/camera.cpp`:
```cpp
#include <cmath>

Mat4 perspective(float fov_y, float aspect, float znear, float zfar) {
    float t = 1.0f / std::tan(fov_y * 0.5f);
    Mat4 P;
    P.m[0]  = t / aspect;
    P.m[5]  = t;
    P.m[10] = zfar / (zfar - znear);          // LH
    P.m[11] = 1.0f;
    P.m[14] = -(zfar * znear) / (zfar - znear);
    return P;
}

static void mul(const Mat4& A, const Mat4& B, Mat4& out) { // out = A*B (col-major)
    for (int c=0;c<4;++c) for (int r=0;r<4;++r) {
        float s=0; for (int k=0;k<4;++k) s += A.m[k*4+r]*B.m[c*4+k];
        out.m[c*4+r]=s;
    }
}

ScreenPoint world_to_screen(Vec3 world, const CameraState& cam,
                            float screen_w, float screen_h) {
    Mat4 V = look_at(cam.position, cam.front, {0,1,0});
    Mat4 P = perspective(cam.fov_y, screen_w/screen_h, 0.05f, 10000.0f);
    Mat4 VP; mul(P, V, VP);
    float cx = VP.m[0]*world.x + VP.m[4]*world.y + VP.m[8]*world.z  + VP.m[12];
    float cy = VP.m[1]*world.x + VP.m[5]*world.y + VP.m[9]*world.z  + VP.m[13];
    float cw = VP.m[3]*world.x + VP.m[7]*world.y + VP.m[11]*world.z + VP.m[15];
    ScreenPoint sp;
    if (cw <= 0.0001f) { sp.behind = true; return sp; }
    float ndcx = cx / cw, ndcy = cy / cw;
    sp.x = (ndcx * 0.5f + 0.5f) * screen_w;
    sp.y = (1.0f - (ndcy * 0.5f + 0.5f)) * screen_h;   // y-down for screen
    sp.on_screen = (ndcx>=-1 && ndcx<=1 && ndcy>=-1 && ndcy<=1);
    return sp;
}
```

- [ ] **Step 5: Rebuild, verify PASS**

Run: `cmake --build build-native 2>&1 | tail -3 && ./build-native/core_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/core/camera.* tests/core/test_camera.cpp
git commit -m "feat(core): perspective + world_to_screen with behind-camera flag"
```

---

## Task 6: Off-screen edge clamp / arrow direction (native TDD)

**Files:**
- Modify: `src/core/camera.hpp`, `src/core/camera.cpp`, `tests/core/test_camera.cpp`

**Interfaces:**
- Produces: `struct EdgePoint { float x, y, angle_rad; };`
  `EdgePoint clamp_to_edge(float sx, float sy, float w, float h, float margin);`
  Given a (possibly off-screen) screen point, returns a point on the screen-rect inset by `margin`, plus the angle to draw an arrow. Behind-camera callers pass the mirrored point (handled in Task 8).
- Consumed by Task 8/10 (arrow rendering).

- [ ] **Step 1: Add failing test**

Append to `tests/core/test_camera.cpp`:
```cpp
TEST_CASE("clamp pulls an off-screen point onto the inset rect") {
    EdgePoint e = clamp_to_edge(3000, 540, 1920, 1080, 40);
    CHECK(e.x == doctest::Approx(1880).epsilon(0.02));   // 1920-40
    CHECK(e.y <= 1040.0f);
    CHECK(e.y >= 40.0f);
}
TEST_CASE("clamp leaves on-screen points essentially in place") {
    EdgePoint e = clamp_to_edge(960, 540, 1920, 1080, 40);
    CHECK(e.x == doctest::Approx(960).epsilon(0.02));
    CHECK(e.y == doctest::Approx(540).epsilon(0.02));
}
```

- [ ] **Step 2: Declare in camera.hpp**

```cpp
struct EdgePoint { float x{}, y{}, angle_rad{}; };
EdgePoint clamp_to_edge(float sx, float sy, float w, float h, float margin);
```

- [ ] **Step 3: Run to verify FAIL**

Run: `cmake --build build-native 2>&1 | tail -5`
Expected: FAIL — `clamp_to_edge` undefined.

- [ ] **Step 4: Implement**

Add to `src/core/camera.cpp`:
```cpp
EdgePoint clamp_to_edge(float sx, float sy, float w, float h, float margin) {
    float cx = w*0.5f, cy = h*0.5f;
    float dx = sx - cx, dy = sy - cy;
    EdgePoint e; e.angle_rad = std::atan2(dy, dx);
    float minx=margin, maxx=w-margin, miny=margin, maxy=h-margin;
    if (sx>=minx && sx<=maxx && sy>=miny && sy<=maxy) { e.x=sx; e.y=sy; return e; }
    if (dx==0 && dy==0) { e.x=cx; e.y=cy; return e; }
    // scale the direction so it hits the nearest inset edge
    float tx = dx>0 ? (maxx-cx)/dx : (dx<0 ? (minx-cx)/dx : 1e30f);
    float ty = dy>0 ? (maxy-cy)/dy : (dy<0 ? (miny-cy)/dy : 1e30f);
    float t = tx<ty ? tx : ty;
    e.x = cx + dx*t; e.y = cy + dy*t;
    return e;
}
```

- [ ] **Step 5: Rebuild, verify PASS**

Run: `cmake --build build-native 2>&1 | tail -3 && ./build-native/core_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/core/camera.* tests/core/test_camera.cpp
git commit -m "feat(core): off-screen edge clamp + arrow angle (native tested)"
```

---

## Task 7: Win32 MumbleLink shared-memory reader

**Files:**
- Create: `src/plugin/mumble_link.hpp`
- Create: `src/plugin/mumble_link.cpp`
- Modify: `CMakeLists.txt` (add to plugin sources)

**Interfaces:**
- Consumes: `core::LinkedMem`, `core::AvatarState`, `core::CameraState`, `core::parse_identity_fov`, `read_avatar`, `read_camera`.
- Produces:
  - `class MumbleReader { public: bool open(); bool sample(core::AvatarState& avatar, core::CameraState& cam); };`
  - `sample` returns false if link not present or stale; converts `identity` (UTF-16) to UTF-8 before `parse_identity_fov`.

**Note:** Win32 IO — not natively unit-testable. Verified via the plugin in-game (Task 8). The struct-parsing it delegates to IS covered by Task 3 tests.

- [ ] **Step 1: Write the header**

`src/plugin/mumble_link.hpp`:
```cpp
#pragma once
#include "mumble_data.hpp"
namespace plugin {
class MumbleReader {
public:
    ~MumbleReader();
    bool open();                          // maps "MumbleLink"
    bool sample(core::AvatarState& avatar, core::CameraState& cam);
private:
    void* handle_ = nullptr;              // HANDLE
    const core::LinkedMem* mem_ = nullptr;
    uint32_t last_tick_ = 0xFFFFFFFF;
};
}
```

- [ ] **Step 2: Write the implementation**

`src/plugin/mumble_link.cpp`:
```cpp
#include "mumble_link.hpp"
#include <windows.h>
#include <cstring>

namespace plugin {

static float fov_from_identity(const wchar_t* wid) {
    char utf8[512] = {0};
    WideCharToMultiByte(CP_UTF8, 0, wid, -1, utf8, sizeof(utf8)-1, nullptr, nullptr);
    return core::parse_identity_fov(utf8, 1.222f);   // ~70deg vertical fallback
}

MumbleReader::~MumbleReader() {
    if (mem_) UnmapViewOfFile((LPCVOID)mem_);
    if (handle_) CloseHandle((HANDLE)handle_);
}

bool MumbleReader::open() {
    if (mem_) return true;
    HANDLE h = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                  0, sizeof(core::LinkedMem), L"MumbleLink");
    if (!h) return false;
    void* view = MapViewOfFile(h, FILE_MAP_READ, 0, 0, sizeof(core::LinkedMem));
    if (!view) { CloseHandle(h); return false; }
    handle_ = h; mem_ = (const core::LinkedMem*)view;
    return true;
}

bool MumbleReader::sample(core::AvatarState& avatar, core::CameraState& cam) {
    if (!mem_ && !open()) return false;
    if (mem_->uiTick == last_tick_) { /* stale frame, but still usable */ }
    last_tick_ = mem_->uiTick;
    avatar = core::read_avatar(*mem_);
    if (!avatar.valid) return false;
    float fov = fov_from_identity(mem_->identity);
    cam = core::read_camera(*mem_, fov);
    return true;
}
}
```

- [ ] **Step 3: Add to plugin target and cross-compile**

In `CMakeLists.txt` Windows branch, add `src/plugin/mumble_link.cpp` and `src/core/mumble_data.cpp` and `src/core/camera.cpp` to the `arcdps_player_outline` sources, and add `src/core` to its include dirs (already present).

Run:
```bash
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake >/dev/null \
 && cmake --build build-win 2>&1 | tail -3
```
Expected: builds without error.

- [ ] **Step 4: Commit**

```bash
git add src/plugin/mumble_link.* CMakeLists.txt
git commit -m "feat(plugin): Win32 MumbleLink reader delegating to core parsers"
```

---

## Task 8: Wire projection into the plugin — draw the ground-ring marker

**Files:**
- Create: `src/plugin/marker.hpp`
- Create: `src/plugin/marker.cpp`
- Modify: `src/plugin/dllmain.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `core::world_to_screen`, `MumbleReader`.
- Produces: `void draw_ground_ring(const core::ScreenPoint& feet, float radius_px, unsigned rgba);` (uses `ImGui::GetBackgroundDrawList`).

**Note:** This is the first end-to-end in-game moment — the marker tracks your character. Handedness/sign issues surface here; fix by flipping the offending axis and re-confirming (see Step 5).

- [ ] **Step 1: Write marker.hpp**

`src/plugin/marker.hpp`:
```cpp
#pragma once
#include "camera.hpp"
namespace plugin {
void draw_ground_ring(const core::ScreenPoint& feet, float radius_px, unsigned rgba);
}
```

- [ ] **Step 2: Write marker.cpp**

`src/plugin/marker.cpp`:
```cpp
#include "marker.hpp"
#include "imgui.h"
namespace plugin {
void draw_ground_ring(const core::ScreenPoint& feet, float radius_px, unsigned rgba) {
    if (feet.behind) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddCircle(ImVec2(feet.x, feet.y), radius_px, rgba, 48, 3.0f);
    dl->AddCircleFilled(ImVec2(feet.x, feet.y), 3.0f, rgba, 12);
}
}
```

- [ ] **Step 3: Replace the imgui callback body in dllmain.cpp**

In `src/plugin/dllmain.cpp` add includes and a static reader, and replace `imgui_cb`:
```cpp
#include "mumble_link.hpp"
#include "marker.hpp"
#include "camera.hpp"
static plugin::MumbleReader g_reader;

static uintptr_t imgui_cb(uint32_t not_charsel_or_loading, uint32_t) {
    if (!not_charsel_or_loading) return 0;
    core::AvatarState avatar; core::CameraState cam;
    if (!g_reader.sample(avatar, cam)) return 0;
    ImVec2 sz = ImGui::GetIO().DisplaySize;
    core::ScreenPoint feet = core::world_to_screen(avatar.position, cam, sz.x, sz.y);
    plugin::draw_ground_ring(feet, 42.0f, IM_COL32(0,255,200,220));
    return 0;
}
```
Remove the old `g_show_test_dot` block.

- [ ] **Step 4: Add marker.cpp to plugin, cross-compile**

Add `src/plugin/marker.cpp` to `arcdps_player_outline` sources. Run:
```bash
cmake --build build-win 2>&1 | tail -3 && \
x86_64-w64-mingw32-objdump -p build-win/arcdps_player_outline.dll | grep -i get_init_addr
```
Expected: builds; `get_init_addr` present.

- [ ] **Step 5: In-game verification (checkpoint — user)**

Copy DLL into GW2 arcdps folder. Confirm the ring sits at your character's feet and tracks as you move/rotate the camera. If it mirrors left/right → flip sign of `right` in `look_at` (change `cross(up,f)` to `cross(f,up)`); if vertical is inverted → flip the `sp.y` line in `world_to_screen`. Re-run Task 5 native tests after any sign change; the invariants still hold. **STOP for user confirmation.**

- [ ] **Step 6: Commit**

```bash
git add src/plugin/marker.* src/plugin/dllmain.cpp CMakeLists.txt
git commit -m "feat: ground-ring marker tracks own character via MumbleLink projection"
```

---

## Task 9: Config UI + ini persistence + marker styles

**Files:**
- Create: `src/plugin/config.hpp`
- Create: `src/plugin/config.cpp`
- Modify: `src/plugin/marker.hpp`, `src/plugin/marker.cpp`, `src/plugin/dllmain.cpp`, `CMakeLists.txt`

**Interfaces:**
- Produces:
  - `enum class MarkerStyle { GroundRing, Chevron, SilhouetteGlow };`
  - `struct Config { MarkerStyle style=MarkerStyle::GroundRing; float size=42; float opacity=0.86f; float color[3]={0,1,0.78f}; bool off_screen_arrow=true; float head_offset_m=2.2f; };`
  - `void load_config(Config&, const char* path); void save_config(const Config&, const char* path);`
  - `void draw_options(Config&);` (called from arcdps `options_end` callback)
  - `void draw_marker(const Config&, const core::ScreenPoint& anchor, const core::CameraState&, ...);`

- [ ] **Step 1: Write config.hpp**

`src/plugin/config.hpp`:
```cpp
#pragma once
namespace plugin {
enum class MarkerStyle { GroundRing=0, Chevron=1, SilhouetteGlow=2 };
struct Config {
    MarkerStyle style = MarkerStyle::GroundRing;
    float size = 42.0f;
    float opacity = 0.86f;
    float color[3] = {0.0f, 1.0f, 0.78f};
    bool off_screen_arrow = true;
    float head_offset_m = 2.2f;   // world-Y added for head-anchored styles
};
void load_config(Config& c, const char* path);
void save_config(const Config& c, const char* path);
void draw_options(Config& c);       // ImGui options panel
}
```

- [ ] **Step 2: Write config.cpp (simple key=value ini)**

`src/plugin/config.cpp`:
```cpp
#include "config.hpp"
#include "imgui.h"
#include <cstdio>
namespace plugin {

void save_config(const Config& c, const char* path) {
    FILE* f = std::fopen(path, "w"); if (!f) return;
    std::fprintf(f, "style=%d\nsize=%.3f\nopacity=%.3f\n", (int)c.style, c.size, c.opacity);
    std::fprintf(f, "r=%.3f\ng=%.3f\nb=%.3f\narrow=%d\nhead=%.3f\n",
                 c.color[0], c.color[1], c.color[2], c.off_screen_arrow?1:0, c.head_offset_m);
    std::fclose(f);
}

void load_config(Config& c, const char* path) {
    FILE* f = std::fopen(path, "r"); if (!f) return;
    char k[32]; float v;
    while (std::fscanf(f, "%31[^=]=%f\n", k, &v) == 2) {
        std::string key(k);
        if (key=="style") c.style=(MarkerStyle)(int)v;
        else if (key=="size") c.size=v;
        else if (key=="opacity") c.opacity=v;
        else if (key=="r") c.color[0]=v;
        else if (key=="g") c.color[1]=v;
        else if (key=="b") c.color[2]=v;
        else if (key=="arrow") c.off_screen_arrow=(v!=0);
        else if (key=="head") c.head_offset_m=v;
    }
    std::fclose(f);
}

void draw_options(Config& c) {
    const char* styles[] = {"Ground ring", "Chevron (head)", "Silhouette glow"};
    int s = (int)c.style;
    if (ImGui::Combo("Marker style", &s, styles, 3)) c.style=(MarkerStyle)s;
    ImGui::SliderFloat("Size", &c.size, 8.0f, 120.0f, "%.0f px");
    ImGui::SliderFloat("Opacity", &c.opacity, 0.1f, 1.0f, "%.2f");
    ImGui::ColorEdit3("Color", c.color);
    ImGui::Checkbox("Off-screen arrow", &c.off_screen_arrow);
    if (c.style != MarkerStyle::GroundRing)
        ImGui::SliderFloat("Head height", &c.head_offset_m, 0.0f, 3.0f, "%.1f m");
}
}
```
(Add `#include <string>` at top.)

- [ ] **Step 3: Add marker styles to marker.cpp**

Replace `src/plugin/marker.hpp`/`.cpp` `draw_*` with a dispatcher:
```cpp
// marker.hpp
#pragma once
#include "camera.hpp"
#include "config.hpp"
namespace plugin {
void draw_marker(const Config& c, const core::ScreenPoint& anchor);
}
```
```cpp
// marker.cpp
#include "marker.hpp"
#include "imgui.h"
namespace plugin {
static unsigned rgba(const Config& c) {
    return IM_COL32((int)(c.color[0]*255),(int)(c.color[1]*255),
                    (int)(c.color[2]*255),(int)(c.opacity*255));
}
void draw_marker(const Config& c, const core::ScreenPoint& a) {
    if (a.behind) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    unsigned col = rgba(c);
    switch (c.style) {
      case MarkerStyle::GroundRing:
        dl->AddCircle(ImVec2(a.x,a.y), c.size, col, 48, 3.0f);
        dl->AddCircleFilled(ImVec2(a.x,a.y), 3.0f, col, 12); break;
      case MarkerStyle::Chevron: {
        float s=c.size*0.5f;
        dl->AddTriangleFilled(ImVec2(a.x,a.y), ImVec2(a.x-s,a.y-s),
                              ImVec2(a.x+s,a.y-s), col); break; }
      case MarkerStyle::SilhouetteGlow:
        dl->AddCircleFilled(ImVec2(a.x,a.y), c.size, (col & 0x00FFFFFF)|0x30000000, 32);
        dl->AddCircle(ImVec2(a.x,a.y), c.size, col, 40, 2.0f); break;
    }
}
}
```

- [ ] **Step 4: Wire config into dllmain.cpp**

Add a global `plugin::Config g_cfg;`, an ini path resolved next to the DLL (use `GetModuleFileNameA` on the plugin `HMODULE` saved in `DllMain`), call `load_config` in `mod_init`, `save_config` in `mod_release`. Set `g_arc.options_end = (void*)options_cb;` where:
```cpp
static uintptr_t options_cb() { plugin::draw_options(g_cfg); return 0; }
```
In `imgui_cb`, apply head offset for non-ring styles:
```cpp
core::Vec3 anchor = avatar.position;
if (g_cfg.style != plugin::MarkerStyle::GroundRing) anchor.y += g_cfg.head_offset_m;
core::ScreenPoint sp = core::world_to_screen(anchor, cam, sz.x, sz.y);
plugin::draw_marker(g_cfg, sp);
```

- [ ] **Step 5: Cross-compile + in-game check (checkpoint — user)**

Build; confirm options appear under arcdps' extension options, styles switch live, and settings persist across relaunch. **STOP for user confirmation — this is the natural point to request the "big visual changes" you reserved.**

- [ ] **Step 6: Commit**

```bash
git add src/plugin/config.* src/plugin/marker.* src/plugin/dllmain.cpp CMakeLists.txt
git commit -m "feat: config UI, ini persistence, and selectable marker styles"
```

---

## Task 10: Validity gating, distance fade, off-screen arrow

**Files:**
- Modify: `src/plugin/dllmain.cpp`, `src/plugin/marker.hpp`, `src/plugin/marker.cpp`
- Create: `tests/core/test_fade.cpp` (native; the fade curve is pure math moved into core)
- Create: `src/core/fade.hpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `float core::distance_fade(float dist_m, float near_full, float far_zero);` (1.0 when close, 0.0 past far, linear between) — native tested.
- Produces: `void plugin::draw_off_screen_arrow(const core::EdgePoint&, float size, unsigned rgba);`

- [ ] **Step 1: Write the failing fade test**

`tests/core/test_fade.cpp`:
```cpp
#include "doctest.h"
#include "fade.hpp"
using core::distance_fade;
TEST_CASE("fade is full up close, zero far, linear between") {
    CHECK(distance_fade(0, 20, 60)  == doctest::Approx(1.0f));
    CHECK(distance_fade(20, 20, 60) == doctest::Approx(1.0f));
    CHECK(distance_fade(60, 20, 60) == doctest::Approx(0.0f));
    CHECK(distance_fade(40, 20, 60) == doctest::Approx(0.5f));
    CHECK(distance_fade(999, 20, 60)== doctest::Approx(0.0f));
}
```

- [ ] **Step 2: Write fade.hpp**

`src/core/fade.hpp`:
```cpp
#pragma once
namespace core {
inline float distance_fade(float d, float near_full, float far_zero) {
    if (d <= near_full) return 1.0f;
    if (d >= far_zero)  return 0.0f;
    return 1.0f - (d - near_full) / (far_zero - near_full);
}
}
```

- [ ] **Step 3: Add to core_tests, run — expect PASS**

Add `tests/core/test_fade.cpp` to `core_tests`. Run:
`cmake -S . -B build-native >/dev/null && cmake --build build-native 2>&1 | tail -3 && ./build-native/core_tests`
Expected: PASS.

- [ ] **Step 4: Add off-screen arrow renderer to marker.cpp**

`marker.hpp` add: `void draw_off_screen_arrow(const core::EdgePoint& e, float size, unsigned rgba);`
`marker.cpp` add:
```cpp
#include <cmath>
void draw_off_screen_arrow(const core::EdgePoint& e, float size, unsigned rgba) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float a = e.angle_rad;
    ImVec2 tip(e.x + std::cos(a)*size, e.y + std::sin(a)*size);
    ImVec2 l(e.x + std::cos(a+2.5f)*size, e.y + std::sin(a+2.5f)*size);
    ImVec2 r(e.x + std::cos(a-2.5f)*size, e.y + std::sin(a-2.5f)*size);
    dl->AddTriangleFilled(tip, l, r, rgba);
}
```

- [ ] **Step 5: Update imgui_cb to gate + fade + arrow**

In `dllmain.cpp` `imgui_cb`, after computing `cam`/`anchor`:
```cpp
float dist = core::length(cam.position - avatar.position);
float fade = core::distance_fade(dist, 8.0f, 5000.0f); // effectively no far cull; tune later
core::ScreenPoint sp = core::world_to_screen(anchor, cam, sz.x, sz.y);
plugin::Config c = g_cfg; c.opacity *= fade;
if (sp.behind || !sp.on_screen) {
    if (g_cfg.off_screen_arrow) {
        // mirror behind-camera points so the arrow points the right way
        float mx = sp.behind ? (sz.x - sp.x) : sp.x;
        float my = sp.behind ? (sz.y - sp.y) : sp.y;
        core::EdgePoint e = core::clamp_to_edge(mx, my, sz.x, sz.y, 48.0f);
        unsigned col = IM_COL32((int)(c.color[0]*255),(int)(c.color[1]*255),
                                (int)(c.color[2]*255),(int)(c.opacity*255));
        plugin::draw_off_screen_arrow(e, 16.0f, col);
    }
} else {
    plugin::draw_marker(c, sp);
}
```
Include `"fade.hpp"`.

- [ ] **Step 6: Add gating for stale link**

In `MumbleReader::sample`, return false when the link has never ticked:
```cpp
if (mem_->uiTick == 0) return false;
```
(place before reading avatar). Rebuild native tests are unaffected.

- [ ] **Step 7: Cross-compile + final in-game check (checkpoint — user)**

Build; confirm: marker hidden on char-select/loading, off-screen arrow points toward you when camera turns away, distance fade behaves. **STOP for user confirmation.**

- [ ] **Step 8: Commit**

```bash
git add src/core/fade.hpp tests/core/test_fade.cpp src/plugin/ CMakeLists.txt
git commit -m "feat: validity gating, distance fade, and off-screen direction arrow"
```

---

## Self-Review Notes

- **Spec coverage:** persistent self-marker (Tasks 8–9), find-in-crowd via always-on background draw list + off-screen arrow (Tasks 6,10), MumbleLink anchor (Tasks 3,7,8), configurable look incl. a silhouette-glow nod to the north star (Task 9). True render-hook outline is intentionally out of scope per the ToS decision — recorded in Architecture, not a task.
- **Placeholder scan:** every code step contains complete code; no TBD/TODO.
- **Type consistency:** `Vec3`, `Mat4` (col-major), `CameraState`/`AvatarState`, `ScreenPoint{x,y,on_screen,behind}`, `EdgePoint{x,y,angle_rad}`, `Config`, `MarkerStyle` names are used identically across tasks 3→10.
- **Known in-game unknowns (expected, handled):** exact world-axis handedness and screen-Y sign are confirmed in Task 8 Step 5 with a documented one-line fix; head-offset meters (Task 9) tuned live; far-cull distance (Task 10) tuned live. These are the only values that can't be nailed without the game, and each has an explicit tuning checkpoint.
```