#pragma once
#include "mumble_data.hpp"
namespace plugin {
class MumbleReader {
public:
    ~MumbleReader();
    bool open();                          // maps "MumbleLink"
    bool sample(core::AvatarState& avatar, core::CameraState& cam);
private:
    void* handle_ = nullptr;              // HANDLE
    const core::LinkedMem* mem_ = nullptr;
    uint32_t last_tick_ = 0xFFFFFFFF;
};
}
