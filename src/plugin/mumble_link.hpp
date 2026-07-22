#pragma once
#include "mumble_data.hpp"
namespace plugin {

// Reads GW2's live MumbleLink block by scanning our own process address space.
// Under Proton the block is populated by GW2 inside Gw2-64.exe (where arcdps —
// and thus this plugin — runs) but is NOT reachable via the named shared-memory
// object, so we locate it with VirtualQuery. See mumble_link.cpp.
class MumbleReader {
public:
    // Fills avatar/cam from the latest torn-read-safe snapshot.
    // Returns false if no live block is found or the avatar isn't in a map.
    bool sample(core::AvatarState& avatar, core::CameraState& cam);
private:
    const core::LinkedMem* link_ = nullptr;   // cached block pointer
};

}  // namespace plugin
