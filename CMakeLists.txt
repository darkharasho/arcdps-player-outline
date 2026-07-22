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
