#pragma once
// Update notice: asks GitHub once which release is the latest. It never
// downloads or installs anything.

namespace plugin {

enum class UpdateState { Idle, Checking, UpToDate, Available, Failed };

struct UpdateStatus {
    UpdateState state = UpdateState::Idle;
    char        tag[32] = "";      // latest release tag, set when Available
};

// Starts the one background check. Does nothing if it was already started.
void start_update_check(const char* current_version);

// Thread-safe copy of the current status.
UpdateStatus update_status();

}  // namespace plugin
