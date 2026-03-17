#include "../combat.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <random>
#include "../../../util/driver/driver.h"
#include "../../hook.h"
#include <windows.h>
#include "../../wallcheck/wallcheck.h"
#include "../../../util/classes/math/math.h"
#include "../combat.h" // Include combat.h for the function declaration
#include "../../../util/console/console.h"
#include "../../../util/teamcheck.h"

#define max
#undef max
#define min
#undef min

using namespace roblox;
using namespace math;
static bool foundTarget = false;

struct MouseSettings {
    float baseDPI = 800.0f;
    float currentDPI = 800.0f;
    float dpiScaleFactor = 1.0f;
    bool dpiAutoDetected = false;

    void updateDPIScale() {
        dpiScaleFactor = baseDPI / currentDPI;
    }

    float getDPIAdjustedSensitivity() const {
        return dpiScaleFactor;
    }
} mouseSettings;

float CalculateDistance(Vector2 first, Vector2 sec) {
    return sqrt(pow(first.x - sec.x, 2) + pow(first.y - sec.y, 2));
}


Vector3 lerp_vector3(const Vector3& a, const Vector3& b, float t) {
    return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
}

namespace math {
    Vector3 normalize(const Vector3& vec) {
        float length = sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
        return (length != 0) ? Vector3{ vec.x / length, vec.y / length, vec.z / length } : vec;
    }

    Vector3 crossProduct(const Vector3& vec1, const Vector3& vec2) {
        return {
            vec1.y * vec2.z - vec1.z * vec2.y,
            vec1.z * vec2.x - vec1.x * vec2.z,
            vec1.x * vec2.y - vec1.y * vec2.x
        };
    }

    Matrix3 LookAtToMatrix(const Vector3& cameraPosition, const Vector3& targetPosition) {
        Vector3 forward = normalize(Vector3{ (targetPosition.x - cameraPosition.x), (targetPosition.y - cameraPosition.y), (targetPosition.z - cameraPosition.z) });
        Vector3 right = normalize(crossProduct({ 0, 1, 0 }, forward));
        Vector3 up = crossProduct(forward, right);

        Matrix3 lookAtMatrix{};
        lookAtMatrix.data[0] = -right.x;  lookAtMatrix.data[1] = up.x;  lookAtMatrix.data[2] = -forward.x;
        lookAtMatrix.data[3] = right.y;  lookAtMatrix.data[4] = up.y;  lookAtMatrix.data[5] = -forward.y;
        lookAtMatrix.data[6] = -right.z;  lookAtMatrix.data[7] = up.z;  lookAtMatrix.data[8] = -forward.z;

        return lookAtMatrix;
    }
}

// Helper function for drawing a rotated N-gon with optional fill (moved from overlay.cpp)
static void AddRotatedNgon(ImDrawList* draw_list, ImVec2 center, float radius, ImU32 outline_color, int num_segments, float thickness, float angle_offset = 0.0f, bool fill = false, ImU32 fill_color = 0) {
    if (num_segments <= 2) return;
    std::vector<ImVec2> points(num_segments);
    for (int i = 0; i < num_segments; i++) {
        float angle = angle_offset + (float)i * (2 * IM_PI / num_segments);
        points[i] = ImVec2(center.x + cos(angle) * radius, center.y + sin(angle) * radius);
    }
    if (fill && fill_color != 0) {
        draw_list->AddConvexPolyFilled(points.data(), num_segments, fill_color);
    }
    draw_list->AddPolyline(points.data(), num_segments, outline_color, ImDrawFlags_Closed, thickness);
}

// Generate a random 2D shake offset within the configured ranges
static Vector2 get_shake_offset(bool enabled, float range_x, float range_y) {
    if (!enabled || (range_x == 0.0f && range_y == 0.0f)) return { 0.0f, 0.0f };

    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist_x(-range_x, range_x);
    std::uniform_real_distribution<float> dist_y(-range_y, range_y);
    return { dist_x(rng), dist_y(rng) };
}

// Offset a world position using camera-oriented shake (right/up relative to camera)
static Vector3 apply_world_shake(const Vector3& target, const Vector3& camera_pos, const Vector2& shake) {
    if (shake.x == 0.0f && shake.y == 0.0f) return target;

    Vector3 forward = math::normalize({ target.x - camera_pos.x, target.y - camera_pos.y, target.z - camera_pos.z });
    Vector3 right = math::normalize(math::crossProduct({ 0, 1, 0 }, forward));
    Vector3 up = math::crossProduct(forward, right);

    Vector3 offset = {
        right.x * shake.x + up.x * shake.y,
        right.y * shake.x + up.y * shake.y,
        right.z * shake.x + up.z * shake.y
    };
    return AddVector3(target, offset);
}

namespace combat {
    void handle_fov_spinning(bool is_aimbot, float speed) {
        HWND robloxWindow = FindWindowA(0, "Roblox");
        if (!robloxWindow) return;

        POINT cursor_pos;
        GetCursorPos(&cursor_pos);
        ScreenToClient(robloxWindow, &cursor_pos);
        ImVec2 mousepos = ImVec2((float)cursor_pos.x, (float)cursor_pos.y);
        ImDrawList* drawbglist = ImGui::GetBackgroundDrawList();

        float current_time = ImGui::GetTime();
        float rotation_angle = current_time * speed;

        if (is_aimbot) {
            ImVec4 fov_color_vec = ImVec4(globals::combat::fovcolor[0], globals::combat::fovcolor[1], globals::combat::fovcolor[2], globals::combat::fovtransparency);
            ImU32 fov_color_with_transparency = ImGui::ColorConvertFloat4ToU32(fov_color_vec);
            ImU32 fov_fill_color = 0;
            if (globals::combat::fovfill) {
                ImVec4 fill_vec = ImVec4(globals::combat::fovfillcolor[0], globals::combat::fovfillcolor[1], globals::combat::fovfillcolor[2], globals::combat::fovfillcolor[3]);
                float fill_alpha_scale = ImClamp(globals::combat::fovfilltransparency, 0.0f, 1.0f);
                fill_vec.w *= fill_alpha_scale;
                fov_fill_color = ImGui::ColorConvertFloat4ToU32(fill_vec);
            }

            switch (globals::combat::fovshape) {
            case 0: // Circle
                if (globals::combat::fovfill && fov_fill_color != 0) {
                    drawbglist->AddCircleFilled(mousepos, globals::combat::fovsize, fov_fill_color);
                }
                drawbglist->AddCircle(mousepos, globals::combat::fovsize, fov_color_with_transparency, globals::combat::fovthickness);
                break;
            case 1: // Square
                AddRotatedNgon(drawbglist, mousepos, globals::combat::fovsize, fov_color_with_transparency, 4, globals::combat::fovthickness, rotation_angle + IM_PI / 4.0f, globals::combat::fovfill, fov_fill_color);
                break;
            case 2: // Triangle
                AddRotatedNgon(drawbglist, mousepos, globals::combat::fovsize, fov_color_with_transparency, 3, globals::combat::fovthickness, rotation_angle - IM_PI / 2.0f, globals::combat::fovfill, fov_fill_color);
                break;
            case 3: // Pentagon
                AddRotatedNgon(drawbglist, mousepos, globals::combat::fovsize, fov_color_with_transparency, 5, globals::combat::fovthickness, rotation_angle - IM_PI / 2.0f, globals::combat::fovfill, fov_fill_color);
                break;
            case 4: // Hexagon
                AddRotatedNgon(drawbglist, mousepos, globals::combat::fovsize, fov_color_with_transparency, 6, globals::combat::fovthickness, rotation_angle, globals::combat::fovfill, fov_fill_color);
                break;
            case 5: // Octagon
                AddRotatedNgon(drawbglist, mousepos, globals::combat::fovsize, fov_color_with_transparency, 8, globals::combat::fovthickness, rotation_angle, globals::combat::fovfill, fov_fill_color);
                break;
            }
        } else { // Silent Aim FOV
            ImVec4 silent_aim_fov_color_vec = ImVec4(globals::combat::silentaimfovcolor[0], globals::combat::silentaimfovcolor[1], globals::combat::silentaimfovcolor[2], globals::combat::silentaimfovtransparency);
            ImU32 silent_aim_fov_color_with_transparency = ImGui::ColorConvertFloat4ToU32(silent_aim_fov_color_vec);
            ImU32 silent_aim_fov_fill_color = 0;
            if (globals::combat::silentaimfovfill) {
                ImVec4 fill_vec = ImVec4(globals::combat::silentaimfovfillcolor[0], globals::combat::silentaimfovfillcolor[1], globals::combat::silentaimfovfillcolor[2], globals::combat::silentaimfovfillcolor[3]);
                float fill_alpha_scale = ImClamp(globals::combat::silentaimfovfilltransparency, 0.0f, 1.0f);
                fill_vec.w *= fill_alpha_scale;
                silent_aim_fov_fill_color = ImGui::ColorConvertFloat4ToU32(fill_vec);
            }

            switch (globals::combat::silentaimfovshape) {
            case 0: // Circle
                if (globals::combat::silentaimfovfill && silent_aim_fov_fill_color != 0) {
                    drawbglist->AddCircleFilled(mousepos, globals::combat::silentaimfovsize, silent_aim_fov_fill_color);
                }
                drawbglist->AddCircle(mousepos, globals::combat::silentaimfovsize, silent_aim_fov_color_with_transparency, globals::combat::silentaimfovthickness);
                break;
            case 1: // Square
                AddRotatedNgon(drawbglist, mousepos, globals::combat::silentaimfovsize, silent_aim_fov_color_with_transparency, 4, globals::combat::silentaimfovthickness, rotation_angle + IM_PI / 4.0f, globals::combat::silentaimfovfill, silent_aim_fov_fill_color);
                break;
            case 2: // Triangle
                AddRotatedNgon(drawbglist, mousepos, globals::combat::silentaimfovsize, silent_aim_fov_color_with_transparency, 3, globals::combat::silentaimfovthickness, rotation_angle - IM_PI / 2.0f, globals::combat::silentaimfovfill, silent_aim_fov_fill_color);
                break;
            case 3: // Pentagon
                AddRotatedNgon(drawbglist, mousepos, globals::combat::silentaimfovsize, silent_aim_fov_color_with_transparency, 5, globals::combat::silentaimfovthickness, rotation_angle - IM_PI / 2.0f, globals::combat::silentaimfovfill, silent_aim_fov_fill_color);
                break;
            case 4: // Hexagon
                AddRotatedNgon(drawbglist, mousepos, globals::combat::silentaimfovsize, silent_aim_fov_color_with_transparency, 6, globals::combat::silentaimfovthickness, rotation_angle, globals::combat::silentaimfovfill, silent_aim_fov_fill_color);
                break;
            case 5: // Octagon
                AddRotatedNgon(drawbglist, mousepos, globals::combat::silentaimfovsize, silent_aim_fov_color_with_transparency, 8, globals::combat::silentaimfovthickness, rotation_angle, globals::combat::silentaimfovfill, silent_aim_fov_fill_color);
                break;
            }
        }
    }
}

bool detectMouseDPI() {
    HWND robloxWindow = FindWindowA(0, "Roblox");
    if (!robloxWindow) return false;

    HDC hdc = GetDC(robloxWindow);
    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
    int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(robloxWindow, hdc);

    HKEY hkey;
    DWORD sensitivity = 10;
    DWORD size = sizeof(DWORD);

    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Mouse", 0, KEY_READ, &hkey) == ERROR_SUCCESS) {
        RegQueryValueExW(hkey, L"MouseSensitivity", NULL, NULL, (LPBYTE)&sensitivity, &size);
        RegCloseKey(hkey);
    }

    float estimatedDPI = 800.0f * (sensitivity / 10.0f) * (dpiX / 96.0f);
    mouseSettings.currentDPI = std::max(400.0f, std::min(3200.0f, estimatedDPI));
    mouseSettings.updateDPIScale();
    mouseSettings.dpiAutoDetected = true;

    return true;
}

void performCameraAimbot(const Vector3& targetPos, const Vector3& cameraPos) {
    roblox::camera camera = globals::instances::camera;
    Matrix3 currentRotation = camera.getRot();
    Matrix3 targetMatrix = LookAtToMatrix(cameraPos, targetPos);

    if (globals::combat::smoothing) {
        float tX = 1.0f / globals::combat::smoothingx;
        float tY = 1.0f / globals::combat::smoothingy;

        tX = std::max(0.01f, std::min(1.0f, tX));
        tY = std::max(0.01f, std::min(1.0f, tY));

        float smoothFactorX, smoothFactorY;

        switch (globals::combat::smoothing_style) {
        case 1: smoothFactorX = easing::linear(tX); smoothFactorY = easing::linear(tY); break;
        case 2: smoothFactorX = easing::easeInQuad(tX); smoothFactorY = easing::easeInQuad(tY); break;
        case 3: smoothFactorX = easing::easeOutQuad(tX); smoothFactorY = easing::easeOutQuad(tY); break;
        case 4: smoothFactorX = easing::easeInOutQuad(tX); smoothFactorY = easing::easeInOutQuad(tY); break;
        case 5: smoothFactorX = easing::easeInCubic(tX); smoothFactorY = easing::easeInCubic(tY); break;
        case 6: smoothFactorX = easing::easeOutCubic(tX); smoothFactorY = easing::easeOutCubic(tY); break;
        case 7: smoothFactorX = easing::easeInOutCubic(tX); smoothFactorY = easing::easeInOutCubic(tY); break;
        case 8: smoothFactorX = easing::easeInSine(tX); smoothFactorY = easing::easeInSine(tY); break;
        case 9: smoothFactorX = easing::easeOutSine(tX); smoothFactorY = easing::easeOutSine(tY); break;
        case 10: smoothFactorX = easing::easeInOutSine(tX); smoothFactorY = easing::easeInOutSine(tY); break;
        default: smoothFactorX = tX; smoothFactorY = tY; break;
        }

        Matrix3 smoothedRotation{};
        for (int i = 0; i < 9; ++i) {
            if (i % 3 == 0) {
                smoothedRotation.data[i] = currentRotation.data[i] + (targetMatrix.data[i] - currentRotation.data[i]) * smoothFactorX;
            }
            else {
                smoothedRotation.data[i] = currentRotation.data[i] + (targetMatrix.data[i] - currentRotation.data[i]) * smoothFactorY;
            }
        }
        camera.writeRot(smoothedRotation);
    }
    else {
        camera.writeRot(targetMatrix);
    }
}
void performMouseAimbot(const Vector2& screenCoords, HWND robloxWindow) {
    POINT cursorPos;
    GetCursorPos(&cursorPos);
    ScreenToClient(robloxWindow, &cursorPos);

    float deltaX = screenCoords.x - cursorPos.x;
    float deltaY = screenCoords.y - cursorPos.y;

    if (globals::combat::smoothing) {
        float tX = 1.0f / (globals::combat::smoothingx * 0.05f);
        float tY = 1.0f / (globals::combat::smoothingy * 0.05f);

        tX = std::max(0.01f, std::min(1.0f, tX));
        tY = std::max(0.01f, std::min(1.0f, tY));

        float smoothFactorX, smoothFactorY;

        switch (globals::combat::smoothing_style) {
        case 1: smoothFactorX = easing::linear(tX); smoothFactorY = easing::linear(tY); break;
        case 2: smoothFactorX = easing::easeInQuad(tX); smoothFactorY = easing::easeInQuad(tY); break;
        case 3: smoothFactorX = easing::easeOutQuad(tX); smoothFactorY = easing::easeOutQuad(tY); break;
        case 4: smoothFactorX = easing::easeInOutQuad(tX); smoothFactorY = easing::easeInOutQuad(tY); break;
        case 5: smoothFactorX = easing::easeInCubic(tX); smoothFactorY = easing::easeInCubic(tY); break;
        case 6: smoothFactorX = easing::easeOutCubic(tX); smoothFactorY = easing::easeOutCubic(tY); break;
        case 7: smoothFactorX = easing::easeInOutCubic(tX); smoothFactorY = easing::easeInOutCubic(tY); break;
        case 8: smoothFactorX = easing::easeInSine(tX); smoothFactorY = easing::easeInSine(tY); break;
        case 9: smoothFactorX = easing::easeOutSine(tX); smoothFactorY = easing::easeOutSine(tY); break;
        case 10: smoothFactorX = easing::easeInOutSine(tX); smoothFactorY = easing::easeInOutSine(tY); break;
        default: smoothFactorX = tX; smoothFactorY = tY; break;
        }

        float dpiAdjustedSensitivity = mouseSettings.getDPIAdjustedSensitivity();
        smoothFactorX *= dpiAdjustedSensitivity;
        smoothFactorY *= dpiAdjustedSensitivity;

        deltaX *= smoothFactorX;
        deltaY *= smoothFactorY;
    }

    if (std::isfinite(deltaX) && std::isfinite(deltaY) && (abs(deltaX) > 0.1f || abs(deltaY) > 0.1f)) {
        INPUT input = { 0 };
        input.type = INPUT_MOUSE;
        input.mi.dx = static_cast<LONG>(deltaX);
        input.mi.dy = static_cast<LONG>(deltaY);
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
        SendInput(1, &input, sizeof(input));
    }
}

roblox::player gettargetclosesttomouse(bool isAimbotActiveParam, bool isSilentAimbotActiveParam) {
    static HWND robloxWindow = nullptr;
    static auto lastWindowCheck = std::chrono::steady_clock::now();

    auto now = std::chrono::steady_clock::now();
    if (!robloxWindow || std::chrono::duration_cast<std::chrono::seconds>(now - lastWindowCheck).count() > 5) {
        robloxWindow = FindWindowA(nullptr, "Roblox");
        lastWindowCheck = now;
    }

    if (!robloxWindow) return {};

    POINT mouse_point;
    if (!GetCursorPos(&mouse_point) || !ScreenToClient(robloxWindow, &mouse_point)) {
        return {};
    }
    const Vector2 mouseCursorPos = { (float)mouse_point.x, (float)mouse_point.y };
    const Vector2 screenCenter = { (float)GetSystemMetrics(SM_CXSCREEN) / 2, (float)GetSystemMetrics(SM_CYSCREEN) / 2 };

    std::vector<roblox::player> current_players;
    {
        std::lock_guard<std::mutex> lock(globals::instances::cachedplayers_mutex); // Lock mutex
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
                    bot_player.health = humanoid.read_health();
                    bot_player.maxhealth = humanoid.read_maxhealth();
                    current_players.push_back(bot_player);
                }
            }
        }
    }

    if (current_players.empty()) return {};

    const bool useKnockCheck = globals::combat::knockcheck;
    const bool useWallCheck = globals::combat::wallcheck;
    const bool useAimbotFov = globals::combat::usefov;
    const bool useHealthCheck = globals::combat::healthcheck; // Keep healthcheck as a separate boolean
    const float aimbotFovSize = globals::combat::fovsize;
    const float aimbotFovSizeSquared = aimbotFovSize * aimbotFovSize;

    const bool useSilentAimbotFov = globals::combat::silentaimfov;
    const float silentAimbotFovSize = globals::combat::silentaimfovsize;
    const float silentAimbotFovSizeSquared = silentAimbotFovSize * silentAimbotFovSize;

    const float healthThreshold = globals::combat::healththreshhold;
    const bool useTeamCheck = globals::combat::teamcheck || ((*globals::combat::flags)[0] != 0) || (globals::instances::gamename == "Arsenal");
    const std::string& localPlayerName = globals::instances::localplayer.get_name();
    const Vector3 cameraPos = globals::instances::camera.getPos();

    roblox::player closest = {};
    float closestDistanceSquared = 9e18f;

    for (auto player : current_players) { // Iterate over the local copy
        // Skip if the player is the local player or invalid
        if (!player.head.is_valid() ||
            player.name == localPlayerName ||
            !player.main.is_valid()) { // Combined validity check
            continue;
        }

        // Skip if the player is marked as friendly
        if ((globals::bools::player_status.count(player.name) && globals::bools::player_status[player.name]) ||
            (globals::bools::player_status.count(player.displayname) && globals::bools::player_status[player.displayname])) {
            continue;
        }

        // Team check: skip players on same team (universal helper)
        if (useTeamCheck && teamcheck::same_team(globals::instances::lp, player)) continue;

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

        const Vector2 screenCoords = roblox::worldtoscreen(player.head.get_pos());
        if (screenCoords.x == -1.0f || screenCoords.y == -1.0f) continue;

        // Robust alive check (handles Arsenal)
        bool isAlive = true;
        if (globals::instances::gamename == "Arsenal") {
            // Prefer NRPBS health for Arsenal
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
            // Fallback to cached health if NRPBS not available
            if (isAlive && player.health <= 0) isAlive = false;
        } else {
            auto humanoid = player.instance.findfirstchild("Humanoid");
            if (!humanoid.is_valid() || humanoid.get_health() <= 0) {
                isAlive = false;
            }
        }
        if (!isAlive) continue;

        // FOV gating: when both aimbot and silent are active, allow targets
        // that are inside EITHER enabled FOV (union), not just the first one.
        bool passesFov = true; // default pass when no FOVs are enabled
        if (isAimbotActiveParam || isSilentAimbotActiveParam) {
            // Treat a feature with FOV disabled as "no restriction" (true)
            bool passAimbotFov = true;
            bool passSilentFov = true;

            if (isAimbotActiveParam) {
                if (useAimbotFov) {
                    const float dxA = mouseCursorPos.x - screenCoords.x;
                    const float dyA = mouseCursorPos.y - screenCoords.y;
                    const float distSqA = dxA * dxA + dyA * dyA;
                    passAimbotFov = (distSqA <= aimbotFovSizeSquared);
                } else {
                    passAimbotFov = true;
                }
            }

            if (isSilentAimbotActiveParam) {
                if (useSilentAimbotFov) {
                    const float dxS = mouseCursorPos.x - screenCoords.x;
                    const float dyS = mouseCursorPos.y - screenCoords.y;
                    const float distSqS = dxS * dxS + dyS * dyS;
                    passSilentFov = (distSqS <= silentAimbotFovSizeSquared);
                } else {
                    passSilentFov = true;
                }
            }

            // Union: inside either feature's allowed region
            bool considerAimbot = isAimbotActiveParam;
            bool considerSilent = isSilentAimbotActiveParam;
            passesFov = (considerAimbot ? passAimbotFov : false) || (considerSilent ? passSilentFov : false);
        }

        if (!passesFov) continue;

        // Selection metric: if any FOV is active, measure distance to mouse; otherwise to screen center
        Vector2 selectionRef = (((isAimbotActiveParam && useAimbotFov) || (isSilentAimbotActiveParam && useSilentAimbotFov))
            ? mouseCursorPos : screenCenter);
        const float dxSel = selectionRef.x - screenCoords.x;
        const float dySel = selectionRef.y - screenCoords.y;
        const float distanceSquared = dxSel * dxSel + dySel * dySel;

        auto humanoid = player.instance.findfirstchild("Humanoid");
        if (!humanoid.is_valid() || humanoid.get_health() <= 0) { // Use is_valid()
            continue;
        }

        if (useKnockCheck) {
            auto bodyEffects = player.instance.findfirstchild("BodyEffects");
            if (bodyEffects.is_valid() && bodyEffects.findfirstchild("K.O").read_bool_value()) { // Use is_valid()
                continue;
            }
        }

        if (useHealthCheck && player.health <= healthThreshold) continue;

        if (useWallCheck) {
            Vector3 player_head_pos = player.head.get_pos();
            Vector3 current_camera_pos = globals::instances::camera.getPos();
            if (!wallcheck::is_valid_vector(player_head_pos) || !wallcheck::is_valid_vector(current_camera_pos)) {
                continue; // Skip if positions are invalid
            }
            if (!wallcheck::can_see(player_head_pos, current_camera_pos)) {
                continue;
            }
        }

        // Check distance to target
        if (globals::combat::rangecheck) {
            Vector3 player_head_pos = player.head.get_pos();
            Vector3 current_camera_pos = globals::instances::camera.getPos();
            if (!wallcheck::is_valid_vector(player_head_pos) || !wallcheck::is_valid_vector(current_camera_pos)) {
                continue; // Skip if positions are invalid
            }
            float distanceToTarget = CalculateDistance1(player_head_pos, current_camera_pos);
            if (distanceToTarget > globals::combat::aim_distance) {
                continue;
            }
        }

        if (distanceSquared < closestDistanceSquared) {
            closestDistanceSquared = distanceSquared;
            closest = player;
        }
    }

    return closest;
}

void hooks::aimbot() {
    if (!mouseSettings.dpiAutoDetected) {
        detectMouseDPI();
    }

    HWND robloxWindow = FindWindowA(0, "Roblox");
    roblox::player target;
    bool currentStickyAim = false;

    while (true) {
        // Per-iteration targets for independent aimbot vs silent selection
        roblox::player aim_target = {};
        roblox::player silent_target = {};
        // Skip aimbot during teleport recovery to prevent auto-targeting
        if (globals::handlingtp || globals::prevent_auto_targeting) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        globals::combat::aimbotkeybind.update();
        globals::combat::silentaimkeybind.update();
        globals::misc::rotate360keybind.update();
        
        // Reset target_was_knocked flag when user presses aimbot key to manually retarget
        if (globals::combat::aimbotkeybind.is_pressed() || globals::combat::silentaimkeybind.is_pressed()) {
            globals::target_was_knocked = false;
        }

        bool isAimbotActive = globals::combat::aimbot && globals::combat::aimbotkeybind.enabled;
        bool isSilentAimbotActive = globals::combat::silentaim && globals::combat::silentaimkeybind.enabled;
        bool isRotate360Active = globals::misc::rotate360 && globals::misc::rotate360keybind.enabled;
        static bool rotating_once = false;
        static int rotation_steps_completed = 0;
        static bool rotation_started = false; // New flag to prevent overwriting original_camera_rotation

        currentStickyAim = isAimbotActive ? globals::combat::stickyaim : globals::combat::stickyaimsilent;

        if (globals::misc::rotate360keybind.is_pressed() && globals::misc::rotate360 && !rotation_started) {
            rotating_once = true;
            rotation_started = true; // Set flag to true
            globals::misc::camera_rotation_yaw = 0.0f; // Start from 0 for a clean 360
            rotation_steps_completed = 0;
            globals::misc::original_camera_rotation = globals::instances::camera.getRot(); // Store original rotation
        }

        if (rotating_once) {
            roblox::camera camera_instance = globals::instances::camera;
            
            globals::misc::camera_rotation_yaw += globals::misc::rotate360_speed; // Use slider value
            rotation_steps_completed++;

            // Calculate total steps based on the new speed
            const int dynamic_total_rotation_steps = static_cast<int>(360.0f / globals::misc::rotate360_speed);

            if (rotation_steps_completed >= dynamic_total_rotation_steps) {
                globals::misc::camera_rotation_yaw = 0.0f; // Reset to 0 after full rotation
                rotating_once = false; // Stop rotation
                rotation_started = false; // Reset flag
                rotation_steps_completed = 0;
                camera_instance.writeRot(globals::misc::original_camera_rotation); // Return to original rotation
            } else {
                math::Matrix3 yawRotation = math::Matrix3::from_yaw(globals::misc::camera_rotation_yaw);
                math::Matrix3 newRotation = globals::misc::original_camera_rotation * yawRotation;
                camera_instance.writeRot(newRotation);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Control rotation speed
            continue;
        }

        if (!globals::focused || !keybind::is_roblox_focused() || (!isAimbotActive && !isSilentAimbotActive)) {
            foundTarget = false;
            target = {};
            globals::instances::cachedtarget = {}; // Clear global cached target
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        if (currentStickyAim && foundTarget) { // If sticky aim is on and we have a target
            // Validate the existing target
            if (!target.main.is_valid() || !target.head.is_valid()) {
                foundTarget = false; // Target became completely invalid
            } else {
                auto humanoid = target.instance.findfirstchild("Humanoid");
                if (!humanoid.is_valid() || humanoid.get_health() <= 0) {
                    foundTarget = false; // Target became invalid (e.g., died)
                } else if (globals::combat::knockcheck) { // Check for knocked status even with sticky aim
                    auto bodyEffects = target.instance.findfirstchild("BodyEffects");
                    if (bodyEffects.is_valid() && bodyEffects.findfirstchild("K.O").read_bool_value()) {
                        // Target is knocked. Reset foundTarget and set flag to prevent immediate re-targeting
                        foundTarget = false;
                        target = {};
                        globals::instances::cachedtarget = {}; // Clear global cached target
                        globals::target_was_knocked = true; // Set flag to prevent immediate re-targeting
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue; // Skip aiming for this frame and find new target
                    }
                }

                // Team validation while sticky: prevent sticking to teammates (e.g., after team swap)
                const bool useTeamCheckLive = globals::combat::teamcheck || ((*globals::combat::flags)[0] != 0) || (globals::instances::gamename == "Arsenal");
                if (useTeamCheckLive) {
                    if (teamcheck::same_team(globals::instances::lp, target)) {
                        foundTarget = false;
                        target = {};
                        globals::instances::cachedtarget = {};
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    }
                }
                // Drop sticky target if it walks out of range
                if (globals::combat::rangecheck) {
                    Vector3 player_head_pos = target.head.get_pos();
                    Vector3 current_camera_pos = globals::instances::camera.getPos();
                    if (!wallcheck::is_valid_vector(player_head_pos) || !wallcheck::is_valid_vector(current_camera_pos)) {
                        foundTarget = false;
                    } else {
                        float distanceToTarget = CalculateDistance1(player_head_pos, current_camera_pos);
                        if (distanceToTarget > globals::combat::aim_distance) {
                            foundTarget = false;
                            globals::target_was_knocked = true; // Block immediate retarget after range loss
                        }
                    }

                    if (!foundTarget) {
                        target = {};
                        globals::instances::cachedtarget = {};
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    }
                }
                // Add other validation checks here if necessary (e.g., out of range)
            }

            // If validation failed (e.g., target died), clear target and continue to find a new one
            if (!foundTarget) {
                target = {};
                globals::instances::cachedtarget = {}; // Clear global cached target
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue; // Continue to next iteration to find a new target
            }
        } else { // No sticky aim, or no target found yet
            // If target was knocked, don't immediately find a new target
            if (globals::target_was_knocked) {
                foundTarget = false;
                target = {};
                globals::instances::cachedtarget = {};
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            
            // Select targets independently for aimbot and silent to avoid cross-coupling
            if (isAimbotActive) {
                aim_target = gettargetclosesttomouse(true, false);
            }
            if (isSilentAimbotActive) {
                silent_target = gettargetclosesttomouse(false, true);
            }

            // Use the appropriate target for this loop's aiming work
            if (isAimbotActive) {
                // When aimbot is active, NEVER fall back to silent target for aiming
                target = (aim_target.head.address != 0) ? aim_target : roblox::player{};
            } else if (isSilentAimbotActive) {
                // Only when aimbot is not active, allow silent target to drive 'target'
                target = (silent_target.head.address != 0) ? silent_target : roblox::player{};
            } else {
                target = {};
            }
            // Filter out friendly targets
            if (target.head.address != 0 && 
                ((globals::bools::player_status.count(target.name) && globals::bools::player_status[target.name]) ||
                 (globals::bools::player_status.count(target.displayname) && globals::bools::player_status[target.displayname]))) {
                target = {}; // Ignore friendly targets
            }

            if (is_valid_address(target.main.address) && target.head.address != 0) {
                foundTarget = true;
                globals::target_was_knocked = false; // Reset flag when new target is found
            } else {
                foundTarget = false;
                target = {}; // Ensure target is cleared if not found
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
        }

        // Silent FOV revalidation and mouse writes are handled in silent module; no cross-clearing here.

        roblox::instance targetPart;
        float closest_dist = FLT_MAX;

        enum HumanoidStateType {
            Jumping = 3,
            Freefall = 10
        };

        // Determine airborne state based on TARGET (not local player)
        int target_state = target.humanoid.is_valid() ? target.humanoid.get_state() : -1;
        bool target_air = (target_state == Jumping ||
            /* handle possible Freefall enum differences */ target_state == Freefall ||
            target_state == 6 /* Freefall in some builds */);
        // Velocity-based fallback to detect airborne reliably (target)
        if (!target_air) {
            roblox::instance body = target.hrp.is_valid() ? target.hrp : (target.uppertorso.is_valid() ? target.uppertorso : target.head);
            if (body.is_valid()) {
                auto v = body.get_velocity();
                if (v.y > 2.0f || v.y < -2.0f) target_air = true;
            }
        }

        // Also consider local player's airborne state to satisfy both interpretations of "airpart"
        int local_state = globals::instances::lp.humanoid.is_valid() ? globals::instances::lp.humanoid.get_state() : -1;
        bool local_air = (local_state == Jumping || local_state == Freefall || local_state == 6);
        if (!local_air) {
            roblox::instance lbody = globals::instances::lp.hrp.is_valid() ? globals::instances::lp.hrp : (globals::instances::lp.uppertorso.is_valid() ? globals::instances::lp.uppertorso : globals::instances::lp.head);
            if (lbody.is_valid()) {
                auto v = lbody.get_velocity();
                if (v.y > 2.0f || v.y < -2.0f) local_air = true;
            }
        }

        bool is_airborne = (target_air || local_air);

        std::vector<int>* aimpart_to_use = is_airborne ? globals::combat::airaimpart : globals::combat::aimpart;
        std::vector<int>* silentaimpart_to_use = is_airborne ? globals::combat::airsilentaimpart : globals::combat::silentaimpart;

        if (isAimbotActive) {
            bool is_all_selected = (*aimpart_to_use)[14];
            int selected_idx = -1;
            std::string selected_name;
            bool is_specific_part_selected = false;
            for (int i = 0; i < 14; ++i) {
                if ((*aimpart_to_use)[i]) {
                    is_specific_part_selected = true;
                    break;
                }
            }

            if (is_all_selected && !is_specific_part_selected) {
                for (int i = 0; i < 14; ++i) {
                    roblox::instance part = target.instance.findfirstchild(globals::combat::hit_parts[i]);
                    // Fallback mapping for rigs where a direct part name doesn't exist (e.g., R15 vs R6)
                    if (!part.address && target.rigtype == 1) {
                        if (i == 3) part = target.instance.findfirstchild("UpperTorso"); // Torso -> UpperTorso
                        else if (i == 4) part = target.instance.findfirstchild("LeftUpperLeg"); // LeftLeg -> LeftUpperLeg
                        else if (i == 5) part = target.instance.findfirstchild("RightUpperLeg"); // RightLeg -> RightUpperLeg
                    }
                    if (part.address) {
                        Vector2 screen_pos = roblox::worldtoscreen(part.get_pos());
                        if (screen_pos.x != -1) {
                            float dist = CalculateDistance(screen_pos, { (float)GetSystemMetrics(SM_CXSCREEN) / 2, (float)GetSystemMetrics(SM_CYSCREEN) / 2 });
                            if (dist < closest_dist) {
                                closest_dist = dist;
                                targetPart = part;
                                selected_idx = i;
                                selected_name = part.get_name();
                            }
                        }
                    }
                }
            } else {
                for (int i = 0; i < 14; ++i) {
                    if ((*aimpart_to_use)[i]) {
                        roblox::instance part = target.instance.findfirstchild(globals::combat::hit_parts[i]);
                        if (!part.address && target.rigtype == 1) {
                            if (i == 3) part = target.instance.findfirstchild("UpperTorso");
                            else if (i == 4) part = target.instance.findfirstchild("LeftUpperLeg");
                            else if (i == 5) part = target.instance.findfirstchild("RightUpperLeg");
                        }
                        if (part.address) {
                            targetPart = part;
                            selected_idx = i;
                            selected_name = part.get_name();
                            break;
                        }
                    }
                }
            }
            // Record which part is currently used for aimbot (for debug/indicator)
            if (selected_idx >= 0) {
                globals::combat::last_used_aimpart = selected_name.empty() ? globals::combat::hit_parts[selected_idx] : selected_name;
                globals::combat::last_used_aimpart_air = is_airborne;
            }
        } else if (isSilentAimbotActive) {
            bool is_all_selected = (*silentaimpart_to_use)[14];
            int selected_idx = -1;
            std::string selected_name;
            bool is_specific_part_selected = false;
            for (int i = 0; i < 14; ++i) {
                if ((*silentaimpart_to_use)[i]) {
                    is_specific_part_selected = true;
                    break;
                }
            }

            if (is_all_selected && !is_specific_part_selected) {
                for (int i = 0; i < 14; ++i) {
                    roblox::instance part = target.instance.findfirstchild(globals::combat::hit_parts[i]);
                    if (!part.address && target.rigtype == 1) {
                        if (i == 3) part = target.instance.findfirstchild("UpperTorso");
                        else if (i == 4) part = target.instance.findfirstchild("LeftUpperLeg");
                        else if (i == 5) part = target.instance.findfirstchild("RightUpperLeg");
                    }
                    if (part.address) {
                        Vector2 screen_pos = roblox::worldtoscreen(part.get_pos());
                        if (screen_pos.x != -1) {
                            float dist = CalculateDistance(screen_pos, { (float)GetSystemMetrics(SM_CXSCREEN) / 2, (float)GetSystemMetrics(SM_CYSCREEN) / 2 });
                            if (dist < closest_dist) {
                                closest_dist = dist;
                                targetPart = part;
                                selected_idx = i;
                                selected_name = part.get_name();
                            }
                        }
                    }
                }
            } else {
                for (int i = 0; i < 14; ++i) {
                    if ((*silentaimpart_to_use)[i]) {
                        roblox::instance part = target.instance.findfirstchild(globals::combat::hit_parts[i]);
                        if (!part.address && target.rigtype == 1) {
                            if (i == 3) part = target.instance.findfirstchild("UpperTorso");
                            else if (i == 4) part = target.instance.findfirstchild("LeftUpperLeg");
                            else if (i == 5) part = target.instance.findfirstchild("RightUpperLeg");
                        }
                        if (part.address) {
                            targetPart = part;
                            selected_idx = i;
                            selected_name = part.get_name();
                            break;
                        }
                    }
                }
            }
            // Record which part is currently used for silent aim (for debug/indicator)
            if (selected_idx >= 0) {
                globals::combat::last_used_aimpart = selected_name.empty() ? globals::combat::hit_parts[selected_idx] : selected_name;
                globals::combat::last_used_aimpart_air = is_airborne;
            }
        }

        if (!targetPart.is_valid()) {
            if (!currentStickyAim) {
                foundTarget = false;
                target = {};
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        Vector3 targetPos = targetPart.get_pos();

        auto humanoid = target.instance.findfirstchild("Humanoid");
        if (!humanoid.is_valid()) { // Only unlock if the humanoid instance is no longer valid
            foundTarget = false;
            continue;
        }

        if (globals::combat::knockcheck) {
            auto bodyEffects = target.instance.findfirstchild("BodyEffects");
            if (bodyEffects.is_valid() && bodyEffects.findfirstchild("K.O").read_bool_value()) {
                foundTarget = false;
                target = {};
                globals::instances::cachedtarget = {};
                globals::target_was_knocked = true; // Set flag to prevent immediate re-targeting
                continue;
            }
        }

        Vector3 predictedPos = targetPos;
        if (globals::combat::predictions) {
            Vector3 velocity = targetPart.get_velocity();
            Vector3 veloVector = DivVector3(velocity, { globals::combat::predictionsx, globals::combat::predictionsy, globals::combat::predictionsx });
            predictedPos = AddVector3(targetPos, veloVector);
        }

        Vector2 screenCoords = roblox::worldtoscreen(predictedPos);
        // If sticky aim is active, do not unlock if the target goes off-screen.
        // Continue to aim at the predicted position even if it's not visible.
        if ((screenCoords.x == -1.0f || screenCoords.y == -1.0f)) {
            if (!currentStickyAim) { // Only unlock if sticky aim is NOT active
                foundTarget = false;
                target = {};
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue; // If not sticky aim and off-screen, clear target and find new one
            }
            // If sticky aim IS active and off-screen, we maintain foundTarget = true.
            // For mouse aimbot, we should skip mouse movement for this frame to avoid erratic behavior.
            // For camera aimbot, we proceed to performCameraAimbot as predictedPos is valid.
            if (isAimbotActive && globals::combat::aimbottype == 1) {
                // If mouse aimbot and off-screen, skip mouse movement for this frame
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue; // Skip to next iteration, but foundTarget remains true
            }
            // If camera aimbot and off-screen, or silent aim, we proceed to aim.
            // No 'continue' here, so the aiming logic below will execute.
        }
        if (isAimbotActive) {
            if (globals::combat::aimbottype == 0) {
                roblox::camera camera = globals::instances::camera;
                Vector3 cameraPos = camera.getPos();
                performCameraAimbot(predictedPos, cameraPos);
                if (globals::combat::smoothing) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            else if (globals::combat::aimbottype == 1) {
                performMouseAimbot(screenCoords, robloxWindow);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        
        // Silent mouse writing is performed in hooks::silentrun2() based on cachedtarget.

        // Prefer aimbot-locked target for locked ESP; otherwise fallback to silent's
        // Use current 'target' when aimbot is active to cover sticky-aim paths
        if (isAimbotActive && target.head.is_valid()) {
            globals::instances::cachedtarget = target;
            globals::instances::cachedlasttarget = target;
        } else if (isSilentAimbotActive) {
            roblox::player silent_cached = (silent_target.head.is_valid()) ? silent_target : gettargetclosesttomouse(false, true);
            if (silent_cached.head.is_valid()) {
                globals::instances::cachedtarget = silent_cached;
                globals::instances::cachedlasttarget = silent_cached;
            } else {
                globals::instances::cachedtarget = {};
            }
        } else if (target.head.is_valid()) {
            globals::instances::cachedtarget = target;
            globals::instances::cachedlasttarget = target;
        } else {
            globals::instances::cachedtarget = {};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
