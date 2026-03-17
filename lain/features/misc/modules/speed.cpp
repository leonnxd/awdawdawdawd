#include "../../hook.h"
#include "../../../util/console/console.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <random>
#include "../../../util/classes/math/math.h"

void hooks::speed() {
    static float original_walkspeed = 16.0f;
    static bool walkspeed_modified = false;

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5)); // Small sleep to prevent busy-waiting

        globals::misc::speedkeybind.update();

        if (!globals::focused || !keybind::is_roblox_focused()) {
            if (walkspeed_modified) {
                globals::instances::lp.humanoid.write_walkspeed(original_walkspeed);
                walkspeed_modified = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (globals::misc::speed && globals::misc::speedkeybind.enabled) {
            if (!walkspeed_modified) {
                original_walkspeed = globals::instances::lp.humanoid.read_walkspeed();
                walkspeed_modified = true;
            }

            switch (globals::misc::speedtype) {
                case 0: { // WalkSpeed
                    auto hrp = globals::instances::lp.hrp; // Get hrp for move_dir
                    math::Vector3 move_dir = hrp.get_move_dir();
                    // Apply speed if the feature is active, regardless of movement direction
                    globals::instances::lp.humanoid.write_walkspeed(globals::misc::speedvalue);
                    break;
                }
            } // Correctly close switch
        } else { // Correctly close if and open else
            if (walkspeed_modified) {
                globals::instances::lp.humanoid.write_walkspeed(original_walkspeed);
                walkspeed_modified = false;
            }
        }
    }
}