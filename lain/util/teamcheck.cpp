#include "teamcheck.h"
#include "classes/classes.h"
#include "globals.h"
#include "offsets.h"

using namespace roblox;

namespace teamcheck {
    static std::chrono::steady_clock::time_point last_scan;
    static std::unordered_map<uint64_t, TeamInfo> teams_by_addr; // key: team instance address

    static void refresh_teams_cache() {
        auto now = std::chrono::steady_clock::now();
        if (!teams_by_addr.empty() && std::chrono::duration_cast<std::chrono::seconds>(now - last_scan).count() < 2) {
            return;
        }
        last_scan = now;
        teams_by_addr.clear();

        // Traverse DataModel -> Teams
        roblox::instance datamodel = globals::instances::datamodel;
        if (!datamodel.is_valid()) return;

        auto teams_root = datamodel.findfirstchild("Teams");
        if (!teams_root.is_valid()) return;

        for (auto team_inst : teams_root.get_children()) {
            if (!team_inst.is_valid()) continue;
            TeamInfo info;
            info.address = team_inst.address;
            info.name = team_inst.get_name();
            // TeamColor is stored on the Team instance
            info.color = read<uint32_t>(team_inst.address + offsets::TeamColor);
            teams_by_addr[team_inst.address] = info;
        }
    }

    static bool compare_by_team_instance(const roblox::player& a, const roblox::player& b) {
        if (!is_valid_address(a.team.address) || !is_valid_address(b.team.address)) return false;
        if (a.team.address == b.team.address) return true;
        // If different addresses, compare names as a secondary signal
        // get_name() expects a non-const instance; copy before calling
        roblox::instance a_team_copy = a.team;
        roblox::instance b_team_copy = b.team;
        std::string an = a_team_copy.get_name();
        std::string bn = b_team_copy.get_name();
        return (!an.empty() && an == bn);
    }

    static bool compare_by_team_color(const roblox::player& a, const roblox::player& b) {
        refresh_teams_cache();

        uint32_t a_color = 0, b_color = 0;
        // Prefer reading color from Team instance if valid
        if (is_valid_address(a.team.address)) {
            a_color = read<uint32_t>(a.team.address + offsets::TeamColor);
        }
        if (is_valid_address(b.team.address)) {
            b_color = read<uint32_t>(b.team.address + offsets::TeamColor);
        }

        // If still zero, try cached map by address
        if (a_color == 0 && teams_by_addr.count(a.team.address)) a_color = teams_by_addr[a.team.address].color;
        if (b_color == 0 && teams_by_addr.count(b.team.address)) b_color = teams_by_addr[b.team.address].color;

        // As an absolute fallback (no Team instance), read color from player main using offsets::TeamColor
        if (a_color == 0 && is_valid_address(a.main.address)) a_color = read<uint32_t>(a.main.address + offsets::TeamColor);
        if (b_color == 0 && is_valid_address(b.main.address)) b_color = read<uint32_t>(b.main.address + offsets::TeamColor);

        return (a_color != 0 && b_color != 0 && a_color == b_color);
    }

    bool same_team(const roblox::player& a, const roblox::player& b) {
        // Explicit friend map: treat as same team
        if ((globals::bools::player_status.count(b.name) && globals::bools::player_status[b.name]) ||
            (globals::bools::player_status.count(b.displayname) && globals::bools::player_status[b.displayname])) {
            return true;
        }

        // Primary: Use Team instance/Name
        if (compare_by_team_instance(a, b)) return true;

        // Secondary: Compare colors (Team instance or fallback)
        if (compare_by_team_color(a, b)) return true;

        return false;
    }
}
