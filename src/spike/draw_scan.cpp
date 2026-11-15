// THROWAWAY SPIKE CODE — see src/spike/draw_scan.hpp.
#include <windows.h>
#include <d3d11.h>
#include <cstdio>
#include <cstring>
#include "draw_scan.hpp"
#include "fingerprint.hpp"

namespace spike {
namespace {

// ID3D11DeviceContext vtable: [0..2] IUnknown, [3..6] ID3D11DeviceChild,
// then 7 VSSetConstantBuffers, 8 PSSetShaderResources, 9 PSSetShader,
// 10 PSSetSamplers, 11 VSSetShader, 12 DrawIndexed, 13 Draw, 14 Map, 15 Unmap,
// 16 PSSetConstantBuffers, 17 IASetInputLayout, 18 IASetVertexBuffers,
// 19 IASetIndexBuffer, 20 DrawIndexedInstanced, 21 DrawInstanced.
constexpr int kDrawIndexedSlot          = 12;
constexpr int kDrawIndexedInstancedSlot = 20;
constexpr int kDrawInstancedSlot        = 21;

// Up to this many captures of each kind are held for the diff. More
// third-person frames make the "present in all of them" filter stricter.
constexpr int kMaxCaps = 4;
// Cap on how many surviving fingerprints get written out, so a diff that fails
// to discriminate produces a readable log instead of a 16k-line dump.
constexpr int kMaxReported = 60;

using DrawIndexed_t = void (STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, INT);
using DrawIndexedInstanced_t =
    void (STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, UINT, INT, UINT);
using DrawInstanced_t =
    void (STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, UINT, UINT);

const GUID kIID_ID3D11Device =
    {0xdb6f6ddb, 0xac77, 0x4e88, {0x82, 0x53, 0x81, 0x9d, 0xf9, 0xbb, 0xf1, 0x40}};

ID3D11Device*        g_dev  = nullptr;
ID3D11DeviceContext* g_ctx  = nullptr;   // immediate context only
void**               g_vtbl = nullptr;
DrawIndexed_t          g_orig     = nullptr;
DrawIndexedInstanced_t g_orig_dii = nullptr;
DrawInstanced_t        g_orig_di  = nullptr;

char  g_log_path[MAX_PATH] = {0};
FILE* g_log = nullptr;

core::Vec3 g_avatar{}, g_camera{};
float g_sx = 0.0f, g_sy = 0.0f;

// Capture storage. third[] are frames where the character is drawn, first[]
// frames where it is not.
FpTable g_third[kMaxCaps];
FpTable g_first[kMaxCaps];
int  g_n_third = 0, g_n_first = 0;

volatile LONG g_armed = 0;      // 0 idle, 1 arm third-person, 2 arm first-person
bool      g_capturing = false;
bool      g_cap_third = false;
FpTable*  g_target    = nullptr;
bool      g_in_hook   = false;
ULONGLONG g_start_ms  = 0;
char g_summary[192] = "no capture yet";

void update_summary(const char* tail) {
    std::snprintf(g_summary, sizeof(g_summary),
                  "3P:%d 1P:%d | F9=3rd F10=1st F11=analyze F12=reset | %s",
                  g_n_third, g_n_first, tail);
}

void begin_capture(bool third) {
    // Oldest capture of that kind is dropped once the ring is full.
    if (third) {
        if (g_n_third == kMaxCaps) g_n_third = kMaxCaps - 1;
        g_target = &g_third[g_n_third];
    } else {
        if (g_n_first == kMaxCaps) g_n_first = kMaxCaps - 1;
        g_target = &g_first[g_n_first];
    }
    g_target->clear();
    g_capturing = true;
    g_cap_third = third;
    g_start_ms  = GetTickCount64();
}

void finish_capture() {
    if (!g_capturing || !g_target) return;
    ULONGLONG ms = GetTickCount64() - g_start_ms;
    if (g_cap_third) ++g_n_third; else ++g_n_first;
    if (!g_log) g_log = std::fopen(g_log_path, "a");
    if (g_log) {
        std::fprintf(g_log,
            "# %s capture #%d: %d draws, %d unique fingerprints, %llums%s\n"
            "#   avatar=(%.2f,%.2f,%.2f) projects to (%.0f,%.0f)\n",
            g_cap_third ? "THIRD-PERSON" : "FIRST-PERSON",
            g_cap_third ? g_n_third : g_n_first,
            g_target->draws, g_target->used, (unsigned long long)ms,
            g_target->overflow ? "  [TABLE OVERFLOW — results incomplete]" : "",
            g_avatar.x, g_avatar.y, g_avatar.z, g_sx, g_sy);
        std::fflush(g_log);
    }
    char tail[80];
    std::snprintf(tail, sizeof(tail), "%s: %d draws",
                  g_cap_third ? "3P" : "1P", g_target->draws);
    update_summary(tail);
    g_capturing = false;
    g_target = nullptr;
}

void record(ID3D11DeviceContext* ctx, uint8_t kind, UINT count, UINT instances) {
    Fingerprint f;
    f.index_count = count;
    f.instances   = instances;
    f.kind        = kind;

    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader*  ps = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    ctx->VSGetShader(&vs, nullptr, nullptr);
    ctx->PSGetShader(&ps, nullptr, nullptr);
    ctx->OMGetRenderTargets(1, &rtv, nullptr);
    // The pipeline keeps these alive; we only want their identity, not a ref.
    f.vs = vs; f.ps = ps; f.rtv = rtv;
    if (vs) vs->Release();
    if (ps) ps->Release();
    if (rtv) rtv->Release();

    g_target->add(f);
}

// Shared body for all three draw entry points. Only the immediate context is
// inspected, and the hook must never re-enter itself.
void dispatch(ID3D11DeviceContext* ctx, uint8_t kind, UINT count, UINT instances) {
    if (ctx != g_ctx || g_in_hook) return;
    g_in_hook = true;
    if (!g_capturing) {
        LONG arm = InterlockedExchange(&g_armed, 0);
        if (arm != 0) begin_capture(arm == 1);
    }
    if (g_capturing) record(ctx, kind, count, instances);
    g_in_hook = false;
}

void STDMETHODCALLTYPE hk_DrawIndexed(ID3D11DeviceContext* ctx, UINT index_count,
                                      UINT start, INT base) {
    dispatch(ctx, 0, index_count, 1);
    g_orig(ctx, index_count, start, base);
}

void STDMETHODCALLTYPE hk_DrawIndexedInstanced(ID3D11DeviceContext* ctx,
                                               UINT ipi, UINT instances,
                                               UINT start, INT base, UINT start_inst) {
    dispatch(ctx, 1, ipi, instances);
    g_orig_dii(ctx, ipi, instances, start, base, start_inst);
}

void STDMETHODCALLTYPE hk_DrawInstanced(ID3D11DeviceContext* ctx, UINT vpi,
                                        UINT instances, UINT start, UINT start_inst) {
    dispatch(ctx, 2, vpi, instances);
    g_orig_di(ctx, vpi, instances, start, start_inst);
}

// Swap one vtable entry; returns the previous pointer through `saved`.
bool patch_slot(int slot, void* fn, void** saved) {
    DWORD old = 0;
    if (!VirtualProtect(&g_vtbl[slot], sizeof(void*), PAGE_READWRITE, &old))
        return false;
    if (saved) *saved = g_vtbl[slot];
    g_vtbl[slot] = fn;
    VirtualProtect(&g_vtbl[slot], sizeof(void*), old, &old);
    return true;
}

const char* kind_name(uint8_t k) {
    return k == 1 ? "DrawIndexedInstanced" : k == 2 ? "DrawInstanced" : "DrawIndexed";
}

}  // namespace

bool install(void* swapchain, const char* log_path) {
    if (!swapchain || g_orig) return false;
    std::snprintf(g_log_path, sizeof(g_log_path), "%s", log_path ? log_path : "");

    auto* sc = static_cast<IDXGISwapChain*>(swapchain);
    if (FAILED(sc->GetDevice(kIID_ID3D11Device, (void**)&g_dev)) || !g_dev) return false;
    g_dev->GetImmediateContext(&g_ctx);
    if (!g_ctx) { g_dev->Release(); g_dev = nullptr; return false; }

    g_vtbl = *reinterpret_cast<void***>(g_ctx);
    if (!patch_slot(kDrawIndexedSlot, (void*)&hk_DrawIndexed, (void**)&g_orig)) {
        g_ctx->Release(); g_dev->Release();
        g_ctx = nullptr; g_dev = nullptr; g_vtbl = nullptr;
        return false;
    }
    // Best-effort: if either instanced path fails to patch, the probe still
    // works, it just cannot see draws submitted through it.
    patch_slot(kDrawIndexedInstancedSlot, (void*)&hk_DrawIndexedInstanced,
               (void**)&g_orig_dii);
    patch_slot(kDrawInstancedSlot, (void*)&hk_DrawInstanced, (void**)&g_orig_di);
    update_summary("hook installed");
    return true;
}

void shutdown() {
    if (g_vtbl) {
        if (g_orig)     patch_slot(kDrawIndexedSlot, (void*)g_orig, nullptr);
        if (g_orig_dii) patch_slot(kDrawIndexedInstancedSlot, (void*)g_orig_dii, nullptr);
        if (g_orig_di)  patch_slot(kDrawInstancedSlot, (void*)g_orig_di, nullptr);
    }
    g_orig = nullptr; g_orig_dii = nullptr; g_orig_di = nullptr;
    g_capturing = false;
    g_target = nullptr;
    if (g_ctx) { g_ctx->Release(); g_ctx = nullptr; }
    if (g_dev) { g_dev->Release(); g_dev = nullptr; }
    if (g_log) { std::fclose(g_log); g_log = nullptr; }
}

void set_target(const core::Vec3& avatar, const core::Vec3& camera,
                float screen_x, float screen_y, float, float) {
    g_avatar = avatar;
    g_camera = camera;
    g_sx = screen_x;
    g_sy = screen_y;
}

void request_capture(bool third_person) {
    InterlockedExchange(&g_armed, third_person ? 1 : 2);
}

void reset_captures() {
    g_n_third = 0;
    g_n_first = 0;
    for (int i = 0; i < kMaxCaps; ++i) { g_third[i].clear(); g_first[i].clear(); }
    update_summary("reset");
}

void analyze() {
    if (!g_log) g_log = std::fopen(g_log_path, "a");
    if (g_n_third == 0 || g_n_first == 0) {
        update_summary("need at least one 3P and one 1P capture");
        if (g_log) {
            std::fprintf(g_log, "# ANALYZE skipped: 3P=%d 1P=%d (need both)\n\n",
                         g_n_third, g_n_first);
            std::fflush(g_log);
        }
        return;
    }
    int total = 0, printed = 0;
    if (g_log)
        std::fprintf(g_log,
            "# ===== ANALYZE: draws in all %d third-person frames and no "
            "first-person frame =====\n", g_n_third);
    diff(g_third, g_n_third, g_first, g_n_first,
         [&](const Fingerprint& f, int n) {
             ++total;
             if (g_log && printed < kMaxReported) {
                 ++printed;
                 std::fprintf(g_log,
                     "  CAND vs=%p ps=%p rtv=%p count=%u inst=%u x%d via %s\n",
                     f.vs, f.ps, f.rtv, f.index_count, f.instances, n,
                     kind_name(f.kind));
             }
         });
    if (g_log) {
        if (total > printed)
            std::fprintf(g_log, "  ... %d more not shown\n", total - printed);
        std::fprintf(g_log, "# ===== %d candidate draw(s) from %d 3P / %d 1P "
                            "captures =====\n\n", total, g_n_third, g_n_first);
        std::fflush(g_log);
    }
    char tail[80];
    std::snprintf(tail, sizeof(tail), "ANALYZE: %d candidates", total);
    update_summary(tail);
}

void end_frame() {
    if (g_capturing) finish_capture();
}

const char* last_summary() { return g_summary; }

}  // namespace spike
