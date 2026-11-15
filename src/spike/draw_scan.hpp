#pragma once
// THROWAWAY SPIKE CODE. Log-only DX11 instrumentation: hooks DrawIndexed,
// DrawIndexedInstanced and DrawInstanced. Renders nothing, changes no device
// state. Built only when PLAYER_OUTLINE_SPIKE=ON.
//
// Probe 2 (differential identification). Probe 1 scanned vertex-stage constant
// buffers for the avatar's position across seven full frames and found nothing,
// so this one stops looking for a transform. GW2's first-person camera does not
// draw your character: capture third-person frames and first-person frames, and
// the draws present in every third-person frame and absent from every
// first-person frame are your character. See fingerprint.hpp for the pure half.
#include "vec3.hpp"

namespace spike {

// swapchain is arcdps's id3dptr (IDXGISwapChain*, d3dversion==11).
// Returns false if the device/context/vtable patch could not be obtained.
bool install(void* swapchain, const char* log_path);
void shutdown();

// Called once per frame from the imgui callback (same thread as the draw
// hooks). Only supplies context for the log header now; nothing gates on it.
void set_target(const core::Vec3& avatar, const core::Vec3& camera,
                float screen_x, float screen_y, float screen_w, float screen_h);

// Arm a capture of the next frame. third_person tags it as a frame where the
// character IS drawn; false tags a first-person frame where it is not.
void request_capture(bool third_person);

// Diff the captures collected so far and write the verdict to the log.
void analyze();

// Discard all captures and start over.
void reset_captures();

// Called at the end of each frame (from the imgui callback, which arcdps runs
// at Present). Closes an in-progress capture so one capture == one frame.
void end_frame();

const char* last_summary();

}  // namespace spike
