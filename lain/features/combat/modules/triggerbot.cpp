#include "../combat.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <limits>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "../../hook.h"
#include <windows.h>
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#include "../../wallcheck/wallcheck.h"
#include "../../../util/classes/math/math.h"
#include "../../../util/console/console.h"
#include "../../../util/teamcheck.h"

using namespace roblox;
using namespace math;

void hooks::triggerbot() {
    HANDLE currentThread = GetCurrentThread();
    SetThreadPriority(currentThread, THREAD_PRIORITY_TIME_CRITICAL);
    SetThreadAffinityMask(currentThread, 1);

    // Cache the Roblox window handle and refresh it periodically to avoid stale/null handles
    static HWND robloxWindow = nullptr;
    auto lastWindowCheck = std::chrono::steady_clock::now();

    while (true) {
        // Skip triggerbot during teleport recovery to prevent auto-targeting
        if (globals::handlingtp || globals::prevent_auto_targeting) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Refresh Roblox window handle every few seconds or if it is null
        auto now = std::chrono::steady_clock::now();
        if (!robloxWindow || std::chrono::duration_cast<std::chrono::seconds>(now - lastWindowCheck).count() > 5) {
            // Try multiple common Roblox window titles for resilience
            robloxWindow = FindWindowA(nullptr, "Roblox");
            if (!robloxWindow) robloxWindow = FindWindowA(nullptr, "RobloxPlayerBeta");
            if (!robloxWindow) robloxWindow = GetForegroundWindow();
            lastWindowCheck = now;
        }

        globals::combat::triggerbotkeybind.update();

        // Consider triggerbot active if the main toggle is on AND
        //  - no key is set, or
        //  - the keybind is currently enabled (toggle/hold/always)
        const bool triggerbotToggleOn = globals::combat::triggerbot;
        const bool keySatisfied = (globals::combat::triggerbotkeybind.key == 0) || globals::combat::triggerbotkeybind.enabled;

        if (!globals::focused || !keybind::is_roblox_focused() || !triggerbotToggleOn || !keySatisfied) {
            if (globals::combat::triggerbot_delay > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            continue;
        }

        // Simplified triggerbot - works when enabled, no aimbot/silent aim requirement
        // This makes it more reliable and responsive

        // Make sure we have a valid Roblox window handle before doing coordinate conversions
        if (!robloxWindow) {
            if (globals::combat::triggerbot_delay > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            continue;
        }

        // Get mouse position
        POINT mouse_point;
        if (!GetCursorPos(&mouse_point) || !ScreenToClient(robloxWindow, &mouse_point)) {
            if (globals::combat::triggerbot_delay > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            continue;
        }

        // Convert mouse to client-space vector
        const Vector2 mouseCursorPos = { (float)mouse_point.x, (float)mouse_point.y };

        // Basic sanity: ensure cursor is within a reasonable client region
        RECT clientRect{};
        if (!GetClientRect(robloxWindow, &clientRect)) {
            if (globals::combat::triggerbot_delay > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            continue;
        }
        if (mouseCursorPos.x < 0 || mouseCursorPos.y < 0 ||
            mouseCursorPos.x > (float)(clientRect.right - clientRect.left) ||
            mouseCursorPos.y > (float)(clientRect.bottom - clientRect.top)) {
            // Mouse not over Roblox client area; skip
            if (globals::combat::triggerbot_delay > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            continue;
        }

        // Get cached players and include spawned bots (NPC targets) to match aimbot behavior
        std::vector<roblox::player> current_players;
        {
            std::lock_guard<std::mutex> lock(globals::instances::cachedplayers_mutex);
            current_players = globals::instances::cachedplayers;
        }
        for (auto bot_head : globals::instances::bots) {
            if (bot_head.is_valid()) {
                roblox::instance npc_model = bot_head.read_parent();
                if (npc_model.is_valid() && npc_model.get_class_name() == "Model") {
                    roblox::instance humanoid = npc_model.findfirstchild("Humanoid");
                    if (humanoid.is_valid() && humanoid.read_health() > 0) {
                        roblox::player bot_player;
                        bot_player.name = "NPC";
                        bot_player.displayname = "NPC";
                        bot_player.head = bot_head;
                        bot_player.humanoid = humanoid;
                        bot_player.hrp = npc_model.findfirstchild("HumanoidRootPart");
                        bot_player.toolname.clear();
                        bot_player.instance = npc_model;
                        bot_player.main = npc_model;
                        bot_player.userid.address = 0;
                        bot_player.health = (int)humanoid.read_health();
                        bot_player.maxhealth = (int)humanoid.read_maxhealth();
                        current_players.push_back(bot_player);
                    }
                }
            }
        }

        if (current_players.empty()) {
            if (globals::combat::triggerbot_delay > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            continue;
        }

        const std::string& localPlayerName = globals::instances::localplayer.get_name();
        bool targetFound = false;

        auto containsAnyKeyword = [](const std::string& value, const std::vector<std::string>& keywords) -> bool {
            if (value.empty())
                return false;
            std::string lower = value;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            for (auto keyword : keywords) {
                std::string key_lower = keyword;
                std::transform(key_lower.begin(), key_lower.end(), key_lower.begin(), ::tolower);
                if (lower.find(key_lower) != std::string::npos) {
                    return true;
                }
            }
            return false;
        };

        auto isToolEquipped = [&](roblox::player player, const std::vector<std::string>& toolNames) -> bool {
            try {
                if (containsAnyKeyword(player.toolname, toolNames)) {
                    return true;
                }

                if (player.instance.is_valid()) {
                    for (const auto& toolName : toolNames) {
                        roblox::instance tool = player.instance.findfirstchild(toolName);
                        if (tool.is_valid() && tool.get_class_name() == "Tool") {
                            return true;
                        }
                    }
                }

                if (player.main.is_valid()) {
                    roblox::instance backpack = player.main.findfirstchild("Backpack");
                    if (backpack.is_valid()) {
                        for (const auto& toolName : toolNames) {
                            roblox::instance tool = backpack.findfirstchild(toolName);
                            if (!tool.is_valid() || tool.get_class_name() != "Tool")
                                continue;

                            roblox::instance parent = tool.read_parent();
                            if (parent.is_valid() && parent.address == player.instance.address) {
                                return true;
                            }
                        }
                    }
                }
            }
            catch (...) {}

            return false;
        };

        const std::vector<std::string> sprayToolNames = { "[SprayCan]", "SprayCan", "Spray" };
        const std::vector<std::string> walletToolNames = { "[Wallet]", "Wallet" };
        const std::vector<std::string> foodToolNames = {
            "[Chicken]", "Chicken",
            "[Cranberry]", "Cranberry",
            "[Pizza]", "Pizza",
            "[Hamburger]", "Hamburger",
            "[Starblox Latte]", "Starblox Latte",
            "[Taco]", "Taco",
            "[Donut]", "Donut",
            "[Cookie]", "Cookie",
            "Food"
        };
        const std::vector<std::string> knifeToolNames = { "[Knife]", "Knife" };

        bool blockLocal = false;
        if (globals::combat::triggerbot_item_checks[0] && isToolEquipped(globals::instances::lp, sprayToolNames)) {
            blockLocal = true;
        }
        if (globals::combat::triggerbot_item_checks[1] && isToolEquipped(globals::instances::lp, walletToolNames)) {
            blockLocal = true;
        }
        if (globals::combat::triggerbot_item_checks[2] && isToolEquipped(globals::instances::lp, foodToolNames)) {
            blockLocal = true;
        }
        if (globals::combat::triggerbot_item_checks[3] && isToolEquipped(globals::instances::lp, knifeToolNames)) {
            blockLocal = true;
        }

        if (blockLocal) {
            if (globals::combat::triggerbot_delay > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            continue;
        }

        // Check if crosshair is over an enemy
        for (auto player : current_players) {
            // Skip if the player is the local player or invalid
            if (!player.head.is_valid() ||
                player.name == localPlayerName ||
                !player.main.is_valid()) {
                continue;
            }

            // Skip if the player is marked as friendly
            if ((globals::bools::player_status.count(player.name) && globals::bools::player_status[player.name]) ||
                (globals::bools::player_status.count(player.displayname) && globals::bools::player_status[player.displayname])) {
                continue;
            }

            // Team check: skip players on the same team as local player when enabled or in Arsenal
            if ((globals::combat::teamcheck || ((*globals::combat::flags)[0] != 0) || (globals::instances::gamename == "Arsenal"))
                && teamcheck::same_team(globals::instances::lp, player)) continue;

            // If "Target Only" is enabled, only target players in the target_only_list
            if (globals::visuals::target_only_esp) {
                bool is_in_target_list = false;
                for (const auto& target_name : globals::visuals::target_only_list) {
                    if (player.name == target_name) {
                        is_in_target_list = true;
                        break;
                    }
                }
                if (!is_in_target_list) {
                    continue;
                }
            }

            // Check if player is alive (Arsenal-aware)
            bool isAlive = true;
            if (globals::instances::gamename == "Arsenal") {
                try {
                    auto nrpbs = player.main.findfirstchild("NRPBS");
                    if (nrpbs.is_valid()) {
                        auto h = nrpbs.findfirstchild("Health");
                        if (h.is_valid()) {
                            double hv = h.read_double_value();
                            if (hv <= 0.0) isAlive = false;
                        }
                    }
                } catch (...) {}
                if (isAlive && player.health <= 0) isAlive = false;
            } else {
                auto humanoid = player.instance.findfirstchild("Humanoid");
                if (!humanoid.is_valid() || humanoid.get_health() <= 0) {
                    isAlive = false;
                }
            }
            if (!isAlive) continue;

            // Check knock status if enabled
            if (globals::combat::knockcheck) {
                auto bodyEffects = player.instance.findfirstchild("BodyEffects");
                if (bodyEffects.is_valid() && bodyEffects.findfirstchild("K.O").read_bool_value()) {
                    continue;
                }
            }

            // Check health threshold if enabled
            if (globals::combat::healthcheck && player.health <= globals::combat::healththreshhold) {
                continue;
            }

            // Item checks: skip targets holding disallowed items
            if (globals::combat::triggerbot_item_checks[0] && isToolEquipped(player, sprayToolNames)) continue;
            if (globals::combat::triggerbot_item_checks[1] && isToolEquipped(player, walletToolNames)) continue;
            if (globals::combat::triggerbot_item_checks[2] && isToolEquipped(player, foodToolNames)) continue;
            if (globals::combat::triggerbot_item_checks[3] && isToolEquipped(player, knifeToolNames)) continue;

            // Check wall visibility if enabled
            if (globals::combat::wallcheck) {
                Vector3 player_head_pos = player.head.get_pos();
                Vector3 current_camera_pos = globals::instances::camera.getPos();
                if (!wallcheck::is_valid_vector(player_head_pos) || !wallcheck::is_valid_vector(current_camera_pos)) {
                    continue;
                }
                if (!wallcheck::can_see(player_head_pos, current_camera_pos)) {
                    continue;
                }
            }

            // Check distance if enabled and keep the value for hitbox sizing
            Vector3 player_head_pos = player.head.get_pos();
            Vector3 current_camera_pos = globals::instances::camera.getPos();
            float distanceToTarget = 0.0f;
            bool distanceValid = false;
            if (wallcheck::is_valid_vector(player_head_pos) && wallcheck::is_valid_vector(current_camera_pos)) {
                distanceToTarget = CalculateDistance1(player_head_pos, current_camera_pos);
                distanceValid = true;
            }
            if (globals::combat::rangecheck) {
                if (!distanceValid || distanceToTarget > globals::combat::aim_distance) {
                    continue;
                }
            }

            // Get screen coordinates of player's head
            // Project key hitboxes and use the closest point for more forgiving detection
            const Vector2 headScreen = roblox::worldtoscreen(player_head_pos);
            const Vector2 hrpScreen = player.hrp.is_valid() ? roblox::worldtoscreen(player.hrp.get_pos()) : Vector2{ -1.0f, -1.0f };
            const Vector2 torsoScreen = hrpScreen; // reuse HRP as torso fallback to avoid missing API

            float bestDistanceSq = std::numeric_limits<float>::max();
            auto considerPoint = [&](const Vector2& pt) {
                if (pt.x == -1.0f || pt.y == -1.0f) return;
                const float dx = mouseCursorPos.x - pt.x;
                const float dy = mouseCursorPos.y - pt.y;
                const float dist2 = dx * dx + dy * dy;
                if (dist2 < bestDistanceSq) bestDistanceSq = dist2;
            };

            considerPoint(headScreen);
            considerPoint(hrpScreen);
            considerPoint(torsoScreen);

            // Dynamic crosshair radius: larger at range for easier hits, clamped for precision up close
            const float baseRadius = 14.0f;
            const float distanceBoost = distanceValid ? (std::min)(distanceToTarget * 0.035f, 46.0f) : 0.0f;
            float crosshairRadius = baseRadius + distanceBoost;
            if (crosshairRadius < baseRadius) crosshairRadius = baseRadius;
            if (crosshairRadius > 60.0f) crosshairRadius = 60.0f;

            if (bestDistanceSq <= crosshairRadius * crosshairRadius) {
                targetFound = true;
                break;
            }
        }

        // If target found, simulate mouse click after delay
        if (targetFound) {
            // Only apply delay if it's greater than 0
            if (globals::combat::triggerbot_delay > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds((int)globals::combat::triggerbot_delay));
            }

            // Simulate left mouse button click
            if (globals::combat::triggerbot_delay <= 0) {
                // Ultra-rapidfire for third-person: send two clicks in one batch
                INPUT inputs[4] = {};
                inputs[0].type = INPUT_MOUSE; inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                inputs[1].type = INPUT_MOUSE; inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
                inputs[2].type = INPUT_MOUSE; inputs[2].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                inputs[3].type = INPUT_MOUSE; inputs[3].mi.dwFlags = MOUSEEVENTF_LEFTUP;
                SendInput(4, inputs, sizeof(INPUT));
            } else {
                // Respect delay path: hold briefly between down and up
                INPUT input_down = { 0 };
                input_down.type = INPUT_MOUSE;
                input_down.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                SendInput(1, &input_down, sizeof(input_down));

                std::this_thread::sleep_for(std::chrono::milliseconds(10));

                INPUT input_up = { 0 };
                input_up.type = INPUT_MOUSE;
                input_up.mi.dwFlags = MOUSEEVENTF_LEFTUP;
                SendInput(1, &input_up, sizeof(input_up));
            }

            // Only add cooldown if delay is > 0; use the same delay value
            if (globals::combat::triggerbot_delay > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds((int)globals::combat::triggerbot_delay));
            }
        }

        // Reduce main loop delay for faster response
        if (globals::combat::triggerbot_delay > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(50)); // Normal speed when delay > 0
        } else {
            // No sleep at all when delay = 0 for maximum speed
            // std::this_thread::sleep_for(std::chrono::microseconds(1)); // Commented out for instant response
        }
    }
}
