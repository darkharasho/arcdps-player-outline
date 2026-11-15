// THROWAWAY SPIKE CODE — see src/spike/replay_tint.hpp.
#include <windows.h>
#include <d3d11.h>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "replay_tint.hpp"

namespace spike {
namespace {

// ID3D11DeviceContext vtable slots (see d3d11.h): 12 DrawIndexed,
// 20 DrawIndexedInstanced.
constexpr int kDrawIndexedSlot          = 12;
constexpr int kDrawIndexedInstancedSlot = 20;
constexpr int kMapSlot = 14, kUnmapSlot = 15, kUpdateSubresourceSlot = 48;

// Draws smaller than this are UI quads, particles and full-screen passes.
constexpr UINT kMinIndices = 90;
// A PS constant-buffer slot GW2 is unlikely to use; saved and restored anyway.
constexpr UINT kCbSlot = 13;

// Cylinder standing on the avatar's feet, in metres.
constexpr float kBelow  = 0.25f;

using DrawIndexed_t = void (STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, INT);
using DrawIndexedInstanced_t =
    void (STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, UINT, INT, UINT);
using D3DCompile_t = HRESULT (WINAPI*)(LPCVOID, SIZE_T, LPCSTR, const void*, void*,
                                       LPCSTR, LPCSTR, UINT, UINT,
                                       ID3D10Blob**, ID3D10Blob**);

using Map_t = HRESULT (STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Resource*, UINT,
                                           D3D11_MAP, UINT, D3D11_MAPPED_SUBRESOURCE*);
using Unmap_t = void (STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Resource*, UINT);
using UpdateSubresource_t = void (STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Resource*,
                                                      UINT, const D3D11_BOX*, const void*,
                                                      UINT, UINT);

const GUID kIID_ID3D11Device =
    {0xdb6f6ddb, 0xac77, 0x4e88, {0x82, 0x53, 0x81, 0x9d, 0xf9, 0xbb, 0xf1, 0x40}};

const char kPixelShader[] =
    "cbuffer P : register(b13) {\n"
    "  float4 cam_pos;\n"
    "  float4 cam_right;\n"    // xyz pre-scaled by tan(fov_x / 2)
    "  float4 cam_up;\n"       // xyz pre-scaled by tan(fov_y / 2)
    "  float4 cam_front;\n"
    "  float4 avatar;\n"       // xyz feet, w radius
    "  float4 vp;\n"           // viewport x, y, width, height
    "  float4 misc;\n"         // x metres above feet, y metres below feet
    "  float4 color;\n"
    "};\n"
    "float inside(float3 world) {\n"
    "  float3 d = world - avatar.xyz;\n"
    "  float r2 = d.x * d.x + d.z * d.z;\n"
    "  if (r2 < avatar.w * avatar.w && d.y > -misc.y && d.y < misc.x) return 1.0;\n"
    "  return 0.0;\n"
    "}\n"
    "float in_cyl(float4 p) {\n"
    "  float nx = (p.x - vp.x) / vp.z * 2.0 - 1.0;\n"
    "  float ny = 1.0 - (p.y - vp.y) / vp.w * 2.0;\n"
    "  float3 dir = cam_front.xyz + cam_right.xyz * nx + cam_up.xyz * ny;\n"
    // Probe 3a result: SV_Position.w is view depth in inches.
    "  return inside(cam_pos.xyz + dir * (p.w / 39.3701));\n"
    "}\n"
    "float4 main(float4 p : SV_Position) : SV_Target {\n"
    "  if (in_cyl(p) < 0.5) discard;\n"
    "  return color;\n"
    "}\n"
    // Probe 3b result: with no depth buffer bound, occlusion queries counted
    // pixels the shader had discarded. The counting variant therefore never
    // discards: it writes depth 0 inside the cylinder and 1 outside, and a
    // private depth buffer cleared to 1 with a LESS test does the rejecting.
    "struct CountOut { float4 c : SV_Target; float d : SV_Depth; };\n"
    "CountOut main_count(float4 p : SV_Position) {\n"
    "  CountOut o;\n"
    "  o.c = color;\n"
    "  o.d = 1.0;\n"
    "  if (in_cyl(p) > 0.5) o.d = 0.0;\n"
    "  return o;\n"
    "}\n"
    // Probe 3e: outline pass. The mask holds 1 where a learned piece was
    // redrawn. For this entry point vp.xy = mask pixels per backbuffer pixel,
    // misc.z = thickness in pixels, misc.w = fill opacity.
    "Texture2D mask_tex : register(t0);\n"
    "float4 vs_full(uint id : SV_VertexID) : SV_Position {\n"
    "  float2 uv = float2((id << 1) & 2, id & 2);\n"
    "  return float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);\n"
    "}\n"
    "float4 main_edge(float4 p : SV_Position) : SV_Target {\n"
    "  int2 c = int2(p.xy * vp.xy);\n"
    "  float m = mask_tex.Load(int3(c, 0)).r;\n"
    "  if (m > 0.5) {\n"
    "    if (misc.w <= 0.0) discard;\n"
    "    return float4(color.rgb, misc.w);\n"
    "  }\n"
    "  float mx = 0.0;\n"
    "  for (int i = 0; i < 16; i++) {\n"
    "    float a = i * 0.3926991;\n"
    "    float2 d = float2(cos(a), sin(a)) * misc.z;\n"
    "    mx = max(mx, mask_tex.Load(int3(c + int2(round(d)), 0)).r);\n"
    "    mx = max(mx, mask_tex.Load(int3(c + int2(round(d * 0.5)), 0)).r);\n"
    "  }\n"
    "  if (mx < 0.5) discard;\n"
    "  return float4(color.rgb, 1.0);\n"
    "}\n";

struct Params {
    float cam_pos[4];
    float cam_right[4];
    float cam_up[4];
    float cam_front[4];
    float avatar[4];
    float vp[4];
    float misc[4];
    float color[4];
};

ID3D11Device*        g_dev  = nullptr;
ID3D11DeviceContext* g_ctx  = nullptr;   // immediate context only
void**               g_vtbl = nullptr;
DrawIndexed_t          g_orig     = nullptr;
DrawIndexedInstanced_t g_orig_dii = nullptr;

ID3D11PixelShader* g_ps    = nullptr;
ID3D11Buffer*      g_cb    = nullptr;
ID3D11BlendState*  g_blend = nullptr;
// Counting path, used only during a logged frame.
ID3D11PixelShader*       g_ps_count   = nullptr;
ID3D11BlendState*        g_blend_none = nullptr;   // colour writes off
ID3D11DepthStencilState* g_ds_less    = nullptr;   // passes only inside the cylinder
ID3D11DepthStencilState* g_ds_always  = nullptr;   // passes every rasterised pixel
ID3D11Texture2D*         g_count_tex  = nullptr;
ID3D11DepthStencilView*  g_count_dsv  = nullptr;
UINT g_count_w = 0, g_count_h = 0, g_count_samples = 0;
bool g_count_cleared = false;                      // this logged frame
char g_dump_text[320] = "learning...";

// Probe 3e: outline.
IDXGISwapChain*           g_sc       = nullptr;   // arcdps's, not owned
ID3D11VertexShader*       g_vs_full  = nullptr;
ID3D11PixelShader*        g_ps_edge  = nullptr;
ID3D11RasterizerState*    g_rs_plain = nullptr;
ID3D11Texture2D*          g_mask_tex = nullptr;
ID3D11RenderTargetView*   g_mask_rtv = nullptr;
ID3D11ShaderResourceView* g_mask_srv = nullptr;
UINT  g_mask_w = 0, g_mask_h = 0;
float g_mask_vp_w = 0.0f, g_mask_vp_h = 0.0f;     // viewport the mask was drawn with
bool  g_outline   = true;
float g_thickness = 3.0f;
float g_margin    = 1.2f;   // metres the mask cylinder is wider than the learning one
float g_learn_pct = 85.0f;  // share of a draw that must be inside to be learned
float g_fill      = 0.15f;
float g_rgb[3]    = {0.2f, 1.0f, 0.9f};
int   g_masked = 0;                               // mask draws this frame

// Learned set: draws that sat (almost) wholly inside the cylinder when last
// sampled. A key is shaders + index buffer + index range.
struct Mine { uint64_t key; int ttl; UINT count; uint64_t mk; };
constexpr int kMaxMine = 256;
constexpr int kMineTtl = 8;                       // samples a key survives unseen
Mine g_mine[kMaxMine];
int  g_n_mine = 0;

// Sample frames: every kSampleEvery frames each candidate draw is counted.
struct Sample { ID3D11Query* q_in; ID3D11Query* q_all; uint64_t key; UINT count; uint64_t mk; };
constexpr int kMaxSample   = 2048;
constexpr int kSampleEvery = 20;
Sample g_samp[kMaxSample];                        // queries are pooled, made lazily
int  g_n_samp = 0;
int  g_samp_age = -1;                             // frames since the sample; -1 = none
bool g_sampling = false;
int  g_frame_no = 0;
int  g_last_sampled = 0, g_last_qualified = 0;
bool g_ready = false;      // shader, buffer and blend state all exist

char  g_log_path[MAX_PATH] = {0};
char  g_summary[256] = "probe 3: not installed";

// Frame parameters handed over by the imgui callback.
core::Vec3 g_avatar{}, g_cam_pos{}, g_right{}, g_up{}, g_front{};
float g_tan_y = 0.0f;
float g_bb_w = 0.0f, g_bb_h = 0.0f;
ULONGLONG g_frame_ms = 0;          // when set_frame last ran

float g_radius = 0.6f;
float g_above  = 2.7f;
int   g_skipped_blend = 0;
bool g_in_hook = false;

// Per-frame counters, published to the summary line at the next set_frame.
int g_draws = 0, g_replayed = 0;

void log_line(const char* fmt, ...) {
    if (!g_log_path[0]) return;
    FILE* f = std::fopen(g_log_path, "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(f, fmt, ap);
    va_end(ap);
    std::fputc('\n', f);
    std::fclose(f);
}

void put3(float* dst, const core::Vec3& v, float w) {
    dst[0] = v.x; dst[1] = v.y; dst[2] = v.z; dst[3] = w;
}

void upload_params(ID3D11DeviceContext* ctx, const D3D11_VIEWPORT& vp, const float* rgb,
                   float grow = 0.0f) {
    Params p;
    const float tan_x = g_tan_y * (vp.Width / vp.Height);
    put3(p.cam_pos, g_cam_pos, 0.0f);
    put3(p.cam_right, g_right * tan_x, 0.0f);
    put3(p.cam_up, g_up * g_tan_y, 0.0f);
    put3(p.cam_front, g_front, 0.0f);
    put3(p.avatar, g_avatar, g_radius + grow);
    p.vp[0] = vp.TopLeftX; p.vp[1] = vp.TopLeftY; p.vp[2] = vp.Width; p.vp[3] = vp.Height;
    p.misc[0] = g_above + grow; p.misc[1] = kBelow + grow * 0.5f; p.misc[2] = 0.0f; p.misc[3] = 0.0f;
    p.color[0] = rgb[0]; p.color[1] = rgb[1]; p.color[2] = rgb[2]; p.color[3] = 1.0f;
    ctx->UpdateSubresource(g_cb, 0, nullptr, &p, 0, 0);
}

// Makes sure g_count_dsv matches the colour target's size and sample count.
bool ensure_count_depth(ID3D11RenderTargetView* rtv) {
    ID3D11Resource* res = nullptr;
    rtv->GetResource(&res);
    if (!res) return false;
    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex);
    res->Release();
    if (FAILED(hr) || !tex) return false;
    D3D11_TEXTURE2D_DESC td{};
    tex->GetDesc(&td);
    tex->Release();
    if (g_count_dsv && td.Width == g_count_w && td.Height == g_count_h &&
        td.SampleDesc.Count == g_count_samples)
        return true;

    if (g_count_dsv) { g_count_dsv->Release(); g_count_dsv = nullptr; }
    if (g_count_tex) { g_count_tex->Release(); g_count_tex = nullptr; }
    D3D11_TEXTURE2D_DESC dd{};
    dd.Width = td.Width; dd.Height = td.Height;
    dd.MipLevels = 1; dd.ArraySize = 1;
    dd.Format = DXGI_FORMAT_D32_FLOAT;
    dd.SampleDesc = td.SampleDesc;
    dd.Usage = D3D11_USAGE_DEFAULT;
    dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(g_dev->CreateTexture2D(&dd, nullptr, &g_count_tex))) {
        log_line("# count depth texture %ux%u x%u failed", td.Width, td.Height, td.SampleDesc.Count);
        return false;
    }
    if (FAILED(g_dev->CreateDepthStencilView(g_count_tex, nullptr, &g_count_dsv))) {
        log_line("# count depth view failed");
        g_count_tex->Release(); g_count_tex = nullptr;
        return false;
    }
    g_count_w = td.Width; g_count_h = td.Height; g_count_samples = td.SampleDesc.Count;
    g_count_cleared = false;
    return true;
}

bool ensure_mask(ID3D11RenderTargetView* rtv) {
    ID3D11Resource* res = nullptr;
    rtv->GetResource(&res);
    if (!res) return false;
    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex);
    res->Release();
    if (FAILED(hr) || !tex) return false;
    D3D11_TEXTURE2D_DESC td{};
    tex->GetDesc(&td);
    tex->Release();
    if (g_mask_rtv && td.Width == g_mask_w && td.Height == g_mask_h) return true;

    if (g_mask_srv) { g_mask_srv->Release(); g_mask_srv = nullptr; }
    if (g_mask_rtv) { g_mask_rtv->Release(); g_mask_rtv = nullptr; }
    if (g_mask_tex) { g_mask_tex->Release(); g_mask_tex = nullptr; }
    D3D11_TEXTURE2D_DESC md{};
    md.Width = td.Width; md.Height = td.Height;
    md.MipLevels = 1; md.ArraySize = 1;
    md.Format = DXGI_FORMAT_R8_UNORM;
    md.SampleDesc.Count = 1;
    md.Usage = D3D11_USAGE_DEFAULT;
    md.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(g_dev->CreateTexture2D(&md, nullptr, &g_mask_tex)) ||
        FAILED(g_dev->CreateRenderTargetView(g_mask_tex, nullptr, &g_mask_rtv)) ||
        FAILED(g_dev->CreateShaderResourceView(g_mask_tex, nullptr, &g_mask_srv))) {
        log_line("# mask target %ux%u failed", td.Width, td.Height);
        if (g_mask_srv) { g_mask_srv->Release(); g_mask_srv = nullptr; }
        if (g_mask_rtv) { g_mask_rtv->Release(); g_mask_rtv = nullptr; }
        if (g_mask_tex) { g_mask_tex->Release(); g_mask_tex = nullptr; }
        return false;
    }
    g_mask_w = td.Width; g_mask_h = td.Height;
    const FLOAT zero[4] = {0, 0, 0, 0};
    g_ctx->ClearRenderTargetView(g_mask_rtv, zero);
    log_line("# mask target %ux%u created (scene target has %u samples)",
             td.Width, td.Height, td.SampleDesc.Count);
    return true;
}

// Probe 3g: what does one character's set of draws have in common that another
// character's does not? Logs everything bound to the vertex stage for a draw.
bool g_detail_armed = false, g_detail = false;
int  g_detail_no = 0, g_detail_rows = 0;

void log_detail(ID3D11DeviceContext* ctx, UINT count, UINT start, INT base, uint64_t key) {
    FILE* f = g_log_path[0] ? std::fopen(g_log_path, "a") : nullptr;
    if (!f) return;
    ID3D11DepthStencilState* dss = nullptr; UINT ref = 0;
    ctx->OMGetDepthStencilState(&dss, &ref);
    D3D11_DEPTH_STENCIL_DESC dd{};
    if (dss) { dss->GetDesc(&dd); dss->Release(); }
    ID3D11InputLayout* il = nullptr; ctx->IAGetInputLayout(&il); if (il) il->Release();
    std::fprintf(f, "DRAW #%d seq=%d key=%016llx idx=%u start=%u base=%d stencil=%d ref=%u il=%p\n",
                 g_detail_no, g_detail_rows, (unsigned long long)key, count, start, base,
                 dd.StencilEnable ? 1 : 0, ref, (void*)il);
    ID3D11Buffer* vbs[4] = {}; UINT strides[4] = {}, offs[4] = {};
    ctx->IAGetVertexBuffers(0, 4, vbs, strides, offs);
    for (int i = 0; i < 4; ++i) if (vbs[i]) {
        std::fprintf(f, "  vb%d=%p stride=%u off=%u\n", i, (void*)vbs[i], strides[i], offs[i]);
        vbs[i]->Release();
    }
    ID3D11ShaderResourceView* srvs[4] = {};
    ctx->VSGetShaderResources(0, 4, srvs);
    for (int i = 0; i < 4; ++i) if (srvs[i]) {
        std::fprintf(f, "  vs_srv%d=%p\n", i, (void*)srvs[i]);
        srvs[i]->Release();
    }
    for (int stage = 0; stage < 2; ++stage) {
        ID3D11Buffer* cbs[8] = {};
        if (stage == 0) ctx->VSGetConstantBuffers(0, 8, cbs);
        else            ctx->PSGetConstantBuffers(0, 8, cbs);
        for (int i = 0; i < 8; ++i) {
            if (!cbs[i]) continue;
            D3D11_BUFFER_DESC bd{};
            cbs[i]->GetDesc(&bd);
            std::fprintf(f, "  %s_cb%d=%p size=%u", stage ? "ps" : "vs", i, (void*)cbs[i], bd.ByteWidth);
            D3D11_BUFFER_DESC sd{};
            sd.ByteWidth = bd.ByteWidth;
            sd.Usage = D3D11_USAGE_STAGING;
            sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            ID3D11Buffer* st = nullptr;
            if (bd.ByteWidth <= 65536 && SUCCEEDED(g_dev->CreateBuffer(&sd, nullptr, &st)) && st) {
                ctx->CopyResource(st, cbs[i]);
                D3D11_MAPPED_SUBRESOURCE m{};
                if (SUCCEEDED(ctx->Map(st, 0, D3D11_MAP_READ, 0, &m)) && m.pData) {
                    const auto* b = static_cast<const unsigned char*>(m.pData);
                    uint64_t h = 1469598103934665603ULL;
                    for (UINT k = 0; k < bd.ByteWidth; ++k) { h ^= b[k]; h *= 1099511628211ULL; }
                    std::fprintf(f, " hash=%016llx blocks=", (unsigned long long)h);
                    // One short hash per 64 bytes (a 4x4 matrix), so matrices
                    // shared between otherwise different buffers show up.
                    for (UINT o = 0; o + 64 <= bd.ByteWidth && o < 64 * 12; o += 64) {
                        uint32_t bh = 2166136261u;
                        for (UINT k = 0; k < 64; ++k) { bh ^= b[o + k]; bh *= 16777619u; }
                        std::fprintf(f, "%08x ", bh);
                    }
                    if (stage == 0) {
                        std::fprintf(f, "\n     floats:");
                        const UINT nf = bd.ByteWidth / 4 < 64 ? bd.ByteWidth / 4 : 64;
                        for (UINT k = 0; k < nf; ++k) {
                            float v; std::memcpy(&v, b + k * 4, 4);
                            std::fprintf(f, " %.4g", v);
                        }
                    }
                    ctx->Unmap(st, 0);
                }
                st->Release();
            }
            std::fputc('\n', f);
            cbs[i]->Release();
        }
    }
    std::fclose(f);
    ++g_detail_rows;
}

// Probe 3g result: every character piece's vertex constant buffer 0 holds
// row-major world transforms whose translation is the model's position in
// inches, axes (x, z, -y) relative to MumbleLink. Reading buffers back from
// the GPU per draw is far too slow, so the first floats of each constant
// buffer are copied as the game writes them.
Map_t               g_orig_map    = nullptr;
Unmap_t             g_orig_unmap  = nullptr;
UpdateSubresource_t g_orig_update = nullptr;
constexpr int kCbFloats = 160;
struct CbShadow { ID3D11Resource* res; int n; float f[kCbFloats]; };
constexpr int kCbTable = 1024;                 // power of two
CbShadow g_cbs[kCbTable];
struct OpenMap { ID3D11Resource* res; const void* data; UINT bytes; };
OpenMap g_open[16];
bool  g_use_tf    = true;
float g_tf_radius = 0.5f;                      // metres, horizontal
bool  g_body_only = true;                      // skinned pieces only, no learned fallback
// Probe 3j: gear lock. The transforms are bone positions, so position alone
// lets a neighbour's limb through and loses our own pieces mid-animation.
// Meshes (index buffer + range) seen near the avatar while nothing else is
// close are remembered; once any are known, only those are outlined, at a
// looser distance, nearest instance winning.
struct LockedMesh {
    uint64_t mk; float prev, cur;
    bool notf;            // learned from the cylinder: carries no readable transform
    uint64_t k1;          // full key (with shaders) of this frame's first sighting = pass 1
    bool had2, had2_prev; // seen again with other shaders (pass 2) this / last frame
    int n1, best_i, n2;   // copies seen in pass 1, which of them was nearest, copies seen in pass 2
};
int g_foreign_near = 0, g_quiet = 0, g_p2_rej = 0, g_demoted = 0;
bool g_mounted = false;
int g_rej_range = 0, g_rej_order = 0, g_lost_tf = 0, g_diag_lines = 0;   // probe 3r diagnostics
constexpr float kForeignDist = 1.5f;           // an unknown mesh with no copy this close is someone else's
bool g_nearest = true;                         // of several copies of a mesh, outline the nearest only
constexpr int kMaxLock = 512, kMaxPend = 128, kLearnFrames = 60;
LockedMesh g_lock[kMaxLock];
int   g_n_lock = 0;
uint64_t g_pend[kMaxPend];
int   g_n_pend = 0, g_ring = 0, g_last_ring = 0, g_alone_streak = 0;
bool  g_lock_on = true;
float g_lock_radius = 3.0f;                    // metres a known mesh may be from the avatar
float g_ring_radius = 2.5f;                    // anything skinned this close blocks learning
LockedMesh* find_lock(uint64_t mk) {
    for (int i = 0; i < g_n_lock; ++i)
        if (g_lock[i].mk == mk) return &g_lock[i];
    return nullptr;
}
// Probe 3k: a mesh whose every instance in a frame is further than the lock
// radius belongs to someone else (ours never leaves us). It is never learned,
// so a player who walks up and stands on us stays out.
template <int N> struct KeySet {           // N a power of two; 0 is "empty"
    uint64_t k[N];
    int n;
    bool has(uint64_t v) const {
        for (size_t i = (size_t)(v * 0x9E3779B97F4A7C15ULL >> 40) & (N - 1), p = 0; p < 64;
             ++p, i = (i + 1) & (N - 1)) {
            if (k[i] == v) return true;
            if (!k[i]) return false;
        }
        return false;
    }
    void add(uint64_t v) {
        if (n >= N / 2) return;
        for (size_t i = (size_t)(v * 0x9E3779B97F4A7C15ULL >> 40) & (N - 1), p = 0; p < 64;
             ++p, i = (i + 1) & (N - 1)) {
            if (k[i] == v) return;
            if (!k[i]) { k[i] = v; ++n; return; }
        }
    }
    void clear() { if (n) { std::memset(k, 0, sizeof(k)); n = 0; } }
};
KeySet<8192> g_foreign;
KeySet<2048> g_close;
constexpr int kMaxFar = 1024;
uint64_t g_far[kMaxFar];
int  g_n_far = 0;
bool g_rigid = true;                           // rigid pieces with a transform take part too
constexpr UINT kSkinnedBytes = 4096;           // bone-palette buffers are ~16 KB, rigid ones < 500 B
int   g_cap_map = 0, g_cap_upd = 0, g_tf_found = 0, g_tf_near = 0;

// Constant buffer size in bytes, or 0 for anything else.
UINT cb_bytes(ID3D11Resource* res) {
    D3D11_RESOURCE_DIMENSION dim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    res->GetType(&dim);
    if (dim != D3D11_RESOURCE_DIMENSION_BUFFER) return 0;
    D3D11_BUFFER_DESC bd{};
    static_cast<ID3D11Buffer*>(res)->GetDesc(&bd);
    return (bd.BindFlags & D3D11_BIND_CONSTANT_BUFFER) ? bd.ByteWidth : 0;
}

CbShadow* cb_slot(ID3D11Resource* res, bool create) {
    size_t i = ((uintptr_t)res >> 4) * 0x9E3779B1u & (kCbTable - 1);
    for (int probe = 0; probe < 32; ++probe, i = (i + 1) & (kCbTable - 1)) {
        if (g_cbs[i].res == res) return &g_cbs[i];
        if (!g_cbs[i].res) {
            if (!create) return nullptr;
            g_cbs[i].res = res;
            return &g_cbs[i];
        }
    }
    return nullptr;
}

void cb_store(ID3D11Resource* res, const void* data, UINT bytes) {
    if (!data || bytes < 48) return;
    CbShadow* c = cb_slot(res, true);
    if (!c) return;
    const UINT n = bytes / 4 < (UINT)kCbFloats ? bytes / 4 : (UINT)kCbFloats;
    std::memcpy(c->f, data, n * 4);
    c->n = (int)n;
}

HRESULT STDMETHODCALLTYPE hk_Map(ID3D11DeviceContext* ctx, ID3D11Resource* res, UINT sub,
                                 D3D11_MAP type, UINT flags, D3D11_MAPPED_SUBRESOURCE* out) {
    const HRESULT hr = g_orig_map(ctx, res, sub, type, flags, out);
    if (hr == S_OK && ctx == g_ctx && !g_in_hook && g_use_tf && res && out && sub == 0 &&
        (type == D3D11_MAP_WRITE_DISCARD || type == D3D11_MAP_WRITE ||
         type == D3D11_MAP_WRITE_NO_OVERWRITE)) {
        const UINT bytes = cb_bytes(res);
        if (bytes)
            for (auto& o : g_open)
                if (!o.res) { o = {res, out->pData, bytes}; break; }
    }
    return hr;
}

void STDMETHODCALLTYPE hk_Unmap(ID3D11DeviceContext* ctx, ID3D11Resource* res, UINT sub) {
    if (ctx == g_ctx && sub == 0)
        for (auto& o : g_open)
            if (o.res == res) {
                cb_store(res, o.data, o.bytes);
                ++g_cap_map;
                o.res = nullptr;
                break;
            }
    g_orig_unmap(ctx, res, sub);
}

void STDMETHODCALLTYPE hk_UpdateSubresource(ID3D11DeviceContext* ctx, ID3D11Resource* res,
                                            UINT sub, const D3D11_BOX* box, const void* src,
                                            UINT row_pitch, UINT depth_pitch) {
    if (ctx == g_ctx && !g_in_hook && g_use_tf && res && src && sub == 0 && !box) {
        const UINT bytes = cb_bytes(res);
        if (bytes) { cb_store(res, src, bytes); ++g_cap_upd; }
    }
    g_orig_update(ctx, res, sub, box, src, row_pitch, depth_pitch);
}

// Horizontal distance in metres from the avatar to the nearest world transform
// in the draw's vertex constant buffer 0, or a negative number if none is
// anywhere near.
float transform_distance(ID3D11DeviceContext* ctx, bool* skinned) {
    ID3D11Buffer* cb = nullptr;
    ctx->VSGetConstantBuffers(0, 1, &cb);
    *skinned = false;
    if (!cb) return -1.0f;
    D3D11_BUFFER_DESC bd{};
    cb->GetDesc(&bd);
    *skinned = bd.ByteWidth >= kSkinnedBytes;
    const CbShadow* c = cb_slot(cb, false);
    cb->Release();
    if (!c || c->n < 12) return -1.0f;
    const float k = 39.3701f;
    const float tx = g_avatar.x * k, ty = g_avatar.z * k, tz = -g_avatar.y * k;
    float best = -1.0f;
    for (int o = 0; o + 8 < c->n; ++o) {
        const float dv = c->f[o + 8] - tz;        // negative is up
        if (!(dv > -160.0f && dv < 80.0f)) continue;
        const float dx = c->f[o] - tx, dy = c->f[o + 4] - ty;
        const float d2 = dx * dx + dy * dy;
        if (!(d2 < 400.0f * 400.0f)) continue;
        const float d = std::sqrt(d2) / k;
        if (best < 0.0f || d < best) best = d;
    }
    return best;
}

uint64_t mix(uint64_t h, uint64_t v) {
    h ^= v + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
    return h;
}

bool is_mine(uint64_t key) {
    for (int i = 0; i < g_n_mine; ++i)
        if (g_mine[i].key == key) return true;
    return false;
}

// Runs after the game's own draw. `instanced` picks which original to replay
// through.
void after_draw(ID3D11DeviceContext* ctx, bool instanced, UINT count,
                UINT instances, UINT start, INT base, UINT start_inst) {
    if (ctx != g_ctx || g_in_hook) return;
    ++g_draws;
    if (!g_ready || !g_outline || count < kMinIndices || instances != 1) return;
    if (GetTickCount64() - g_frame_ms > 250) return;
    g_in_hook = true;

    UINT n_vp = 1;
    D3D11_VIEWPORT vp{};
    ctx->RSGetViewports(&n_vp, &vp);

    ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    ID3D11DepthStencilView* dsv = nullptr;
    ctx->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, &dsv);

    // The main scene pass: colour + depth bound, viewport shaped like the
    // backbuffer and at least half its size (covers reduced render scale).
    bool scene = n_vp >= 1 && rtvs[0] && dsv &&
                 vp.Width >= g_bb_w * 0.5f && vp.Height > 1.0f && g_bb_h > 1.0f;
    if (scene) {
        const float a = vp.Width / vp.Height, b = g_bb_w / g_bb_h;
        if (std::fabs(a - b) > 0.02f * b) scene = false;
    }

    // Probe 3b/3c result: only geometry that writes depth. Sprites, particles
    // and glows do not, and they fill the whole cylinder.
    if (scene) {
        ID3D11DepthStencilState* dss = nullptr;
        UINT ref = 0;
        ctx->OMGetDepthStencilState(&dss, &ref);
        if (dss) {
            D3D11_DEPTH_STENCIL_DESC dd{};
            dss->GetDesc(&dd);
            dss->Release();
            if (!dd.DepthEnable || dd.DepthWriteMask != D3D11_DEPTH_WRITE_MASK_ALL) {
                scene = false; ++g_skipped_blend;
            }
        }
    }

    if (scene) {
        ++g_replayed;   // candidates this frame
        ID3D11VertexShader* vs = nullptr;
        ID3D11PixelShader* old_ps = nullptr;
        ID3D11Buffer* ib = nullptr;
        DXGI_FORMAT ib_fmt = DXGI_FORMAT_UNKNOWN;
        UINT ib_off = 0;
        ctx->VSGetShader(&vs, nullptr, nullptr);
        ctx->PSGetShader(&old_ps, nullptr, nullptr);
        ctx->IAGetIndexBuffer(&ib, &ib_fmt, &ib_off);
        uint64_t key = mix(0, (uint64_t)(uintptr_t)vs);
        key = mix(key, (uint64_t)(uintptr_t)old_ps);
        key = mix(key, (uint64_t)(uintptr_t)ib);
        key = mix(key, ((uint64_t)count << 32) | start);
        key = mix(key, ((uint64_t)(uint32_t)base << 32) | ib_off);
        uint64_t mk = mix(1, (uint64_t)(uintptr_t)ib);
        mk = mix(mk, ((uint64_t)count << 32) | start);
        mk = mix(mk, ((uint64_t)(uint32_t)base << 32) | ib_off);
        if (vs) vs->Release();           // identity only
        if (ib) ib->Release();

        // A draw that carries a world transform is judged by where that
        // transform is: instant for newly spawned pieces, and it rejects other
        // characters that merely overlap the cylinder. Draws with no
        // recognisable transform fall back to the learned set.
        //
        // Body only (3i): body and armour are skinned (16 KB bone-palette
        // buffer) and every one logged carried a transform; weapons are rigid
        // and some carry none. So take skinned pieces near the avatar and
        // nothing else: no learned set, no sample frames.
        const bool body_only = g_use_tf && g_body_only;
        const bool lock_mode = body_only && g_lock_on;
        bool accept = !body_only && is_mine(key);
        bool want_sample = !body_only;
        bool tight = false;   // piece with no transform: clip its mask close to the avatar
        if (g_use_tf) {
            bool skinned = false;
            const float d = transform_distance(ctx, &skinned);
            if (d >= 0.0f) {
                ++g_tf_found;
                const bool kind_ok = skinned || g_rigid;
                accept = d <= g_tf_radius && (kind_ok || !body_only);
                if (lock_mode && kind_ok) {
                    LockedMesh* lm = find_lock(mk);
                    if (lm && !lm->notf) {
                        // Probe 3l: nearest instance wins. The scene is drawn
                        // in two passes, all of pass 1 first. Pass 1 only
                        // measures; pass 2 masks the instance that was nearest.
                        if (!lm->k1) lm->k1 = key;
                        if (key == lm->k1) {
                            if (d < lm->cur) { lm->cur = d; lm->best_i = lm->n1; }
                            ++lm->n1;
                            accept = !(g_nearest && lm->had2_prev) && d <= g_lock_radius && d <= lm->prev + 0.25f;
                            if (d > g_lock_radius) {
                                ++g_rej_range;
                                if (g_diag_lines < 300) { ++g_diag_lines;
                                    log_line("# 3p out of range: mesh %016llx idx %u d %.2f pass 1 (copy %d)",
                                             (unsigned long long)mk, count, d, lm->n1); }
                            }
                        } else {
                            lm->had2 = true;
                            // Probe 3m: pass 2's transforms are not the same numbers
                            // as pass 1's while moving, so distances cannot be
                            // compared across passes. One copy: take it. Several:
                            // take the one in the same position in the draw order.
                            const int i2 = lm->n2++;
                            accept = d <= g_lock_radius &&
                                     (!g_nearest || lm->n1 <= 1 || i2 == lm->best_i);
                            if (!accept) {
                                ++g_p2_rej;
                                if (d > g_lock_radius) ++g_rej_range; else ++g_rej_order;
                                if (g_diag_lines < 300) { ++g_diag_lines;
                                    log_line("# 3p rejected: mesh %016llx idx %u d %.2f pass 2 copy %d of pass-1 %d, nearest was %d at %.2f",
                                             (unsigned long long)mk, count, d, i2, lm->n1, lm->best_i, lm->cur); }
                            }
                        }
                    } else if (!lm) {
                        if (d > kForeignDist) { if (g_n_far < kMaxFar) g_far[g_n_far++] = mk; }
                        else g_close.add(mk);
                        const bool foreign = g_foreign.has(mk);
                        if (foreign) {
                            accept = false;
                            if (d <= g_ring_radius) ++g_foreign_near;
                        } else if (d <= g_tf_radius) {
                            bool have = false;
                            for (int i = 0; i < g_n_pend; ++i) if (g_pend[i] == mk) { have = true; break; }
                            if (!have && g_n_pend < kMaxPend) g_pend[g_n_pend++] = mk;
                        } else if (d <= g_ring_radius) {
                            ++g_ring;
                        }
                        if (g_n_lock > 0) accept = false;
                    }
                }
                if (accept) ++g_tf_near;
            } else if (lock_mode) {
                // No readable transform (some head pieces): known ones are
                // drawn, clipped by the mask cylinder; unknown ones are counted
                // against the cylinder on sample frames.
                const LockedMesh* lm = find_lock(mk);
                accept = lm != nullptr;
                tight = lm && lm->notf;
                want_sample = !lm;
                // A known mesh whose transform could not be read this time is
                // still drawn; the mask cylinder keeps distant copies out.
                if (lm && !lm->notf) ++g_lost_tf;
            }
        }
        const bool mine = accept && ensure_mask(rtvs[0]);
        const bool sample = want_sample && g_sampling && g_n_samp < kMaxSample && g_ps_count &&
                            ensure_count_depth(rtvs[0]);
        if (mine || sample) {
            ID3D11Buffer*     old_cb = nullptr;
            ID3D11BlendState* old_bs = nullptr;
            FLOAT old_factor[4] = {0, 0, 0, 0};
            UINT  old_mask = 0xffffffff;
            ctx->PSGetConstantBuffers(kCbSlot, 1, &old_cb);
            ctx->OMGetBlendState(&old_bs, old_factor, &old_mask);

            const float white[3] = {1.0f, 1.0f, 1.0f};
            upload_params(ctx, vp, white);
            const FLOAT factor[4] = {0, 0, 0, 0};
            ctx->PSSetConstantBuffers(kCbSlot, 1, &g_cb);

            if (sample) {
                // Two colourless passes against a private depth buffer: every
                // rasterised pixel, then only the pixels inside the cylinder.
                Sample& sm = g_samp[g_n_samp];
                D3D11_QUERY_DESC qd{D3D11_QUERY_OCCLUSION, 0};
                if (!sm.q_in)  g_dev->CreateQuery(&qd, &sm.q_in);
                if (!sm.q_all) g_dev->CreateQuery(&qd, &sm.q_all);
                if (sm.q_in && sm.q_all) {
                    if (!g_count_cleared) {
                        ctx->ClearDepthStencilView(g_count_dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
                        g_count_cleared = true;
                    }
                    ID3D11DepthStencilState* old_ds = nullptr;
                    UINT old_ref = 0;
                    ctx->OMGetDepthStencilState(&old_ds, &old_ref);

                    ctx->OMSetRenderTargets(1, &rtvs[0], g_count_dsv);
                    ctx->PSSetShader(g_ps_count, nullptr, 0);
                    ctx->OMSetBlendState(g_blend_none, factor, 0xffffffff);

                    ctx->OMSetDepthStencilState(g_ds_always, 0);
                    ctx->Begin(sm.q_all);
                    if (instanced) g_orig_dii(ctx, count, 1, start, base, start_inst);
                    else           g_orig(ctx, count, start, base);
                    ctx->End(sm.q_all);

                    ctx->OMSetDepthStencilState(g_ds_less, 0);
                    ctx->Begin(sm.q_in);
                    if (instanced) g_orig_dii(ctx, count, 1, start, base, start_inst);
                    else           g_orig(ctx, count, start, base);
                    ctx->End(sm.q_in);

                    ctx->OMSetDepthStencilState(old_ds, old_ref);
                    if (old_ds) old_ds->Release();
                    sm.key = key; sm.count = count;
                    sm.mk = mk;
                    ++g_n_samp;
                }
            }
            if (mine && g_detail && g_detail_rows < 400) log_detail(ctx, count, start, base, key);
            if (mine) {
                // Mask only, no depth view: occluded parts are drawn too, which
                // is what makes the outline show through other things. The
                // cylinder test still applies, so someone else wearing the
                // same piece elsewhere on screen is not included.
                // Probe 3e result: clipping the mask to the learning cylinder
                // cut the outline through capes and weapons. The mask uses a
                // roomier cylinder so a whole piece survives.
                upload_params(ctx, vp, white, tight ? 0.2f : g_margin);
                ctx->OMSetRenderTargets(1, &g_mask_rtv, nullptr);
                ctx->OMSetBlendState(nullptr, factor, 0xffffffff);
                ctx->PSSetShader(g_ps, nullptr, 0);
                if (instanced) g_orig_dii(ctx, count, 1, start, base, start_inst);
                else           g_orig(ctx, count, start, base);
                g_mask_vp_w = vp.Width; g_mask_vp_h = vp.Height;
                ++g_masked;
            }

            ctx->PSSetConstantBuffers(kCbSlot, 1, &old_cb);
            ctx->PSSetShader(old_ps, nullptr, 0);
            ctx->OMSetBlendState(old_bs, old_factor, old_mask);
            ctx->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, dsv);
            if (old_cb) old_cb->Release();
            if (old_bs) old_bs->Release();
        }
        if (old_ps) old_ps->Release();
    }

    for (auto* r : rtvs) if (r) r->Release();
    if (dsv) dsv->Release();
    g_in_hook = false;
}

// Draws the outline onto the backbuffer from the mask, then clears the mask.
// Runs at Present time from the imgui callback; every piece of state it
// touches is put back.
void composite() {
    if (!g_mask_rtv) return;
    ID3D11DeviceContext* ctx = g_ctx;
    const FLOAT zero[4] = {0, 0, 0, 0};
    if (g_masked == 0 || !g_outline || !g_sc || !g_vs_full || !g_ps_edge || !g_rs_plain) {
        ctx->ClearRenderTargetView(g_mask_rtv, zero);
        return;
    }
    ID3D11Texture2D* bb = nullptr;
    if (FAILED(g_sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb)) || !bb) return;
    D3D11_TEXTURE2D_DESC bd{};
    bb->GetDesc(&bd);
    ID3D11RenderTargetView* bb_rtv = nullptr;
    g_dev->CreateRenderTargetView(bb, nullptr, &bb_rtv);
    bb->Release();
    if (!bb_rtv) return;

    g_in_hook = true;
    ID3D11RenderTargetView* o_rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    ID3D11DepthStencilView* o_dsv = nullptr;
    ctx->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, o_rtvs, &o_dsv);
    UINT o_nvp = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    D3D11_VIEWPORT o_vps[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    ctx->RSGetViewports(&o_nvp, o_vps);
    ID3D11RasterizerState* o_rs = nullptr;        ctx->RSGetState(&o_rs);
    ID3D11BlendState* o_bs = nullptr; FLOAT o_bf[4]; UINT o_bm = 0;
    ctx->OMGetBlendState(&o_bs, o_bf, &o_bm);
    ID3D11DepthStencilState* o_ds = nullptr; UINT o_ref = 0;
    ctx->OMGetDepthStencilState(&o_ds, &o_ref);
    ID3D11InputLayout* o_il = nullptr;            ctx->IAGetInputLayout(&o_il);
    D3D11_PRIMITIVE_TOPOLOGY o_topo;              ctx->IAGetPrimitiveTopology(&o_topo);
    ID3D11VertexShader* o_vs = nullptr;           ctx->VSGetShader(&o_vs, nullptr, nullptr);
    ID3D11PixelShader* o_ps = nullptr;            ctx->PSGetShader(&o_ps, nullptr, nullptr);
    ID3D11GeometryShader* o_gs = nullptr;         ctx->GSGetShader(&o_gs, nullptr, nullptr);
    ID3D11ShaderResourceView* o_srv = nullptr;    ctx->PSGetShaderResources(0, 1, &o_srv);
    ID3D11Buffer* o_cb = nullptr;                 ctx->PSGetConstantBuffers(kCbSlot, 1, &o_cb);

    Params p{};
    p.vp[0] = g_mask_vp_w / (float)bd.Width;
    p.vp[1] = g_mask_vp_h / (float)bd.Height;
    p.misc[2] = g_thickness * p.vp[1];
    p.misc[3] = g_fill;
    p.color[0] = g_rgb[0]; p.color[1] = g_rgb[1]; p.color[2] = g_rgb[2]; p.color[3] = 1.0f;
    ctx->UpdateSubresource(g_cb, 0, nullptr, &p, 0, 0);

    D3D11_VIEWPORT vp{0.0f, 0.0f, (float)bd.Width, (float)bd.Height, 0.0f, 1.0f};
    const FLOAT factor[4] = {0, 0, 0, 0};
    ctx->OMSetRenderTargets(1, &bb_rtv, nullptr);
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetState(g_rs_plain);
    ctx->OMSetBlendState(g_blend, factor, 0xffffffff);
    ctx->OMSetDepthStencilState(g_ds_always, 0);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(g_vs_full, nullptr, 0);
    ctx->GSSetShader(nullptr, nullptr, 0);
    ctx->PSSetShader(g_ps_edge, nullptr, 0);
    ctx->PSSetShaderResources(0, 1, &g_mask_srv);
    ctx->PSSetConstantBuffers(kCbSlot, 1, &g_cb);
    ctx->Draw(3, 0);

    ctx->PSSetShaderResources(0, 1, &o_srv);
    ctx->PSSetConstantBuffers(kCbSlot, 1, &o_cb);
    ctx->PSSetShader(o_ps, nullptr, 0);
    ctx->GSSetShader(o_gs, nullptr, 0);
    ctx->VSSetShader(o_vs, nullptr, 0);
    ctx->IASetPrimitiveTopology(o_topo);
    ctx->IASetInputLayout(o_il);
    ctx->OMSetDepthStencilState(o_ds, o_ref);
    ctx->OMSetBlendState(o_bs, o_bf, o_bm);
    ctx->RSSetState(o_rs);
    ctx->RSSetViewports(o_nvp, o_vps);
    ctx->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, o_rtvs, o_dsv);
    ctx->ClearRenderTargetView(g_mask_rtv, zero);

    for (auto* r : o_rtvs) if (r) r->Release();
    if (o_dsv) o_dsv->Release();
    if (o_rs) o_rs->Release();
    if (o_bs) o_bs->Release();
    if (o_ds) o_ds->Release();
    if (o_il) o_il->Release();
    if (o_vs) o_vs->Release();
    if (o_ps) o_ps->Release();
    if (o_gs) o_gs->Release();
    if (o_srv) o_srv->Release();
    if (o_cb) o_cb->Release();
    bb_rtv->Release();
    g_in_hook = false;
}

// Reads a finished sample frame and updates the learned set.
void read_samples() {
    for (int i = 0; i < g_n_mine; ++i) --g_mine[i].ttl;
    int qualified = 0;
    for (int i = 0; i < g_n_samp; ++i) {
        Sample& sm = g_samp[i];
        UINT64 in = 0, all = 0;
        if (g_ctx->GetData(sm.q_in, &in, sizeof(in), 0) != S_OK) continue;
        if (g_ctx->GetData(sm.q_all, &all, sizeof(all), 0) != S_OK) continue;
        if (all < 8 || (double)in < 0.01 * g_learn_pct * (double)all) continue;
        ++qualified;
        int j = 0;
        for (; j < g_n_mine; ++j) if (g_mine[j].key == sm.key) break;
        if (j == g_n_mine) {
            if (g_n_mine == kMaxMine) continue;
            ++g_n_mine;
        }
        g_mine[j] = {sm.key, kMineTtl, sm.count, sm.mk};
    }
    for (int i = 0; i < g_n_mine;) {
        if (g_mine[i].ttl <= 0) g_mine[i] = g_mine[--g_n_mine];
        else ++i;
    }
    g_last_sampled = g_n_samp; g_last_qualified = qualified;
    g_n_samp = 0;
}

void STDMETHODCALLTYPE hk_DrawIndexed(ID3D11DeviceContext* ctx, UINT index_count,
                                      UINT start, INT base) {
    g_orig(ctx, index_count, start, base);
    after_draw(ctx, false, index_count, 1, start, base, 0);
}

void STDMETHODCALLTYPE hk_DrawIndexedInstanced(ID3D11DeviceContext* ctx,
                                               UINT ipi, UINT instances,
                                               UINT start, INT base, UINT start_inst) {
    g_orig_dii(ctx, ipi, instances, start, base, start_inst);
    after_draw(ctx, true, ipi, instances, start, base, start_inst);
}

bool patch_slot(int slot, void* fn, void** saved) {
    DWORD old = 0;
    if (!VirtualProtect(&g_vtbl[slot], sizeof(void*), PAGE_READWRITE, &old))
        return false;
    if (saved) *saved = g_vtbl[slot];
    g_vtbl[slot] = fn;
    VirtualProtect(&g_vtbl[slot], sizeof(void*), old, &old);
    return true;
}

// Compiles the pixel shader and creates the buffer and blend state. Any
// failure is logged and leaves g_ready false: the hooks then pass through.
bool create_resources() {
    HMODULE comp = LoadLibraryA("d3dcompiler_47.dll");
    if (!comp) comp = LoadLibraryA("d3dcompiler_43.dll");
    if (!comp) { log_line("# no d3dcompiler dll could be loaded (err %lu)", GetLastError()); return false; }
    auto compile = reinterpret_cast<D3DCompile_t>(
        reinterpret_cast<void*>(GetProcAddress(comp, "D3DCompile")));
    if (!compile) { log_line("# D3DCompile export not found"); return false; }

    auto make_ps = [&](const char* entry, ID3D11PixelShader** out) -> bool {
        ID3D10Blob* code = nullptr;
        ID3D10Blob* err  = nullptr;
        HRESULT chr = compile(kPixelShader, sizeof(kPixelShader) - 1, "probe3_ps", nullptr,
                              nullptr, entry, "ps_4_0", 0, 0, &code, &err);
        if (FAILED(chr) || !code) {
            log_line("# shader %s compile failed hr=0x%08lx: %s", entry, (unsigned long)chr,
                     err ? (const char*)err->GetBufferPointer() : "(no message)");
            if (err) err->Release();
            if (code) code->Release();
            return false;
        }
        if (err) err->Release();
        chr = g_dev->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(),
                                       nullptr, out);
        code->Release();
        if (FAILED(chr)) {
            log_line("# CreatePixelShader %s failed hr=0x%08lx", entry, (unsigned long)chr);
            return false;
        }
        return true;
    };
    if (!make_ps("main", &g_ps)) return false;
    // The counting shader only matters for logged frames; without it the tint
    // still works and a logged frame falls back to having no pixel counts.
    if (!make_ps("main_count", &g_ps_count)) g_ps_count = nullptr;
    if (!make_ps("main_edge", &g_ps_edge)) return false;
    HRESULT hr = S_OK;
    {
        ID3D10Blob* code = nullptr;
        ID3D10Blob* err  = nullptr;
        hr = compile(kPixelShader, sizeof(kPixelShader) - 1, "probe3_vs", nullptr,
                     nullptr, "vs_full", "vs_4_0", 0, 0, &code, &err);
        if (FAILED(hr) || !code) {
            log_line("# vs_full compile failed hr=0x%08lx: %s", (unsigned long)hr,
                     err ? (const char*)err->GetBufferPointer() : "(no message)");
            if (err) err->Release();
            if (code) code->Release();
            return false;
        }
        if (err) err->Release();
        hr = g_dev->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(),
                                       nullptr, &g_vs_full);
        code->Release();
        if (FAILED(hr)) { log_line("# CreateVertexShader failed hr=0x%08lx", (unsigned long)hr); return false; }
    }
    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    hr = g_dev->CreateRasterizerState(&rd, &g_rs_plain);
    if (FAILED(hr)) { log_line("# CreateRasterizerState failed hr=0x%08lx", (unsigned long)hr); return false; }

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof(Params);
    bd.Usage     = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = g_dev->CreateBuffer(&bd, nullptr, &g_cb);
    if (FAILED(hr)) { log_line("# CreateBuffer failed hr=0x%08lx", (unsigned long)hr); return false; }

    D3D11_BLEND_DESC bl{};
    bl.RenderTarget[0].BlendEnable    = TRUE;
    bl.RenderTarget[0].SrcBlend       = D3D11_BLEND_SRC_ALPHA;
    bl.RenderTarget[0].DestBlend      = D3D11_BLEND_INV_SRC_ALPHA;
    bl.RenderTarget[0].BlendOp        = D3D11_BLEND_OP_ADD;
    bl.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ZERO;
    bl.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    bl.RenderTarget[0].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
    bl.RenderTarget[0].RenderTargetWriteMask =
        D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN |
        D3D11_COLOR_WRITE_ENABLE_BLUE;
    hr = g_dev->CreateBlendState(&bl, &g_blend);
    if (FAILED(hr)) { log_line("# CreateBlendState failed hr=0x%08lx", (unsigned long)hr); return false; }

    bl.RenderTarget[0].BlendEnable = FALSE;
    bl.RenderTarget[0].RenderTargetWriteMask = 0;
    D3D11_DEPTH_STENCIL_DESC ds{};
    ds.DepthEnable    = TRUE;
    ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    ds.DepthFunc      = D3D11_COMPARISON_LESS;
    bool count_ok = g_ps_count &&
                    SUCCEEDED(g_dev->CreateBlendState(&bl, &g_blend_none)) &&
                    SUCCEEDED(g_dev->CreateDepthStencilState(&ds, &g_ds_less));
    ds.DepthFunc = D3D11_COMPARISON_ALWAYS;
    count_ok = count_ok && SUCCEEDED(g_dev->CreateDepthStencilState(&ds, &g_ds_always));
    if (!count_ok) {
        log_line("# counting path unavailable; nothing can be learned, so no outline");
        if (g_ps_count) { g_ps_count->Release(); g_ps_count = nullptr; }
    }
    return true;
}

}  // namespace

bool install(void* swapchain, const char* log_path) {
    if (!swapchain || g_orig) return false;
    std::snprintf(g_log_path, sizeof(g_log_path), "%s", log_path ? log_path : "");

    auto* sc = static_cast<IDXGISwapChain*>(swapchain);
    g_sc = sc;
    if (FAILED(sc->GetDevice(kIID_ID3D11Device, (void**)&g_dev)) || !g_dev) {
        std::snprintf(g_summary, sizeof(g_summary), "probe 3: no D3D11 device");
        return false;
    }
    g_dev->GetImmediateContext(&g_ctx);
    if (!g_ctx) { g_dev->Release(); g_dev = nullptr; return false; }

    g_ready = create_resources();
    log_line("# probe 3r install: resources %s", g_ready ? "ok" : "FAILED (see above)");
    if (!g_ready) {
        // Without a shader there is nothing to replay with, so leave the
        // game's vtable untouched.
        std::snprintf(g_summary, sizeof(g_summary),
                      "probe 3: shader setup FAILED, see _scan.log");
        return false;
    }

    g_vtbl = *reinterpret_cast<void***>(g_ctx);
    if (!patch_slot(kDrawIndexedSlot, (void*)&hk_DrawIndexed, (void**)&g_orig)) {
        std::snprintf(g_summary, sizeof(g_summary), "probe 3: vtable patch failed");
        g_vtbl = nullptr;
        return false;
    }
    if (!patch_slot(kDrawIndexedInstancedSlot, (void*)&hk_DrawIndexedInstanced,
                    (void**)&g_orig_dii))
        g_orig_dii = nullptr;
    if (!patch_slot(kMapSlot, (void*)&hk_Map, (void**)&g_orig_map)) g_orig_map = nullptr;
    if (g_orig_map && !patch_slot(kUnmapSlot, (void*)&hk_Unmap, (void**)&g_orig_unmap))
        g_orig_unmap = nullptr;
    if (!patch_slot(kUpdateSubresourceSlot, (void*)&hk_UpdateSubresource, (void**)&g_orig_update))
        g_orig_update = nullptr;
    log_line("# probe 3r hooks: map %d unmap %d update %d", g_orig_map ? 1 : 0,
             g_orig_unmap ? 1 : 0, g_orig_update ? 1 : 0);
    std::snprintf(g_summary, sizeof(g_summary), "probe 3: installed, waiting for MumbleLink");
    return true;
}

void shutdown() {
    if (g_vtbl) {
        if (g_orig)     patch_slot(kDrawIndexedSlot, (void*)g_orig, nullptr);
        if (g_orig_dii) patch_slot(kDrawIndexedInstancedSlot, (void*)g_orig_dii, nullptr);
        if (g_orig_map)    patch_slot(kMapSlot, (void*)g_orig_map, nullptr);
        if (g_orig_unmap)  patch_slot(kUnmapSlot, (void*)g_orig_unmap, nullptr);
        if (g_orig_update) patch_slot(kUpdateSubresourceSlot, (void*)g_orig_update, nullptr);
    }
    g_orig = nullptr; g_orig_dii = nullptr; g_vtbl = nullptr;
    g_orig_map = nullptr; g_orig_unmap = nullptr; g_orig_update = nullptr;
    g_ready = false;
    for (auto& sm : g_samp) {
        if (sm.q_in)  { sm.q_in->Release();  sm.q_in = nullptr; }
        if (sm.q_all) { sm.q_all->Release(); sm.q_all = nullptr; }
    }
    g_n_samp = 0; g_samp_age = -1; g_sampling = false; g_n_mine = 0;
    if (g_mask_srv) { g_mask_srv->Release(); g_mask_srv = nullptr; }
    if (g_mask_rtv) { g_mask_rtv->Release(); g_mask_rtv = nullptr; }
    if (g_mask_tex) { g_mask_tex->Release(); g_mask_tex = nullptr; }
    if (g_rs_plain) { g_rs_plain->Release(); g_rs_plain = nullptr; }
    if (g_ps_edge)  { g_ps_edge->Release();  g_ps_edge = nullptr; }
    if (g_vs_full)  { g_vs_full->Release();  g_vs_full = nullptr; }
    g_sc = nullptr;
    if (g_count_dsv)  { g_count_dsv->Release();  g_count_dsv = nullptr; }
    if (g_count_tex)  { g_count_tex->Release();  g_count_tex = nullptr; }
    if (g_ds_always)  { g_ds_always->Release();  g_ds_always = nullptr; }
    if (g_ds_less)    { g_ds_less->Release();    g_ds_less = nullptr; }
    if (g_blend_none) { g_blend_none->Release(); g_blend_none = nullptr; }
    if (g_ps_count)   { g_ps_count->Release();   g_ps_count = nullptr; }
    if (g_blend) { g_blend->Release(); g_blend = nullptr; }
    if (g_cb)    { g_cb->Release();    g_cb = nullptr; }
    if (g_ps)    { g_ps->Release();    g_ps = nullptr; }
    if (g_ctx)   { g_ctx->Release();   g_ctx = nullptr; }
    if (g_dev)   { g_dev->Release();   g_dev = nullptr; }
}

void set_frame(const core::Vec3& avatar, const core::CameraState& cam,
               float backbuffer_w, float backbuffer_h) {
    // Close out the frame that just finished.
    if (g_ready) composite();
    if (g_sampling) { g_sampling = false; g_samp_age = 0; }
    else if (g_samp_age >= 0 && ++g_samp_age >= 5) { read_samples(); g_samp_age = -1; }
    std::snprintf(g_summary, sizeof(g_summary),
                  "probe 3r %s | draws %d, candidates %d, masked %d | buffers seen map %d upd %d | "
                  "with transform %d, near %d",
                  !g_ready ? "NOT READY" : g_outline ? "ON" : "off", g_draws, g_replayed, g_masked,
                  g_cap_map, g_cap_upd, g_tf_found, g_tf_near);
    g_cap_map = 0; g_cap_upd = 0; g_tf_found = 0; g_tf_near = 0;
    // Gear lock bookkeeping: learn the unknown near meshes once nothing else
    // skinned has been close for kLearnFrames frames in a row.
    // Probe 3o: our own meshes never leave us. A known mesh whose nearest copy
    // this frame was out of range was learned by mistake: drop it for good.
    for (int i = 0; i < g_n_lock;) {
        if (!g_mounted && !g_lock[i].notf && g_lock[i].n1 > 0 && g_lock[i].cur > g_lock_radius) {
            g_foreign.add(g_lock[i].mk); ++g_demoted;
            log_line("# 3p dropped mesh %016llx: nearest copy %.2f m", (unsigned long long)g_lock[i].mk, g_lock[i].cur);
            g_lock[i] = g_lock[--g_n_lock];
        } else ++i;
    }
    for (int i = 0; i < g_n_lock; ++i) {
        LockedMesh& l = g_lock[i];
        l.prev = l.cur; l.cur = 1e9f; l.k1 = 0; l.had2_prev = l.had2; l.had2 = false;
        l.n1 = 0; l.best_i = 0; l.n2 = 0;
    }
    // Pieces with no transform: promote what the cylinder samples found once
    // nobody else has been close for a while.
    // Probe 3q: a mount is too big to judge by distance and would be learned in
    // bits; while mounted the gear set is frozen and only the rider is outlined.
    if (g_mounted) { g_ring = 1; g_n_far = 0; }
    g_quiet = (g_ring == 0) ? g_quiet + 1 : 0;
    if (g_use_tf && g_body_only && g_lock_on && g_quiet >= kLearnFrames) {
        int added = 0;
        for (int i = 0; i < g_n_mine && g_n_lock < kMaxLock; ++i)
            if (g_mine[i].mk && !find_lock(g_mine[i].mk)) {
                LockedMesh l{}; l.mk = g_mine[i].mk; l.prev = l.cur = 1e9f; l.notf = true;
                g_lock[g_n_lock++] = l; ++added;
            }
        if (added) log_line("# gear lock: learned %d pieces without a transform, %d known", added, g_n_lock);
    }
    for (int i = 0; i < g_n_far; ++i)
        if (!g_close.has(g_far[i])) g_foreign.add(g_far[i]);
    g_n_far = 0; g_close.clear();
    if (g_ring == 0 && g_n_pend > 0) {
        if (++g_alone_streak >= kLearnFrames) {
            int added = 0;
            for (int i = 0; i < g_n_pend && g_n_lock < kMaxLock; ++i)
                if (!find_lock(g_pend[i]) && !g_foreign.has(g_pend[i])) {
                    LockedMesh l{}; l.mk = g_pend[i]; l.prev = l.cur = 1e9f; g_lock[g_n_lock++] = l; ++added;
                }
            if (added) log_line("# gear lock: learned %d meshes, %d known, %d foreign", added,
                                g_n_lock, g_foreign.n);
            g_alone_streak = 0;
        }
    } else if (g_ring > 0) {
        g_alone_streak = 0;
    }
    std::snprintf(g_dump_text, sizeof(g_dump_text),
                  "gear: %d known, %d waiting, %d others within %.1f m (+%d seen before), %d foreign, %d dropped%s\n"
                  "this frame: out of range %d, wrong copy %d, no position %d",
                  g_n_lock, g_n_pend, g_ring, g_ring_radius, g_foreign_near, g_foreign.n, g_demoted, g_mounted ? ", MOUNTED (frozen)" : "",
                  g_rej_range, g_rej_order, g_lost_tf);
    {   // Probe 3r: say why nothing is being learned, every ~5 s.
        static int tick = 0;
        if (++tick >= 300) { tick = 0;
            log_line("# 3r state: known %d, waiting %d, others %d, seen-before near %d, foreign %d, dropped %d, mounted %d",
                     g_n_lock, g_n_pend, g_ring, g_foreign_near, g_foreign.n, g_demoted, (int)g_mounted); }
    }
    g_p2_rej = 0; g_rej_range = 0; g_rej_order = 0; g_lost_tf = 0;
    g_last_ring = g_ring; g_n_pend = 0; g_ring = 0; g_foreign_near = 0;
    if (g_detail) {
        g_detail = false;
        log_line("# ===== end detail #%d: %d learned draws, avatar=(%.2f,%.2f,%.2f) camera=(%.2f,%.2f,%.2f) =====\n",
                 g_detail_no, g_detail_rows, g_avatar.x, g_avatar.y, g_avatar.z,
                 g_cam_pos.x, g_cam_pos.y, g_cam_pos.z);
    }
    if (g_detail_armed) { g_detail_armed = false; g_detail = true; g_detail_rows = 0; ++g_detail_no; }
    g_draws = 0; g_replayed = 0; g_skipped_blend = 0; g_masked = 0;
    if (g_ready && g_outline && g_samp_age < 0 && ++g_frame_no % kSampleEvery == 0) {
        g_sampling = true; g_n_samp = 0; g_count_cleared = false;
    }

    // Same basis the marker's look_at uses: left-handed, right = up x front.
    g_front = core::normalized(cam.front);
    g_right = core::normalized(core::cross(cam.up, g_front));
    g_up    = core::cross(g_front, g_right);
    g_cam_pos = cam.position;
    g_avatar  = avatar;
    g_tan_y   = std::tan(cam.fov_y * 0.5f);
    g_bb_w = backbuffer_w; g_bb_h = backbuffer_h;
    g_frame_ms = GetTickCount64();
}

bool*  outline_flag() { return &g_outline; }
float* radius()    { return &g_radius; }
float* height()    { return &g_above; }
float* thickness() { return &g_thickness; }
float* fill()      { return &g_fill; }
float* margin()    { return &g_margin; }
bool*  use_transforms() { return &g_use_tf; }
bool*  body_only() { return &g_body_only; }
bool*  gear_lock() { return &g_lock_on; }
float* gear_radius() { return &g_lock_radius; }
float* learn_range() { return &g_ring_radius; }
void set_mounted(bool m) { g_mounted = m; }
void forget_gear() { g_n_lock = 0; g_alone_streak = 0; g_quiet = 0; g_n_mine = 0; g_foreign.clear(); }
bool*  rigid_pieces() { return &g_rigid; }
bool*  nearest_only() { return &g_nearest; }
float* transform_radius() { return &g_tf_radius; }
float* learn_pct() { return &g_learn_pct; }
float* outline_rgb() { return g_rgb; }
int detail_count() { return g_detail_no; }
void log_learned() {
    g_detail_armed = true;
    log_line("# probe 3e learned set: %d pieces (last sample %d counted, %d inside), radius %.2f",
             g_n_mine, g_last_sampled, g_last_qualified, g_radius);
    for (int i = 0; i < g_n_mine; ++i)
        log_line("  key=%016llx idx=%6u ttl=%d", (unsigned long long)g_mine[i].key,
                 g_mine[i].count, g_mine[i].ttl);
}
const char* last_summary() { return g_summary; }
const char* dump_status() { return g_dump_text; }

}  // namespace spike
