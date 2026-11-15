#pragma once
// THROWAWAY SPIKE CODE. Built only when PLAYER_OUTLINE_SPIKE=ON.
//
// Probe 3 (replay + position gate). Probes 1 and 2 tried to recognise the
// character's draws from GW2's data; this one does not look at GW2's data at
// all. Every scene draw is issued a second time with GW2's own vertex shader
// and OUR pixel shader. The pixel shader rebuilds each pixel's world position
// from SV_Position and the MumbleLink camera, and tints the pixel only if it
// lies inside a cylinder standing on the MumbleLink avatar position.
//
// Results so far: SV_Position.w is view depth in inches; opaque draws that
// land (almost) wholly inside the cylinder are the character's pieces.
//
// Probe 3e: those draws are learned from periodic sample frames, redrawn into
// a mask with no depth test, and the mask's edge is drawn at Present.
#include "mumble_data.hpp"
#include "vec3.hpp"

namespace spike {

// swapchain is arcdps's id3dptr (IDXGISwapChain*, d3dversion==11).
bool install(void* swapchain, const char* log_path);
void shutdown();

// Once per frame from the imgui callback (render thread, at Present).
void set_frame(const core::Vec3& avatar, const core::CameraState& cam,
               float backbuffer_w, float backbuffer_h);

// Controls for the probe window (no keyboard polling).
bool*  outline_flag();
float* radius();         // cylinder radius, metres
float* height();         // cylinder height above the feet, metres
float* thickness();      // outline thickness, pixels
float* margin();         // metres the mask cylinder extends past the learning one
float* learn_pct();      // percent of a draw inside the cylinder to be learned
bool*  use_transforms();   // judge draws by the world transform in their constant buffer
float* transform_radius(); // metres from the avatar a transform may be
bool*  body_only();        // skinned pieces (body + armour) only; weapons ignored
bool*  gear_lock();        // only outline meshes learned while nobody else was close
float* gear_radius();      // metres a learned mesh may be from the avatar
float* learn_range();      // metres; another skinned draw this close blocks learning
void forget_gear();
void set_map(unsigned id, unsigned who);   // map id and character-name hash; a change forgets the gear
void set_log_limit(int lines);     // stop writing the log after this many lines (0 = never)
void set_mounted(bool mounted);   // while mounted nothing is learned, marked or dropped
bool*  nearest_only();     // of several copies of a learned mesh, outline the nearest only
bool*  rigid_pieces();     // let rigid pieces that carry a transform take part
float* fill();           // opacity of the fill inside the outline
float* outline_rgb();    // 3 floats
void log_learned();      // log the learned set and every learned draw's bound state
int detail_count();      // how many times that has been done
const char* last_summary();
const char* dump_status();   // learned-set status line

}  // namespace spike
