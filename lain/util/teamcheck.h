#pragma once
#include "classes/classes.h"
#include "globals.h"
#include <unordered_map>
#include <chrono>

namespace teamcheck {

    struct TeamInfo {
        uint64_t address{0};
        std::string name;
        uint32_t color{0};
    };

    // Returns true if both players are detected to be on the same team.
    bool same_team(const roblox::player& a, const roblox::player& b);
    // Returns true if target is considered an enemy of local player.
    inline bool is_enemy_of_local(const roblox::player& target) {
        return !same_team(globals::instances::lp, target);
    }
}

