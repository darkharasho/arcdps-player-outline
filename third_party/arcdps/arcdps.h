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
