// Manual check of the real network path of the update notice. Not part of
// core_tests: it talks to github.com. Run it with scripts/test-update-check.sh.
//
// Prints the resulting state for the version given on the command line:
//   state=2 (UpToDate), state=3 tag=vX.Y.Z (Available), state=4 (Failed).
#include "update_check.hpp"
#include <windows.h>
#include <cstdio>

int main(int argc, char** argv) {
    plugin::start_update_check(argc > 1 ? argv[1] : "0.0.1");
    for (int i = 0; i < 200; ++i) {
        const plugin::UpdateStatus s = plugin::update_status();
        if (s.state != plugin::UpdateState::Checking) {
            std::printf("state=%d tag=%s\n", (int)s.state, s.tag);
            return 0;
        }
        Sleep(100);
    }
    std::printf("timeout\n");
    return 1;
}
