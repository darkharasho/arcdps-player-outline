cmake_minimum_required(VERSION 3.20)
project(arcdps_player_outline CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Plugin version from the git tag (v0.1.0 -> 0.1.0). Dev builds get a describe
# string like 0.1.0-3-gabc123. This is what the plugin reports to arcdps and
# keeps it in sync with GitHub releases. Falls back if git/tags are unavailable.
set(PLUGIN_VERSION "0.0.0-dev")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE _po_git_desc
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(_po_git_desc)
        string(REGEX REPLACE "^v" "" PLUGIN_VERSION "${_po_git_desc}")
    endif()
endif()
message(STATUS "player_outline version: ${PLUGIN_VERSION}")

# Dev build: adds the outline probe window and scan log, and
# renames the whole addon (file, arcdps sig, ini) so it can be installed next to
# a release build without colliding with it.
option(PLAYER_OUTLINE_SPIKE "Build the dev draw-call scanner DLL" OFF)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    file(GLOB IMGUI_SRC third_party/imgui/*.cpp)
    add_library(arcdps_player_outline SHARED
        src/plugin/dllmain.cpp
        src/plugin/mumble_link.cpp
        src/plugin/marker.cpp
        src/plugin/config.cpp
        src/plugin/config_io.cpp
        src/core/mumble_data.cpp
        src/core/camera.cpp
        src/spike/replay_tint.cpp
        ${IMGUI_SRC})
    if(PLAYER_OUTLINE_SPIKE)
        set(PLUGIN_OUTPUT_NAME "arcdps_player_outline_dev")
    else()
        set(PLUGIN_OUTPUT_NAME "arcdps_player_outline")
    endif()
    set_target_properties(arcdps_player_outline PROPERTIES
        PREFIX "" OUTPUT_NAME "${PLUGIN_OUTPUT_NAME}")
    target_include_directories(arcdps_player_outline PRIVATE
        third_party/imgui third_party/arcdps src/core src/spike)
    target_compile_definitions(arcdps_player_outline PRIVATE
        PLUGIN_VERSION="${PLUGIN_VERSION}")
    target_link_libraries(arcdps_player_outline PRIVATE d3d11 dxgi)
    if(PLAYER_OUTLINE_SPIKE)
        target_compile_definitions(arcdps_player_outline PRIVATE PLAYER_OUTLINE_SPIKE)
    endif()
    target_link_options(arcdps_player_outline PRIVATE
        -static -static-libgcc -static-libstdc++)
else()
    # ---- Native core test target ----
    add_executable(core_tests
        tests/core/doctest_main.cpp
        tests/core/test_vec3.cpp
        tests/core/test_mumble_parse.cpp
        tests/core/test_camera.cpp
        tests/core/test_one_euro.cpp
        tests/core/test_config.cpp
        tests/core/test_cb_scan.cpp
        tests/core/test_fingerprint.cpp
        src/plugin/config_io.cpp
        src/core/mumble_data.cpp
        src/core/camera.cpp)
    target_include_directories(core_tests PRIVATE
        third_party/doctest src/core src/plugin src/spike)
    enable_testing()
    add_test(NAME core_tests COMMAND core_tests)
endif()
