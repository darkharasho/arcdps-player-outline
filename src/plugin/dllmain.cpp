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
