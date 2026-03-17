#define NOMINMAX // Prevents Windows.h from defining min/max macros
#include "../visuals.h"
#include <Windows.h>
#include <cfloat>

static bool original_fog_settings_saved = false;
static float original_fog_start = 0.0f;
static float original_fog_end = 0.0f;
static math::Vector3 original_fog_color = { 0.0f, 0.0f, 0.0f };
#include "../../wallcheck/wallcheck.h"
#include "../../../util/teamcheck.h"
#include <unordered_map>
#include <chrono>
#include <map>
#include <limits>
#include <mutex>
#include <thread>
#include <algorithm>
#include <vector>
#include <algorithm> // For std::min and std::max

ESPData visuals::espData;
std::thread visuals::updateThread;
bool visuals::threadRunning = false;

void draw_text_with_shadow_enhanced(float font_size, const ImVec2& position, ImColor color, const char* text);
static void render_sonar_enhanced();

inline float distance_sq(const math::Vector3& v1, const math::Vector3& v2) {
    float dx = v1.x - v2.x;
    float dy = v1.y - v2.y;
    float dz = v1.z - v2.z;
    return dx * dx + dy * dy + dz * dz;
}

inline float distance_sq(const math::Vector2& v1, const math::Vector2& v2) {
    float dx = v1.x - v2.x;
    float dy = v1.y - v2.y;
    return dx * dx + dy * dy;
}

float vector_length(const math::Vector3& vec) {
    return sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}

void render_box_enhanced(const math::Vector2& top_left, const math::Vector2& bottom_right, ImColor color) {
    auto dimensions = globals::instances::visualengine.get_dimensions();
    if (dimensions.x == 0 || dimensions.y == 0) return;

    float width = bottom_right.x - top_left.x;
    float height = bottom_right.y - top_left.y;

    if (globals::visuals::boxfill) {
        globals::instances::draw->AddRectFilled(
            ImVec2(top_left.x, top_left.y),
            ImVec2(bottom_right.x, bottom_right.y),
            ImGui::ColorConvert(globals::visuals::boxfillcolor), 0
        );
    }

    if (globals::visuals::boxtype == 0) {

        if ((*globals::visuals::box_overlay_flags)[0]) {
            globals::instances::draw->AddRect(
                ImVec2(top_left.x, top_left.y),
                ImVec2(bottom_right.x, bottom_right.y),
                ImColor(0, 0, 0, 255), 0.0f, 0, 2.5f
            );
        }

        globals::instances::draw->AddRect(
            ImVec2(top_left.x, top_left.y),
            ImVec2(bottom_right.x, bottom_right.y),
            color, 0.0f, 0, 0.25f
        );

        if ((*globals::visuals::box_overlay_flags)[2]) {
            ImColor boxfill = ImColor(globals::visuals::boxfillcolor[0], globals::visuals::boxfillcolor[1], globals::visuals::boxfillcolor[2], globals::visuals::boxfillcolor[3]);
            globals::instances::draw->AddRectFilled(
                ImVec2(top_left.x, top_left.y),
                ImVec2(bottom_right.x, bottom_right.y),
                boxfill, 0.0f, 0
            );
        }
    }
    else if (globals::visuals::boxtype == 1) {
        float corner_size = std::min(width * 1.5f, height) * 0.25f;


        if ((*globals::visuals::box_overlay_flags)[2]) {
            ImColor boxfill = ImColor(
                globals::visuals::boxfillcolor[0],
                globals::visuals::boxfillcolor[1],
                globals::visuals::boxfillcolor[2],
                globals::visuals::boxfillcolor[3]
            );
            globals::instances::draw->AddRectFilled(
                ImVec2(top_left.x, top_left.y),
                ImVec2(bottom_right.x, bottom_right.y),
                boxfill, 0.0f, 0
            );
        }

        globals::instances::draw->AddLine(ImVec2(top_left.x, top_left.y), ImVec2(top_left.x + corner_size, top_left.y), color, 1.0f);
        globals::instances::draw->AddLine(ImVec2(top_left.x, top_left.y), ImVec2(top_left.x, top_left.y + corner_size), color, 1.0f);

        globals::instances::draw->AddLine(ImVec2(bottom_right.x, top_left.y), ImVec2(bottom_right.x - corner_size, top_left.y), color, 1.0f);
        globals::instances::draw->AddLine(ImVec2(bottom_right.x, top_left.y), ImVec2(bottom_right.x, top_left.y + corner_size), color, 1.0f);

        globals::instances::draw->AddLine(ImVec2(top_left.x, bottom_right.y), ImVec2(top_left.x + corner_size, bottom_right.y), color, 1.0f);
        globals::instances::draw->AddLine(ImVec2(top_left.x, bottom_right.y), ImVec2(top_left.x, bottom_right.y - corner_size), color, 1.0f);

        globals::instances::draw->AddLine(ImVec2(bottom_right.x, bottom_right.y), ImVec2(bottom_right.x - corner_size, bottom_right.y), color, 1.0f);
        globals::instances::draw->AddLine(ImVec2(bottom_right.x, bottom_right.y), ImVec2(bottom_right.x, bottom_right.y - corner_size), color, 1.0f);

        if ((*globals::visuals::box_overlay_flags)[0]) {
            ImColor outline_color = ImColor(0, 0, 0, 180);
            float outline_thickness = 2.0f;

            globals::instances::draw->AddLine(ImVec2(top_left.x - 1, top_left.y - 1), ImVec2(top_left.x + corner_size + 1, top_left.y - 1), outline_color, outline_thickness);
            globals::instances::draw->AddLine(ImVec2(top_left.x - 1, top_left.y - 1), ImVec2(top_left.x - 1, top_left.y + corner_size + 1), outline_color, outline_thickness);

            globals::instances::draw->AddLine(ImVec2(bottom_right.x + 1, top_left.y - 1), ImVec2(bottom_right.x - corner_size - 1, top_left.y - 1), outline_color, outline_thickness);
            globals::instances::draw->AddLine(ImVec2(bottom_right.x + 1, top_left.y - 1), ImVec2(bottom_right.x + 1, top_left.y + corner_size + 1), outline_color, outline_thickness);

            globals::instances::draw->AddLine(ImVec2(top_left.x - 1, bottom_right.y + 1), ImVec2(top_left.x + corner_size + 1, bottom_right.y + 1), outline_color, outline_thickness);
            globals::instances::draw->AddLine(ImVec2(top_left.x - 1, bottom_right.y + 1), ImVec2(top_left.x - 1, bottom_right.y - corner_size - 1), outline_color, outline_thickness);

            globals::instances::draw->AddLine(ImVec2(bottom_right.x + 1, bottom_right.y + 1), ImVec2(bottom_right.x - corner_size - 1, bottom_right.y + 1), outline_color, outline_thickness);
            globals::instances::draw->AddLine(ImVec2(bottom_right.x + 1, bottom_right.y + 1), ImVec2(bottom_right.x + 1, bottom_right.y - corner_size - 1), outline_color, outline_thickness);
        }

        globals::instances::draw->AddLine(ImVec2(top_left.x, top_left.y), ImVec2(top_left.x + corner_size, top_left.y), color, 1.0f);
        globals::instances::draw->AddLine(ImVec2(top_left.x, top_left.y), ImVec2(top_left.x, top_left.y + corner_size), color, 1.0f);

        globals::instances::draw->AddLine(ImVec2(bottom_right.x, top_left.y), ImVec2(bottom_right.x - corner_size, top_left.y), color, 1.0f);
        globals::instances::draw->AddLine(ImVec2(bottom_right.x, top_left.y), ImVec2(bottom_right.x, top_left.y + corner_size), color, 1.0f);

        globals::instances::draw->AddLine(ImVec2(top_left.x, bottom_right.y), ImVec2(top_left.x + corner_size, bottom_right.y), color, 1.0f);
        globals::instances::draw->AddLine(ImVec2(top_left.x, bottom_right.y), ImVec2(top_left.x, bottom_right.y - corner_size), color, 1.0f);

        globals::instances::draw->AddLine(ImVec2(bottom_right.x, bottom_right.y), ImVec2(bottom_right.x - corner_size, bottom_right.y), color, 1.0f);
        globals::instances::draw->AddLine(ImVec2(bottom_right.x, bottom_right.y), ImVec2(bottom_right.x, bottom_right.y - corner_size), color, 1.0f);
    }
}

void render_health_bar_enhanced(const math::Vector2& top_left, const math::Vector2& bottom_right, float current_health, float max_health, ImColor color) {
    if (!globals::visuals::healthbar) return;

    float health_percent = current_health / max_health;
    float box_width = bottom_right.x - top_left.x;
    float box_height = bottom_right.y - top_left.y;

    float bar_width = 1.3f; // Made health bar thicker
    float bar_height = box_height;
    float bar_x = top_left.x - bar_width - 4.0f;

    if (globals::visuals::healthbar_overlay_flags->size() < 2) {
        globals::visuals::healthbar_overlay_flags->resize(2, 0);
    }

    if ((*globals::visuals::healthbar_overlay_flags)[0]) {
        ImColor outline_color = ImColor(0, 0, 0, 255);
        globals::instances::draw->AddRectFilled(
            ImVec2(bar_x - 1, top_left.y - 1),
            ImVec2(bar_x + bar_width + 1, bottom_right.y + 1),
            outline_color
        );
    }

    ImColor background_color = ImColor(35, 35, 35, 200);
    globals::instances::draw->AddRectFilled(
        ImVec2(bar_x, top_left.y),
        ImVec2(bar_x + bar_width, bottom_right.y),
        background_color
    );

    color = ImGui::ColorConvert(globals::visuals::healthbarcolor);
    auto color1 = ImGui::ColorConvert(globals::visuals::healthbarcolor1);
    float fill_height = bar_height * health_percent;

    if ((*globals::visuals::healthbar_overlay_flags)[1]) {
        globals::instances::draw->AddRectFilledMultiColor(
            ImVec2(bar_x, bottom_right.y - fill_height),
            ImVec2(bar_x + bar_width, bottom_right.y),
            color, color, color1, color1
        );
    }
    else {
        globals::instances::draw->AddRectFilled(
            ImVec2(bar_x, bottom_right.y - fill_height),
            ImVec2(bar_x + bar_width, bottom_right.y),
            color
        );
    }

}

void render_name_enhanced(const std::string& name, const math::Vector2& top_left, const math::Vector2& bottom_right) {
    if (!globals::visuals::name || name.empty()) return;

    float center_x = (top_left.x + bottom_right.x) * 0.5f;
    ImVec2 text_size = ImGui::CalcTextSize(name.c_str());
    float y_pos = top_left.y - 15.0f;

    draw_text_with_shadow_enhanced(13.0f, ImVec2(center_x - text_size.x * 0.5f, y_pos), ImGui::ColorConvert(globals::visuals::namecolor), name.c_str());
}

void render_distance_enhanced(float distance, const math::Vector2& top_left, const math::Vector2& bottom_right, bool has_tool) {
    if (!globals::visuals::distance) return;

    char distance_str[16];
    sprintf_s(distance_str, "[%.0fm]", distance);

    float center_x = (top_left.x + bottom_right.x) * 0.5f;
    float offset = bottom_right.y + (globals::visuals::toolesp ? 14.0f : 2.0f);
    ImVec2 text_size = ImGui::CalcTextSize(distance_str);
    text_size.x *= (9.0f / ImGui::GetFontSize());

    draw_text_with_shadow_enhanced(9.0f, ImVec2(center_x - text_size.x * 0.5f, offset), ImGui::ColorConvert(globals::visuals::distancecolor), distance_str);
}

void render_tool_name_enhanced(const std::string& tool_name, const math::Vector2& top_left, const math::Vector2& bottom_right) {
    if (!globals::visuals::toolesp || tool_name.empty()) return;

    float center_x = (top_left.x + bottom_right.x) * 0.5f;
    float y_pos = bottom_right.y + 2.0f;
    ImVec2 text_size = ImGui::CalcTextSize(tool_name.c_str());
    text_size.x *= (11.0f / ImGui::GetFontSize());
    draw_text_with_shadow_enhanced(11.0f, ImVec2(center_x - text_size.x * 0.5f, y_pos), ImGui::ColorConvert(globals::visuals::toolespcolor), tool_name.c_str());
}

#include <cmath> // For std::cos and std::sin

void render_snapline_enhanced(const math::Vector2& screen_pos, SnaplineType type, bool is_locked) {
    if (type == SnaplineType::None) return;

    ImVec2 screen_size = ImGui::GetIO().DisplaySize;
    ImVec2 start_point;

    switch (type) {
    case SnaplineType::Top:
        start_point = ImVec2(screen_size.x * 0.5f, 0);
        break;
    case SnaplineType::Bottom:
        start_point = ImVec2(screen_size.x * 0.5f, screen_size.y);
        break;
    case SnaplineType::Center:
        start_point = ImVec2(screen_size.x * 0.5f, screen_size.y * 0.5f);
        break;
    case SnaplineType::Crosshair: {
        POINT cursor_pos;
        GetCursorPos(&cursor_pos);
        HWND robloxWindow = FindWindowA(0, "Roblox");
        if (robloxWindow && ScreenToClient(robloxWindow, &cursor_pos)) {
            start_point = ImVec2((float)cursor_pos.x, (float)cursor_pos.y);
        }
        else {
            start_point = ImVec2(screen_size.x * 0.5f, screen_size.y * 0.5f); // Fallback to center if Roblox window not found
        }
        break;
    }
    default:
        return;
    }

    ImColor snapline_color = is_locked ? ImGui::ColorConvert(globals::visuals::lockedespcolor) : ImGui::ColorConvert(globals::visuals::snaplinecolor);

    if (globals::visuals::snaplineoverlaytype == 0) { // Straight
        // Draw outline (thicker black line behind)
        globals::instances::draw->AddLine(
            start_point,
            ImVec2(screen_pos.x, screen_pos.y),
            IM_COL32(0, 0, 0, 255), // Black outline
            3.0f // Thicker than the main line
        );
        
        // Draw main line
        globals::instances::draw->AddLine(
            start_point,
            ImVec2(screen_pos.x, screen_pos.y),
            snapline_color,
            1.0f
        );
    }
    else if (globals::visuals::snaplineoverlaytype == 1) { // Spiderweb
        ImVec2 start = start_point;
        ImVec2 end(screen_pos.x, screen_pos.y);
        float midX = (start.x + end.x) / 2.0f;
        float midY = (start.y + end.y) / 2.0f;

        ImVec2 control(midX, midY + 250.0f); // curve control

        const int segments = 35; // leave.
        // First pass: draw black outline (thicker lines)
        for (int i = 0; i < segments; i++)
        {
            float t0 = i / (float)segments;
            float t1 = (i + 1) / (float)segments;

            ImVec2 p0(
                (1 - t0) * (1 - t0) * start.x + 2 * (1 - t0) * t0 * control.x + t0 * t0 * end.x,
                (1 - t0) * (1 - t0) * start.y + 2 * (1 - t0) * t0 * control.y + t0 * t0 * end.y
            );

            ImVec2 p1(
                (1 - t1) * (1 - t1) * start.x + 2 * (1 - t1) * t1 * control.x + t1 * t1 * end.x,
                (1 - t1) * (1 - t1) * start.y + 2 * (1 - t1) * t1 * control.y + t1 * t1 * end.y
            );

            // Draw outline (thicker black line)
            globals::instances::draw->AddLine(p0, p1, IM_COL32(0, 0, 0, 200), 3.0f);
        }
        
        // Second pass: draw main line
        for (int i = 0; i < segments; i++)
        {
            float t0 = i / (float)segments;
            float t1 = (i + 1) / (float)segments;

            ImVec2 p0(
                (1 - t0) * (1 - t0) * start.x + 2 * (1 - t0) * t0 * control.x + t0 * t0 * end.x,
                (1 - t0) * (1 - t0) * start.y + 2 * (1 - t0) * t0 * control.y + t0 * t0 * end.y
            );

            ImVec2 p1(
                (1 - t1) * (1 - t1) * start.x + 2 * (1 - t1) * t1 * control.x + t1 * t1 * end.x,
                (1 - t1) * (1 - t1) * start.y + 2 * (1 - t1) * t1 * control.y + t1 * t1 * end.y
            );

            // Draw main line
            globals::instances::draw->AddLine(p0, p1, snapline_color, 1.0f);
        }
    }
}

static std::vector<math::Vector2> cached_skeleton_points;
static std::chrono::steady_clock::time_point last_skeleton_update;

void render_skeleton_enhanced(const CachedPlayerData& playerData, ImColor color) {
    if (!globals::visuals::skeletons || !playerData.valid || playerData.distance > 500.0f) return;

    roblox::player player = playerData.player;

    if (!is_valid_address(player.head.address) ||
        (!is_valid_address(player.lefthand.address) && !is_valid_address(player.leftupperarm.address)) ||
        (!is_valid_address(player.righthand.address) && !is_valid_address(player.rightupperarm.address)) ||
        (!is_valid_address(player.leftfoot.address) && !is_valid_address(player.leftupperleg.address)) ||
        (!is_valid_address(player.rightfoot.address) && !is_valid_address(player.rightupperleg.address)))
        return;

    auto dimensions = globals::instances::visualengine.get_dimensions();
    if (dimensions.x == 0 || dimensions.y == 0) return;

    auto on_screen = [&dimensions](const math::Vector2& pos) {
        return pos.x > 0 && pos.x < dimensions.x && pos.y > 0 && pos.y < dimensions.y;
        };

    if (player.rigtype == 0) {
        math::Vector2 head = roblox::worldtoscreen(player.head.get_pos());
        math::Vector2 torso = roblox::worldtoscreen(player.uppertorso.get_pos());
        math::Vector2 left_arm = roblox::worldtoscreen(player.lefthand.get_pos());
        math::Vector2 right_arm = roblox::worldtoscreen(player.righthand.get_pos());
        math::Vector2 left_leg = roblox::worldtoscreen(player.leftfoot.get_pos());
        math::Vector2 right_leg = roblox::worldtoscreen(player.rightfoot.get_pos());

        if (on_screen(head) && on_screen(torso)) {
            globals::instances::draw->AddLine(ImVec2(head.x, head.y), ImVec2(torso.x, torso.y), color, 1.5f);
        }
        if (on_screen(torso) && on_screen(left_arm)) {
            globals::instances::draw->AddLine(ImVec2(torso.x, torso.y), ImVec2(left_arm.x, left_arm.y), color, 1.5f);
        }
        if (on_screen(torso) && on_screen(right_arm)) {
            globals::instances::draw->AddLine(ImVec2(torso.x, torso.y), ImVec2(right_arm.x, right_arm.y), color, 1.5f);
        }
        if (on_screen(torso) && on_screen(left_leg)) {
            globals::instances::draw->AddLine(ImVec2(torso.x, torso.y), ImVec2(left_leg.x, left_leg.y), color, 1.5f);
        }
        if (on_screen(torso) && on_screen(right_leg)) {
            globals::instances::draw->AddLine(ImVec2(torso.x, torso.y), ImVec2(right_leg.x, right_leg.y), color, 1.5f);
        }
    }
    else {
        if (!is_valid_address(player.leftupperarm.address) ||
            !is_valid_address(player.rightupperarm.address) ||
            !is_valid_address(player.leftlowerarm.address) ||
            !is_valid_address(player.rightlowerarm.address) ||
            !is_valid_address(player.leftupperleg.address) ||
            !is_valid_address(player.rightupperleg.address) ||
            !is_valid_address(player.leftlowerleg.address) ||
            !is_valid_address(player.rightlowerleg.address))
            return;

        math::Vector2 head = roblox::worldtoscreen(player.head.get_pos());
        math::Vector2 upper_torso = roblox::worldtoscreen(player.uppertorso.get_pos());
        math::Vector2 lower_torso = roblox::worldtoscreen(player.lowertorso.get_pos());
        math::Vector2 left_upper_arm = roblox::worldtoscreen(player.leftupperarm.get_pos());
        math::Vector2 right_upper_arm = roblox::worldtoscreen(player.rightupperarm.get_pos());
        math::Vector2 left_lower_arm = roblox::worldtoscreen(player.leftlowerarm.get_pos());
        math::Vector2 right_lower_arm = roblox::worldtoscreen(player.rightlowerarm.get_pos());
        math::Vector2 left_hand = roblox::worldtoscreen(player.lefthand.get_pos());
        math::Vector2 right_hand = roblox::worldtoscreen(player.righthand.get_pos());
        math::Vector2 left_upper_leg = roblox::worldtoscreen(player.leftupperleg.get_pos());
        math::Vector2 right_upper_leg = roblox::worldtoscreen(player.rightupperleg.get_pos());
        math::Vector2 left_lower_leg = roblox::worldtoscreen(player.leftlowerleg.get_pos());
        math::Vector2 right_lower_leg = roblox::worldtoscreen(player.rightlowerleg.get_pos());
        math::Vector2 left_foot = roblox::worldtoscreen(player.leftfoot.get_pos());
        math::Vector2 right_foot = roblox::worldtoscreen(player.rightfoot.get_pos());

        if (on_screen(head) && on_screen(upper_torso)) {
            head.y -= 1.5f;
            globals::instances::draw->AddLine(ImVec2(head.x, head.y), ImVec2(upper_torso.x, upper_torso.y), color, 1.5f);
        }
        if (on_screen(upper_torso) && on_screen(lower_torso)) {
            lower_torso.y += 1.5f;
            globals::instances::draw->AddLine(ImVec2(upper_torso.x, upper_torso.y), ImVec2(lower_torso.x, lower_torso.y), color, 1.5f);
        }
        if (on_screen(upper_torso) && on_screen(left_upper_arm)) {
            globals::instances::draw->AddLine(ImVec2(upper_torso.x, upper_torso.y), ImVec2(left_upper_arm.x, left_upper_arm.y), color, 1.5f);
        }
        if (on_screen(upper_torso) && on_screen(right_upper_arm)) {
            globals::instances::draw->AddLine(ImVec2(upper_torso.x, upper_torso.y), ImVec2(right_upper_arm.x, right_upper_arm.y), color, 1.5f);
        }
        if (on_screen(left_upper_arm) && on_screen(left_lower_arm)) {
            globals::instances::draw->AddLine(ImVec2(left_upper_arm.x, left_upper_arm.y), ImVec2(left_lower_arm.x, left_lower_arm.y), color, 1.5f);
        }
        if (on_screen(right_upper_arm) && on_screen(right_lower_arm)) {
            globals::instances::draw->AddLine(ImVec2(right_upper_arm.x, right_upper_arm.y), ImVec2(right_lower_arm.x, right_lower_arm.y), color, 1.5f);
        }
        if (on_screen(left_lower_arm) && on_screen(left_hand)) {
            globals::instances::draw->AddLine(ImVec2(left_lower_arm.x, left_lower_arm.y), ImVec2(left_hand.x, left_hand.y), color, 1.5f);
        }
        if (on_screen(right_lower_arm) && on_screen(right_hand)) {
            globals::instances::draw->AddLine(ImVec2(right_lower_arm.x, right_lower_arm.y), ImVec2(right_hand.x, right_hand.y), color, 1.5f);
        }
        if (on_screen(lower_torso) && on_screen(left_upper_leg)) {
            globals::instances::draw->AddLine(ImVec2(lower_torso.x, lower_torso.y), ImVec2(left_upper_leg.x, left_upper_leg.y), color, 1.5f);
        }
        if (on_screen(lower_torso) && on_screen(right_upper_leg)) {
            globals::instances::draw->AddLine(ImVec2(lower_torso.x, lower_torso.y), ImVec2(right_upper_leg.x, right_upper_leg.y), color, 1.5f);
        }
        if (on_screen(left_upper_leg) && on_screen(left_lower_leg)) {
            globals::instances::draw->AddLine(ImVec2(left_upper_leg.x, left_upper_leg.y), ImVec2(left_lower_leg.x, left_lower_leg.y), color, 1.5f);
        }
        if (on_screen(right_upper_leg) && on_screen(right_lower_leg)) {
            globals::instances::draw->AddLine(ImVec2(right_upper_leg.x, right_upper_leg.y), ImVec2(right_lower_leg.x, right_lower_leg.y), color, 1.5f);
        }
        if (on_screen(left_lower_leg) && on_screen(left_foot)) {
            globals::instances::draw->AddLine(ImVec2(left_lower_leg.x, left_lower_leg.y), ImVec2(left_foot.x, left_foot.y), color, 1.5f);
        }
        if (on_screen(right_lower_leg) && on_screen(right_foot)) {
            globals::instances::draw->AddLine(ImVec2(right_lower_leg.x, right_lower_leg.y), ImVec2(right_foot.x, right_foot.y), color, 1.5f);
        }
    }
}

void draw_text_with_shadow_enhanced(float font_size, const ImVec2& position, ImColor color, const char* text) {
    ImFont* font = ImGui::GetIO().Fonts->Fonts[0];
    if (font) {
        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                if (i == 0 && j == 0) continue;
                globals::instances::draw->AddText(font, font_size, ImVec2(position.x + i, position.y + j), ImColor(0, 0, 0, 255), text);
            }
        }
        globals::instances::draw->AddText(font, font_size, position, color, text);
    }
    else {
        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                if (i == 0 && j == 0) continue;
                globals::instances::draw->AddText(0, font_size, ImVec2(position.x + i, position.y + j), ImColor(0, 0, 0, 255), text);
            }
        }
        globals::instances::draw->AddText(0, font_size, position, color, text);
    }
}

void render_offscreen_arrow_enhanced(const math::Vector3& player_pos, const math::Vector3& local_pos, ImColor color, const std::string& player_name = "") {
    if (!globals::visuals::oofarrows) return;

    auto dimensions = globals::instances::visualengine.get_dimensions();
    if (dimensions.x == 0 || dimensions.y == 0) return;

    ImVec2 screen_center(dimensions.x / 2.f, dimensions.y / 2.f);
    auto view_matrix = globals::instances::visualengine.GetViewMatrix();
    auto camerarotation = globals::instances::camera.getRot();

    math::Vector3 camera_forward = { -camerarotation.data[2], -camerarotation.data[5], -camerarotation.data[8] };

    math::Vector3 dir_to_player = { player_pos.x - local_pos.x, player_pos.y - local_pos.y, player_pos.z - local_pos.z };
    float distance = vector_length(dir_to_player);
    if (distance < 1.0f) return;

    dir_to_player.x /= distance;
    dir_to_player.y /= distance;
    dir_to_player.z /= distance;

    math::Vector3 camera_right = { camerarotation.data[0], camerarotation.data[3], camerarotation.data[6] };
    math::Vector3 camera_up = { camerarotation.data[1], camerarotation.data[4], camerarotation.data[7] };

    float forward_dot = dir_to_player.x * camera_forward.x + dir_to_player.y * camera_forward.y + dir_to_player.z * camera_forward.z;
    if (forward_dot > 0.98f) return;

    float right_dot = dir_to_player.x * camera_right.x + dir_to_player.y * camera_right.y + dir_to_player.z * camera_right.z;
    float up_dot = dir_to_player.x * camera_up.x + dir_to_player.y * camera_up.y + dir_to_player.z * camera_up.z;

    ImVec2 screen_dir(right_dot, -up_dot);
    float screen_dir_len = sqrtf(screen_dir.x * screen_dir.x + screen_dir.y * screen_dir.y);
    if (screen_dir_len < 0.001f) return;

    screen_dir.x /= screen_dir_len;
    screen_dir.y /= screen_dir_len;

    const float radius = 150.0f;
    ImVec2 pos(
        screen_center.x + screen_dir.x * radius,
        screen_center.y + screen_dir.y * radius
    );

    float angle = atan2f(screen_dir.y, screen_dir.x);
    const float arrow_size = 10.0f;
    const float arrow_angle = 0.6f;

    ImVec2 point1(
        pos.x - arrow_size * cosf(angle - arrow_angle),
        pos.y - arrow_size * sinf(angle - arrow_angle)
    );
    ImVec2 point2(
        pos.x - arrow_size * cosf(angle + arrow_angle),
        pos.y - arrow_size * sinf(angle + arrow_angle)
    );

    ImColor arrow_color = ImGui::ColorConvert(globals::visuals::oofcolor);


    globals::instances::draw->AddTriangleFilled(pos, point1, point2, arrow_color);
    globals::instances::draw->AddTriangle(pos, point1, point2, ImColor(0, 0, 0, 255), 0.5f);

    if (!player_name.empty()) {
        char distance_text[32];
        sprintf_s(distance_text, "%.0fm", distance);

        ImVec2 text_pos(pos.x - 15, pos.y + 15);
        draw_text_with_shadow_enhanced(10.0f, text_pos, arrow_color, distance_text);
    }
}

float cross_product_2d(const ImVec2& p, const ImVec2& q, const ImVec2& r) {
    return (q.x - p.x) * (r.y - p.y) - (q.y - p.y) * (r.x - p.x);
}

std::vector<ImVec2> convexHull(std::vector<ImVec2>& points) {
    int n = points.size();
    if (n <= 3) return points;

    int bottommost_index = 0;
    for (int i = 1; i < n; ++i) {
        if (points[i].y < points[bottommost_index].y || (points[i].y == points[bottommost_index].y && points[i].x < points[bottommost_index].x)) {
            bottommost_index = i;
        }
    }
    std::swap(points[0], points[bottommost_index]);
    ImVec2 p0 = points[0];

    std::sort(points.begin() + 1, points.end(), [&](const ImVec2& a, const ImVec2& b) {
        float cross = cross_product_2d(p0, a, b);
        if (cross == 0) {
            float distSqA = (p0.x - a.x) * (p0.x - a.x) + (p0.y - a.y) * (p0.y - a.y);
            float distSqB = (p0.x - b.x) * (p0.x - b.x) + (p0.y - b.y) * (p0.y - b.y);
            return distSqA < distSqB;
        }
        return cross > 0;
        });

    std::vector<ImVec2> hull;
    hull.push_back(points[0]);
    hull.push_back(points[1]);

    for (int i = 2; i < n; ++i) {
        while (hull.size() > 1 && cross_product_2d(hull[hull.size() - 2], hull.back(), points[i]) <= 0) {
            hull.pop_back();
        }
        hull.push_back(points[i]);
    }

    return hull;
}

math::Vector3 Rotate(const math::Vector3& vec, const math::Matrix3& rotation_matrix) {
    return math::Vector3{
        vec.x * rotation_matrix.data[0] + vec.y * rotation_matrix.data[1] + vec.z * rotation_matrix.data[2],
        vec.x * rotation_matrix.data[3] + vec.y * rotation_matrix.data[4] + vec.z * rotation_matrix.data[5],
        vec.x * rotation_matrix.data[6] + vec.y * rotation_matrix.data[7] + vec.z * rotation_matrix.data[8]
    };
}

std::vector<math::Vector3> GetCorners(const math::Vector3& partCF, const math::Vector3& partSize) {
    std::vector<math::Vector3> corners;
    corners.reserve(8);

    for (int X : {-1, 1}) {
        for (int Y : {-1, 1}) {
            for (int Z : {-1, 1}) {
                corners.emplace_back(
                    partCF.x + partSize.x * X,
                    partCF.y + partSize.y * Y,
                    partCF.z + partSize.z * Z
                );
            }
        }
    }
    return corners;
}

namespace esp_helpers {
    bool on_screen(const math::Vector2& pos) {
        auto dimensions = globals::instances::visualengine.get_dimensions();
        if (dimensions.x == -1 || dimensions.y == -1) return false;
        return pos.x > -1 && pos.x < dimensions.x && pos.y > -1 && pos.y - 28 < dimensions.y;
    }

    bool get_player_bounds(roblox::player& player, math::Vector2& top_left, math::Vector2& bottom_right, float& margin) {
        std::vector<roblox::instance> parts;
        if (player.rigtype == 0) { // R6
            parts = {
                player.head, player.uppertorso, player.leftfoot,
                player.rightfoot, player.lefthand, player.righthand
            };
        }
        else { // R15
            parts = {
                player.head, player.uppertorso, player.lowertorso,
                player.leftupperarm, player.rightupperarm, player.leftlowerarm,
                player.rightlowerarm, player.lefthand, player.righthand,
                player.leftupperleg, player.rightupperleg, player.leftlowerleg,
                player.rightlowerleg, player.leftfoot, player.rightfoot
            };
        }

        float min_x = FLT_MAX, min_y = FLT_MAX;
        float max_x = -FLT_MAX, max_y = -FLT_MAX;
        bool at_least_one_point_on_screen = false;

        for (auto& part : parts) {
            if (!is_valid_address(part.address)) continue;

            math::Vector3 part_size = part.get_part_size();
            math::Vector3 part_pos = part.get_pos();
            math::Matrix3 part_rotation = part.read_cframe().rotation;

            std::vector<math::Vector3> corners = GetCorners({ 0,0,0 }, { part_size.x / 2, part_size.y / 2, part_size.z / 2 });
            for (auto& corner : corners) {
                corner = Rotate(corner, part_rotation);
                corner = { corner.x + part_pos.x, corner.y + part_pos.y, corner.z + part_pos.z };

                math::Vector2 screen_pos = roblox::worldtoscreen(corner);
                if (on_screen(screen_pos)) {
                    at_least_one_point_on_screen = true;
                    min_x = std::min(min_x, screen_pos.x);
                    min_y = std::min(min_y, screen_pos.y);
                    max_x = std::max(max_x, screen_pos.x);
                    max_y = std::max(max_y, screen_pos.y);
                }
            }
        }

        if (!at_least_one_point_on_screen) {
            return false;
        }

        top_left = { min_x, min_y };
        bottom_right = { max_x, max_y };

        float height = bottom_right.y - top_left.y;
        margin = height * 0.1f;
        top_left.y -= margin;
        bottom_right.y += margin / 4;

        float width = bottom_right.x - top_left.x;
        margin = width * 0.1f;
        top_left.x -= margin;
        bottom_right.x += margin;

        return true;
    }
}

void visuals::updateESP() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    while (visuals::threadRunning) {
        try {
            std::this_thread::sleep_for(std::chrono::microseconds(5));
            // Only update ESP if visuals are enabled and the application is focused
            if (!globals::visuals::visuals || !globals::focused) {
                std::this_thread::sleep_for(std::chrono::microseconds(7500));
                continue;
            }

            if (!is_valid_address(globals::instances::lp.hrp.address)) {
                std::this_thread::sleep_for(std::chrono::microseconds(7500));
                continue;
            }

            std::vector<CachedPlayerData> tempPlayers;
            math::Vector3 local_pos = globals::instances::camera.getPos();
            
            globals::instances::cachedplayers_mutex.lock(); // Lock mutex before accessing cachedplayers
            visuals::espData.cachedmutex.lock(); // Keep existing mutex for espData

            // Removed: globals::bools::player_status.clear(); // Clear player status to prevent stale data
            // Don't clear target_only_list when feature is disabled - let user manage it manually

            auto& players = globals::instances::cachedplayers;
            auto& bots = globals::instances::bots;

            std::vector<roblox::player> all_entities;
            all_entities.insert(all_entities.end(), players.begin(), players.end());

            for (auto& bot_head : bots) {
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
                            roblox::instance upperTorso = npc_model.findfirstchild("UpperTorso");
                            roblox::instance lowerTorso = npc_model.findfirstchild("LowerTorso");
                            roblox::instance torso = npc_model.findfirstchild("Torso");

                            // Detect rig type and fill common body parts so bounding boxes match players
                            bool usesR15 = upperTorso.is_valid() || lowerTorso.is_valid();
                            bot_player.rigtype = usesR15 ? 1 : 0;

                            bot_player.uppertorso = upperTorso.is_valid() ? upperTorso : (torso.is_valid() ? torso : bot_player.hrp);
                            bot_player.lowertorso = lowerTorso.is_valid() ? lowerTorso : (upperTorso.is_valid() ? upperTorso : bot_player.hrp);

                            // R15 limbs
                            bot_player.leftupperarm = npc_model.findfirstchild("LeftUpperArm");
                            bot_player.rightupperarm = npc_model.findfirstchild("RightUpperArm");
                            bot_player.leftlowerarm = npc_model.findfirstchild("LeftLowerArm");
                            bot_player.rightlowerarm = npc_model.findfirstchild("RightLowerArm");
                            bot_player.lefthand = npc_model.findfirstchild("LeftHand");
                            bot_player.righthand = npc_model.findfirstchild("RightHand");
                            bot_player.leftupperleg = npc_model.findfirstchild("LeftUpperLeg");
                            bot_player.rightupperleg = npc_model.findfirstchild("RightUpperLeg");
                            bot_player.leftlowerleg = npc_model.findfirstchild("LeftLowerLeg");
                            bot_player.rightlowerleg = npc_model.findfirstchild("RightLowerLeg");
                            bot_player.leftfoot = npc_model.findfirstchild("LeftFoot");
                            bot_player.rightfoot = npc_model.findfirstchild("RightFoot");

                            // R6 fallbacks
                            roblox::instance leftArm = npc_model.findfirstchild("Left Arm");
                            roblox::instance rightArm = npc_model.findfirstchild("Right Arm");
                            roblox::instance leftLeg = npc_model.findfirstchild("Left Leg");
                            roblox::instance rightLeg = npc_model.findfirstchild("Right Leg");

                            if (!usesR15) {
                                bot_player.uppertorso = torso.is_valid() ? torso : bot_player.hrp;
                                bot_player.lowertorso = bot_player.uppertorso;

                                bot_player.leftupperarm = leftArm.is_valid() ? leftArm : bot_player.uppertorso;
                                bot_player.rightupperarm = rightArm.is_valid() ? rightArm : bot_player.uppertorso;
                                bot_player.leftlowerarm = bot_player.leftupperarm;
                                bot_player.rightlowerarm = bot_player.rightupperarm;
                                bot_player.lefthand = bot_player.leftupperarm;
                                bot_player.righthand = bot_player.rightupperarm;

                                bot_player.leftupperleg = leftLeg.is_valid() ? leftLeg : bot_player.hrp;
                                bot_player.rightupperleg = rightLeg.is_valid() ? rightLeg : bot_player.hrp;
                                bot_player.leftlowerleg = bot_player.leftupperleg;
                                bot_player.rightlowerleg = bot_player.rightupperleg;
                                bot_player.leftfoot = bot_player.leftupperleg;
                                bot_player.rightfoot = bot_player.rightupperleg;
                            }

							bot_player.instance = npc_model;
							bot_player.main = npc_model;
							bot_player.userid.address = 0;
							bot_player.health = humanoid.read_health();
							bot_player.maxhealth = humanoid.read_maxhealth();
							all_entities.push_back(bot_player);
						}
                    }
                }
            }

            for (auto& player : all_entities) {
                if (!is_valid_address(player.main.address) || player.name == globals::instances::lp.name)
                    continue;

                if (!is_valid_address(player.hrp.address))
                    continue;

                // Filter out teammates when teamcheck is on or in Arsenal
                if (((*globals::visuals::visuals_flags)[0] != 0) || globals::visuals::teamcheck || (globals::instances::gamename == "Arsenal")) {
                    if (teamcheck::same_team(globals::instances::lp, player)) continue;
                }

                // Skip if target-only ESP is enabled and player is not in target list
                if (globals::visuals::target_only_esp && !globals::visuals::target_only_list.empty()) {
                    auto it = std::find(globals::visuals::target_only_list.begin(), 
                                      globals::visuals::target_only_list.end(),
                                      player.name);
                    if (it == globals::visuals::target_only_list.end()) continue;
                }

                try {
                    CachedPlayerData data;
                    data.player = player;
                    data.position = player.hrp.get_pos();
                    data.distance = (local_pos - data.position).magnitude();

                    // Populate player.displayname correctly using main player + humanoid fallbacks
                    bool isBot = (player.name == "NPC");
                    if (isBot) {
                        player.displayname = "NPC";
                        data.name = "NPC";
                    } else {
                        auto sanitize = [](std::string value) {
                            value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) { return !std::isspace(ch); }));
                            value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), value.end());
                            return value;
                        };
                        auto is_valid_display = [](const std::string& candidate, const std::string& username) {
                            if (candidate.empty()) return false;
                            std::string lower = candidate;
                            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                            if (lower == "humanoid") return false;
                            std::string user_lower = username;
                            std::transform(user_lower.begin(), user_lower.end(), user_lower.begin(), ::tolower);
                            return lower != user_lower;
                        };

                        std::string username = player.name;
                        std::string display = sanitize(player.displayname);
                        if (!is_valid_display(display, username)) {
                            std::string main_display = sanitize(player.main.get_displayname());
                            if (is_valid_display(main_display, username)) display = main_display;
                        }
                        if (!is_valid_display(display, username)) {
                            std::string hum_display = sanitize(player.humanoid.get_humdisplayname());
                            if (is_valid_display(hum_display, username)) display = hum_display;
                        }
                        if (!is_valid_display(display, username)) {
                            display = username;
                        }
                        player.displayname = display;

                        if (globals::visuals::nametype == 0) // User wants username
                            data.name = player.name; // Assign username (e.g., "BoatsBox")
                        else // User wants display name
                            data.name = player.displayname; // Assign display name (e.g., "Kymchea")
                    }

                    data.tool_name = player.toolname;
                    float current_health = 0.0f;
                    float max_health = 0.0f;
                    if (globals::instances::gamename == "Arsenal") {
                        // Use cached values populated with NRPBS-aware reads in hook
                        current_health = static_cast<float>(player.health);
                        max_health = static_cast<float>(player.maxhealth);
                    } else {
                        current_health = read<float>(player.humanoid.address + offsets::Health);
                        max_health = read<float>(player.humanoid.address + offsets::MaxHealth);
                    }

                    if (max_health > 0) {
                        data.health = current_health / max_health;
                    } else {
                        data.health = 0.0f; // Prevent division by zero, set health to 0 if max_health is invalid
                    }
                    player.health = current_health; // Update player's health
                    player.maxhealth = max_health; // Update player's max health
                math::Vector2 top_left, bottom_right;
                float margin;
                data.valid = esp_helpers::get_player_bounds(player, top_left, bottom_right, margin);

                    if (data.valid) {
                        // In Arsenal, filter out dead players from ESP
                        if (globals::instances::gamename == "Arsenal" && current_health <= 0.0f) {
                            continue;
                        }
                        data.top_left = top_left;
                        data.bottom_right = bottom_right;
                        data.margin = margin;
                        tempPlayers.push_back(data);
                    }
                }
                catch (...) {
                    continue;
                }
            }

            if (!tempPlayers.empty()) {
                std::this_thread::sleep_for(std::chrono::microseconds(1));
                std::sort(tempPlayers.begin(), tempPlayers.end(),
                    [](const CachedPlayerData& a, const CachedPlayerData& b) {
                        return a.distance < b.distance;
                    });

                visuals::espData.cachedPlayers = std::move(tempPlayers);
                visuals::espData.localPos = local_pos;
            }
        }
        catch (...) {
        }
        visuals::espData.cachedmutex.unlock();
        globals::instances::cachedplayers_mutex.unlock(); // Unlock mutex after accessing cachedplayers
        std::this_thread::sleep_for(std::chrono::microseconds(5));
    }
}

void visuals::apply_fog_settings() {
    if (!globals::instances::lighting.address) return;

    if (globals::visuals::fog_changer) {
        if (!original_fog_settings_saved) {
            original_fog_start = globals::instances::lighting.read_fogstart();
            original_fog_end = globals::instances::lighting.read_fogend();
            original_fog_color = globals::instances::lighting.read_fogcolor();
            original_fog_settings_saved = true;
        }
        globals::instances::lighting.write_fogstart(globals::visuals::fog_start);
        globals::instances::lighting.write_fogend(globals::visuals::fog_end);
        globals::instances::lighting.write_fogcolor(math::Vector3(globals::visuals::fog_color[0], globals::visuals::fog_color[1], globals::visuals::fog_color[2]));
    }
    else {
        if (original_fog_settings_saved) {
            globals::instances::lighting.write_fogstart(original_fog_start);
            globals::instances::lighting.write_fogend(original_fog_end);
            globals::instances::lighting.write_fogcolor(original_fog_color);
            original_fog_settings_saved = false;
        }
    }
}

struct Polygon_t {
    std::vector<ImVec2> vertices;
    float depth;
};
static std::mutex chamsMutex;
static std::vector<Polygon_t> polys;

void utils::CacheChamsPoly() {
    while (true) {
        if (!globals::visuals::visuals || !globals::visuals::chams) {
            std::this_thread::sleep_for(std::chrono::microseconds(10000));
            continue;
        }

        std::vector<Polygon_t> temp_polygons;
        auto& playerlist = globals::instances::cachedplayers;
        if (playerlist.empty()) {
            std::this_thread::sleep_for(std::chrono::microseconds(5000));
            continue;
        }
        auto& local_player = playerlist[0];

        for (roblox::player& player : playerlist) {
            if (!local_player.main.address) continue;
            if (player.main.address == local_player.main.address) continue;

            std::vector<roblox::instance> parts_chams;
            switch (player.rigtype) {
            case 0:
                parts_chams = {
                    player.head,
                    player.uppertorso,
                    player.leftfoot,
                    player.rightfoot,
                    player.lefthand,
                    player.righthand
                };
                break;
            case 1:
                parts_chams = {
                    player.head,
                    player.uppertorso,
                    player.lowertorso,
                    player.leftupperarm,
                    player.rightupperarm,
                    player.leftlowerleg,
                    player.rightlowerleg,
                    player.leftfoot,
                    player.rightfoot,
                    player.leftupperleg,
                    player.rightupperleg,
                    player.leftlowerarm,
                    player.rightlowerarm,
                    player.lefthand,
                    player.righthand
                };
                break;
            default:
                continue;
            }

            // Per-part projection to hull, similar to provided chams snippet
            for (auto& part : parts_chams) {
                try {
                    math::Vector3 part_size = part.get_part_size();
                    math::Vector3 part_pos = part.get_pos();
                    math::Matrix3 part_rotation = part.read_cframe().rotation;

                    std::vector<math::Vector3> cube_vertices = GetCorners(
                        { 0, 0, 0 },
                        { part_size.x / 2, part_size.y / 2, part_size.z / 2 }
                    );

                    for (auto& vertex : cube_vertices) {
                        vertex = Rotate(vertex, part_rotation);
                        vertex = { vertex.x + part_pos.x, vertex.y + part_pos.y, vertex.z + part_pos.z };
                    }
                    std::vector<ImVec2> projected;
                    projected.reserve(cube_vertices.size());
                    float depth_sum = 0.0f;

                    for (const auto& vertex : cube_vertices) {
                        math::Vector2 screen = roblox::worldtoscreen(vertex);
                        // Allow partially off-screen projections but require valid coordinates
                        if (screen.x < 0.f || screen.y < 0.f)
                            continue;
                        projected.emplace_back(ImVec2(screen.x, screen.y));
                        depth_sum += vertex.z;
                    }

                    if (projected.size() < 3)
                        continue;

                    std::sort(projected.begin(), projected.end(), [](const ImVec2& a, const ImVec2& b) {
                        return a.x < b.x || (a.x == b.x && a.y < b.y);
                        });

                    std::vector<ImVec2> hull;
                    auto cross = [](const ImVec2& O, const ImVec2& A, const ImVec2& B) {
                        return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
                        };

                    for (auto& p : projected) {
                        while (hull.size() >= 2 && cross(hull[hull.size() - 2], hull[hull.size() - 1], p) <= 0)
                            hull.pop_back();
                        hull.push_back(p);
                    }

                    size_t t = hull.size() + 1;
                    for (int i = static_cast<int>(projected.size()) - 1; i >= 0; --i) {
                        auto& p = projected[i];
                        while (hull.size() >= t && cross(hull[hull.size() - 2], hull[hull.size() - 1], p) <= 0)
                            hull.pop_back();
                        hull.push_back(p);
                    }

                    if (!hull.empty())
                        hull.pop_back();

                    if (hull.size() >= 3) {
                        temp_polygons.emplace_back(Polygon_t{ hull, depth_sum / projected.size() });
                    }
                }
                catch (...) {
                    continue;
                }
            }
        }

        if (!temp_polygons.empty()) {
            std::sort(temp_polygons.begin(), temp_polygons.end(), [](const Polygon_t& a, const Polygon_t& b) {
                return a.depth > b.depth;
                });
            std::lock_guard<std::mutex> lock(chamsMutex);
            polys = std::move(temp_polygons);
        }
        else {
            std::lock_guard<std::mutex> lock(chamsMutex);
            polys.clear();
        }

        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
}

void render_chams_enhanced() {
    if (!globals::visuals::chams || !globals::instances::draw)
        return;

    std::vector<Polygon_t> polygons_to_draw;
    {
        std::lock_guard<std::mutex> lock(chamsMutex);
        polygons_to_draw = polys;
    }

    if (polygons_to_draw.empty())
        return;

    globals::instances::draw->_FringeScale = 0.f;
    ImDrawListFlags oldFlags = globals::instances::draw->Flags;

    ImColor fillColor(
        globals::visuals::chamscolor[0],
        globals::visuals::chamscolor[1],
        globals::visuals::chamscolor[2],
        globals::visuals::chamscolor[3]
    );

    ImColor outlineColor(0.0f, 0.0f, 0.0f, globals::visuals::chamscolor[3]);

    // Ensure overlay flags have expected size (outline, fill)
    if (globals::visuals::chams_overlay_flags->size() < 2) {
        globals::visuals::chams_overlay_flags->resize(2, 0);
    }

    for (const auto& polygon : polygons_to_draw) {
        if (polygon.vertices.size() < 3)
            continue;

        if ((*globals::visuals::chams_overlay_flags)[1]) {
            globals::instances::draw->AddConvexPolyFilled(
                polygon.vertices.data(),
                polygon.vertices.size(),
                fillColor
            );
        }
        if ((*globals::visuals::chams_overlay_flags)[0]) {
            globals::instances::draw->AddPolyline(
                polygon.vertices.data(),
                polygon.vertices.size(),
                outlineColor,
                true,
                1.5f
            );
        }
    }
    globals::instances::draw->Flags = oldFlags;
}

static void render_sonar_enhanced() {
    if (!globals::visuals::sonar) return;
    if (!is_valid_address(globals::instances::lp.hrp.address)) return;

    ImDrawList* draw_list = globals::instances::draw;
    if (!draw_list) return;

    auto screen_dimensions = globals::instances::visualengine.get_dimensions();
    if (screen_dimensions.x == 0 || screen_dimensions.y == 0) return;

    math::Vector3 humanoid_pos = globals::instances::lp.hrp.get_pos();

    static auto start_time = std::chrono::steady_clock::now();
    auto current_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
    float time_seconds = elapsed / 1000.0f;

    static float last_scan_cycle = 0.0f;

    float scan_cycle = std::fmod(time_seconds * globals::visuals::sonar_speed, 1.0f);

    if (scan_cycle < last_scan_cycle && scan_cycle < 0.1f) {
    }
    last_scan_cycle = scan_cycle;

    float expansion_progress = scan_cycle;
    float current_radius = expansion_progress * globals::visuals::sonar_range;
    float circle_alpha = 0.9f * (1.0f - expansion_progress);

    ImColor circle_color(
        globals::visuals::sonarcolor[0],
        globals::visuals::sonarcolor[1],
        globals::visuals::sonarcolor[2],
        globals::visuals::sonarcolor[3] * circle_alpha
    );

    if (globals::visuals::sonar_detect_players) {
        float nearest = std::numeric_limits<float>::max();
        bool any_in_range = false;
        static float last_time = 0.0f;
        static float mix_value = 0.0f;

        float dt = time_seconds - last_time;
        if (dt < 0.0f || dt > 1.0f) dt = 0.016f; // fallback
        last_time = time_seconds;

        {
            std::lock_guard<std::mutex> lock(visuals::espData.cachedmutex);
            for (const auto& player : visuals::espData.cachedPlayers) {
                if (!player.valid) continue;
                if (player.name == globals::instances::lp.name) continue;
                if (globals::visuals::target_only_esp && !globals::visuals::target_only_list.empty()) {
                    auto it = std::find(globals::visuals::target_only_list.begin(), globals::visuals::target_only_list.end(), player.name);
                    if (it == globals::visuals::target_only_list.end()) continue;
                }
                math::Vector3 delta = player.position - humanoid_pos;
                float distance = vector_length(delta);
                if (distance < nearest) nearest = distance;
                if (distance <= globals::visuals::sonar_range) any_in_range = true;
            }
        }

        // Distance factor 0 -> far, 1 -> at or inside radius
        float t = 0.0f;
        if (nearest < std::numeric_limits<float>::max()) {
            float norm = nearest / std::max(globals::visuals::sonar_range, 1.0f);
            norm = std::clamp(norm, 0.0f, 1.0f);
            t = 1.0f - norm;
        }
        if (any_in_range) t = 1.0f;

        // Fade smoothly toward target color state
        float target = t;
        float lerp_speed = 5.0f; // how quickly we fade between states
        mix_value = std::clamp(mix_value + (target - mix_value) * std::clamp(dt * lerp_speed, 0.0f, 1.0f), 0.0f, 1.0f);
        t = mix_value;

        // Smooth fade curve for nicer transition
        t = std::clamp(t, 0.0f, 1.0f);
        t = t * t * (3.0f - 2.0f * t);

        auto lerp = [](float a, float b, float k) { return a + (b - a) * k; };
        float out_a = globals::visuals::sonar_detect_color_out[3] * circle_alpha;
        float in_a = globals::visuals::sonar_detect_color_in[3] * circle_alpha;
        float r = lerp(globals::visuals::sonar_detect_color_out[0], globals::visuals::sonar_detect_color_in[0], t);
        float g = lerp(globals::visuals::sonar_detect_color_out[1], globals::visuals::sonar_detect_color_in[1], t);
        float b = lerp(globals::visuals::sonar_detect_color_out[2], globals::visuals::sonar_detect_color_in[2], t);
        float a = lerp(out_a, in_a, t);
        circle_color = ImColor(r, g, b, a);
    }

    const int num_segments = 64;
    const float two_pi = 6.28318530718f;
    std::vector<ImVec2> screen_points;
    screen_points.reserve(num_segments + 1);

    math::Vector2 center_screen = roblox::worldtoscreen(humanoid_pos);
    if (!std::isfinite(center_screen.x) || !std::isfinite(center_screen.y)) {
        return;
    }

    for (int i = 0; i <= num_segments; i++) {
        float angle = (two_pi * i) / num_segments;

        math::Vector3 circle_point(
            humanoid_pos.x + std::cos(angle) * current_radius,
            humanoid_pos.y - 3.0f,
            humanoid_pos.z + std::sin(angle) * current_radius
        );

        math::Vector2 screen_point = roblox::worldtoscreen(circle_point);

        if (screen_point.x == -1 && screen_point.y == -1) {
            continue;
        }

        if (std::isfinite(screen_point.x) && std::isfinite(screen_point.y) &&
            screen_point.x > -1000 && screen_point.x < screen_dimensions.x + 1000 &&
            screen_point.y > -1000 && screen_point.y < screen_dimensions.y + 1000) {
            screen_points.push_back(ImVec2(screen_point.x, screen_point.y));
        }
    }

    if (screen_points.size() >= 8) {
        for (size_t i = 0; i + 1 < screen_points.size(); i++) {
            float dx = screen_points[i + 1].x - screen_points[i].x;
            float dy = screen_points[i + 1].y - screen_points[i].y;
            float distance = sqrtf(dx * dx + dy * dy);

            if (distance < 500.0f) {
                draw_list->AddLine(
                    screen_points[i],
                    screen_points[i + 1],
                    circle_color,
                    globals::visuals::sonar_thickness * (1.0f + expansion_progress * 0.3f)
                );
            }
        }

        if (screen_points.size() > 2) {
            float dx = screen_points.front().x - screen_points.back().x;
            float dy = screen_points.front().y - screen_points.back().y;
            float distance = sqrtf(dx * dx + dy * dy);

            if (distance < 500.0f) {
                draw_list->AddLine(
                    screen_points.back(),
                    screen_points.front(),
                    circle_color,
                    globals::visuals::sonar_thickness * (1.0f + expansion_progress * 0.3f)
                );
            }
        }
    }

}

void visuals::stopThread() {
    if (threadRunning) {
        threadRunning = false;
        if (updateThread.joinable()) {
            updateThread.join();
        }
    }
}

void visuals::run() {
    if (!threadRunning) {
        updateThread = std::thread(visuals::updateESP);
        updateThread.detach();
        std::thread updatechams(utils::CacheChamsPoly);
        updatechams.detach();
        threadRunning = true;
    }

    // If visuals are not enabled or the application is not focused, do not render any ESP elements.
    if (!globals::visuals::visuals || !globals::instances::draw || !globals::focused || !keybind::is_roblox_focused()) {
        return;
    }

    auto screen_dimensions = globals::instances::visualengine.get_dimensions();
    if (screen_dimensions.x == 0 || screen_dimensions.y == 0) {
        return;
    }

    render_chams_enhanced();
    visuals::espData.cachedmutex.lock();

    for (auto& player : visuals::espData.cachedPlayers) {
        if (!player.valid || player.distance > globals::visuals::visual_range)
            continue;

        if (globals::visuals::target_only_esp) {
            if (!globals::visuals::target_only_list.empty()) {
                auto it = std::find(globals::visuals::target_only_list.begin(), globals::visuals::target_only_list.end(), player.name);
                if (it == globals::visuals::target_only_list.end()) {
                    continue; // Skip if target_only_esp is true and player is not in the list
                }
            }
            // If target_only_esp is true but the list is empty, show all players.
            // No 'continue' here means all players will be processed if the list is empty.
        }

        math::Vector2 screen_check = roblox::worldtoscreen(player.position);
        if (screen_check.x <= 0 || screen_check.x >= screen_dimensions.x ||
            screen_check.y <= 0 || screen_check.y >= screen_dimensions.y) {
            if (player.distance > 100.0f) {
                continue;
            }
        }

        math::Vector2 top_left = player.top_left;
        math::Vector2 bottom_right = player.bottom_right;

        if (!esp_helpers::on_screen(top_left) || !esp_helpers::on_screen(bottom_right))
            continue;

        ImColor player_color = ImColor(255, 255, 255);

        if (globals::visuals::boxes) {
            player_color = ImGui::ColorConvert(globals::visuals::boxcolors);

            if (globals::visuals::mm2esp && !player.tool_name.empty()) {
                std::string itemname = player.tool_name;
                std::transform(itemname.begin(), itemname.end(), itemname.begin(), ::tolower);

                if (itemname.find("gun") != std::string::npos) {
                    player_color = ImColor(0, 0, 255, 255);
                }
                else if (itemname.find("knife") != std::string::npos) {
                    player_color = ImColor(255, 0, 0, 255);
                }
            }

            if (globals::instances::cachedtarget.name == player.name && globals::visuals::lockedesp)
                player_color = ImGui::ColorConvert(globals::visuals::lockedespcolor);

            render_box_enhanced(top_left, bottom_right, player_color);
        }

        if (globals::visuals::name) {
            render_name_enhanced(player.name, top_left, bottom_right);
        }

        if (globals::visuals::distance) {
            render_distance_enhanced(player.distance, top_left, bottom_right, !player.tool_name.empty() && globals::visuals::toolesp);
        }

        if (globals::visuals::toolesp && !player.tool_name.empty()) {
            render_tool_name_enhanced(player.tool_name, top_left, bottom_right);
        }

        if (globals::visuals::snapline) {
            bool is_locked = (globals::instances::cachedtarget.name == player.name && globals::visuals::lockedesp);
            render_snapline_enhanced(screen_check, static_cast<SnaplineType>(globals::visuals::snaplinetype), is_locked);
        }

        if (globals::visuals::healthbar) {
            float current_health = read<float>(player.player.humanoid.address + offsets::Health);
            float max_health = read<float>(player.player.humanoid.address + offsets::MaxHealth);
            render_health_bar_enhanced(top_left, bottom_right, current_health, max_health, ImColor(255, 0, 0));
        }

        if (globals::visuals::skeletons && !globals::visuals::esp_preview_mode) { // Added condition for ESP preview
            render_skeleton_enhanced(player, ImGui::ColorConvert(globals::visuals::skeletonscolor));
        }
    }

    for (auto& player : visuals::espData.cachedPlayers) {
        if (!player.valid) continue;

        math::Vector2 screen_pos = roblox::worldtoscreen(player.position);
        auto dimensions = globals::instances::visualengine.get_dimensions();

        bool is_offscreen = screen_pos.x <= 0 || screen_pos.x >= dimensions.x ||
            screen_pos.y <= 0 || screen_pos.y >= dimensions.y;

        if (is_offscreen && globals::visuals::oofarrows) {
            render_offscreen_arrow_enhanced(player.position, visuals::espData.localPos,
                ImGui::ColorConvert(globals::visuals::oofcolor), player.name);
        }
    }

    visuals::espData.cachedmutex.unlock();

    render_sonar_enhanced();
}
