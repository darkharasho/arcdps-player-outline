cmake_minimum_required(VERSION 3.20)
project(arcdps_player_outline CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    file(GLOB IMGUI_SRC third_party/imgui/*.cpp)
    add_library(arcdps_player_outline SHARED
        src/plugin/dllmain.cpp
        src/plugin/mumble_link.cpp
        src/plugin/marker.cpp
        src/plugin/config.cpp
        src/core/mumble_data.cpp
        src/core/camera.cpp
        ${IMGUI_SRC})
    set_target_properties(arcdps_player_outline PROPERTIES
        PREFIX "" OUTPUT_NAME "arcdps_player_outline")
    target_include_directories(arcdps_player_outline PRIVATE
        third_party/imgui third_party/arcdps src/core)
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
        src/core/mumble_data.cpp
        src/core/camera.cpp)
    target_include_directories(core_tests PRIVATE
        third_party/doctest src/core)
    enable_testing()
    add_test(NAME core_tests COMMAND core_tests)
endif()
