#pragma once
#include "mumble_data.hpp"
namespace plugin {

// Reads GW2's live MumbleLink block by scanning our own process address space.
// Under Proton the block is populated by GW2 inside Gw2-64.exe (where arcdps —
// and thus this plugin — runs) but is NOT reachable via the named shared-memory
// object, so we locate it with VirtualQuery. See mumble_link.cpp.
class MumbleReader {
public:
    ~MumbleReader();

    // Create + hold the named "MumbleLink" section. On native Windows GW2 only
    // writes MumbleLink if a consumer created the object first; on Proton GW2
    // self-populates its own block and this is harmless (the scan finds that
    // block; our empty section fails the live-block signature). Call once at init.
    void ensure_link_object();

    // Fills avatar/cam from the latest torn-read-safe snapshot.
    // Returns false if no live block is found or the avatar isn't in a map.
    bool sample(core::AvatarState& avatar, core::CameraState& cam);
private:
    void* own_handle_ = nullptr;              // HANDLE to the named section we hold
    void* own_view_ = nullptr;                // mapped view of it
    const core::LinkedMem* link_ = nullptr;   // cached (borrowed) live-block pointer
};

}  // namespace plugin
