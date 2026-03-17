#define _HAS_STD_BYTE 0
#include "overlay.h"
#include "../imgui/imgui.h"
#include "../imgui/backends/imgui_impl_win32.h"
#include "../imgui/backends/imgui_impl_dx11.h"
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

// Helper function to create a color picker with hex input
bool ColorPickerWithHex(const char* label, float col[4], ImGuiColorEditFlags flags = 0) {
    bool changed = false;
    
    // Generate unique ID for this color picker
    ImGui::PushID(label);
    
    // Create hex input
    static char hex_color[8] = "#";
    static bool hex_edited = false;
    
    // Update hex when color picker changes
    if (!hex_edited) {
        snprintf(hex_color, sizeof(hex_color), "#%02X%02X%02X", 
                (int)(col[0] * 255), 
                (int)(col[1] * 255), 
                (int)(col[2] * 255));
    }
    
    // Save style vars
    float frame_rounding = ImGui::GetStyle().FrameRounding;
    
    // Push rectangular button style
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, frame_rounding * 0.3f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 3.0f)); // More vertical padding for rectangle
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
    
    // Set dimensions for the color picker to be smaller and rectangular
    const float color_picker_width = 24.0f; // Smaller width for rectangle
    const float color_picker_height = 10.0f; // Smaller height for the color picker
    
    // Create a custom button with the color
    ImVec2 button_size = ImVec2(color_picker_width, color_picker_height);
    ImVec4 col_with_alpha = ImVec4(col[0], col[1], col[2], col[3]);
    
    // Draw the color button
    if (ImGui::ColorButton(label, col_with_alpha, flags, button_size)) {
        // Open color picker popup when clicked
        ImGui::OpenPopup("##color_picker_popup");
    }
    
    // Color picker popup
    if (ImGui::BeginPopup("##color_picker_popup")) {
        ImGui::ColorPicker4("##picker", col, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview);
        ImGui::EndPopup();
        changed = true;
    }
    
    // Restore style
    ImGui::PopStyleVar(3);
    
    ImGui::PopID();
    return changed;
}

#include "../imgui/TextEditor.h"
#include "../star_effect.h" // Include for StarEffect
#include <dwmapi.h>

#include <filesystem>
#include <thread>
#include <bitset>
#include <iostream>
#include <algorithm>
#include <functional>
#include <cmath>
#include <unordered_set>
#include <cstring>
#ifdef min
#undef min
#endif
#include <stack>
#include "../../util/notification/notification.h"
#ifdef max
#undef max
#endif
#include <Psapi.h>
#include "../imgui/backends/imgui_impl_win32.h"
#include "../imgui/backends/imgui_impl_dx11.h"
#include "../imgui/misc/freetype/imgui_freetype.h"
#include "../imgui/addons/imgui_addons.h"
#include <dwmapi.h>
#include <d3dx11.h>
#include <shellapi.h>
#include <urlmon.h>
#include <gdiplus.h>
#include "../../util/globals.h"
#include "keybind/keybind.h"
#include "../../features/visuals/visuals.h"
#include "../../util/config/configsystem.h"
#include "../../util/classes/math/math.h"
#include "../../features/misc/modules/nojumpcooldown.h" // Explicitly include nojumpcooldown.h
#include "../../features/misc/misc.h" // Include misc.h for jumppower function
#include "../../features/misc/modules/autofriend.h" // Include autofriend module

static std::unordered_map<uint64_t, std::string> g_displayname_cache;
static std::mutex g_displayname_cache_mutex;
static std::unordered_set<uint64_t> g_displayname_inflight;
static char player_search[128] = "";
static std::unordered_map<uint64_t, double> g_copy_username_timeout;
static std::unordered_map<uint64_t, double> g_copy_userid_timeout;

// Blocking fetch, only used inside async worker
static std::string fetch_display_name_api(uint64_t userid) {
    if (userid == 0) return {};

    {
        std::lock_guard<std::mutex> lock(g_displayname_cache_mutex);
        auto it = g_displayname_cache.find(userid);
        if (it != g_displayname_cache.end()) return it->second;
    }

    std::wstring host = L"users.roblox.com";
    std::wstring path = L"/v1/users/" + std::to_wstring(userid);

    std::string response;
    HINTERNET hSession = WinHttpOpen(L"frizyOverlay/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return {};

    WinHttpSetTimeouts(hSession, 3000, 3000, 3000, 3000);

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return {};
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return {};
    }

    bool ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(hRequest, NULL);

    if (ok) {
        DWORD dwSize = 0;
        do {
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || dwSize == 0) break;
            std::string buffer(dwSize, 0);
            DWORD dwDownloaded = 0;
            if (WinHttpReadData(hRequest, &buffer[0], dwSize, &dwDownloaded) && dwDownloaded > 0) {
                buffer.resize(dwDownloaded);
                response.append(buffer);
            }
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (response.empty()) return {};

    std::string display;
    const std::string key = "\"displayName\":\"";
    auto pos = response.find(key);
    if (pos != std::string::npos) {
        pos += key.size();
        auto end = response.find('"', pos);
        if (end != std::string::npos && end > pos) {
            display = response.substr(pos, end - pos);
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_displayname_cache_mutex);
        g_displayname_cache[userid] = display;
    }

    return display;
}

// Non-blocking ensure: returns cached value if ready, otherwise kicks off a background fetch and returns empty
static std::string ensure_display_name_async(uint64_t userid) {
    if (userid == 0) return {};

    {
        std::lock_guard<std::mutex> lock(g_displayname_cache_mutex);
        auto it = g_displayname_cache.find(userid);
        if (it != g_displayname_cache.end()) return it->second;

        if (g_displayname_inflight.count(userid)) return {};
        g_displayname_inflight.insert(userid);
    }

    std::thread([userid]() {
        std::string display = fetch_display_name_api(userid);
        std::lock_guard<std::mutex> lock(g_displayname_cache_mutex);
        g_displayname_cache[userid] = display;
        g_displayname_inflight.erase(userid);
    }).detach();

    return {};
}

static std::string get_best_display(roblox::player& player) {
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
    if (username == "NPC") return "NPC";

    std::string display = sanitize(player.displayname);
    if (!is_valid_display(display, username)) {
        std::string pd = sanitize(player.main.get_displayname());
        if (is_valid_display(pd, username)) display = pd;
    }
    if (!is_valid_display(display, username)) {
        std::string hd = sanitize(player.humanoid.get_humdisplayname());
        if (is_valid_display(hd, username)) display = hd;
    }
    if (!is_valid_display(display, username)) {
        std::string api_display = ensure_display_name_async(player.userid.address);
        if (is_valid_display(api_display, username)) display = api_display;
    }
    if (!is_valid_display(display, username)) display = username;
    return display;
}

void render_theme_window() {
    if (!globals::misc::show_theme_window) return;

    // Slightly larger window to accommodate better layout
    ImGui::SetNextWindowSize(ImVec2(450, 500), ImGuiCond_FirstUseEver);
    ImGui::Begin("Theme Customization", &globals::misc::show_theme_window, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    
    // Temporary variables for theme editing
    static float temp_overlay_color[4] = {12.0f / 255.0f, 13.0f / 255.0f, 12.0f / 255.0f, 1.0f};
    static float temp_menuglowcolor[4] = {252.0f / 255.0f, 253.0f / 255.0f, 148.0f / 255.0f, 1.0f};
    static float temp_accent_color[4] = {252.0f / 255.0f, 253.0f / 255.0f, 148.0f / 255.0f, 1.0f};
    static float temp_overlay_star_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    static bool initialized = false;
    
    // Initialize temp variables with current values on first open
    if (!initialized) {
        memcpy(temp_overlay_color, globals::misc::overlay_color, 4 * sizeof(float));
        memcpy(temp_menuglowcolor, globals::misc::menuglowcolor, 4 * sizeof(float));
        memcpy(temp_accent_color, globals::misc::accent_color, 4 * sizeof(float));
        memcpy(temp_overlay_star_color, globals::misc::overlay_star_color, 4 * sizeof(float));
        initialized = true;
    }
    
    // Draw header with accent color
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 content_start = ImGui::GetCursorScreenPos();
    float header_height = 4.0f;
    draw->AddRectFilled(
        content_start,
        ImVec2(content_start.x + ImGui::GetContentRegionAvail().x, content_start.y + header_height),
        ImGui::GetColorU32(ImVec4(temp_accent_color[0], temp_accent_color[1], temp_accent_color[2], 0.8f))
    );
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + header_height + 8);
    
    // Main content area with padding
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    ImGui::BeginChild("ThemeContent", ImVec2(0, ImGui::GetContentRegionAvail().y - 45), true, ImGuiWindowFlags_AlwaysUseWindowPadding);
    
    // Background Colors Section
    if (ImGui::CollapsingHeader("Background Colors", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        
        // Window Background
        ImGui::Text("Window Background");
        ImGui::SameLine(ImGui::GetWindowWidth() * 0.35f);
        ColorPickerWithHex("##window", temp_overlay_color);
        
        // Preview box
        ImGui::Spacing();
        ImGui::Text("Preview:");
        
        // Draw preview background with shadow
        ImVec4 bg_color = ImVec4(temp_overlay_color[0], temp_overlay_color[1], temp_overlay_color[2], temp_overlay_color[3]);
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y), 
            ImVec2(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x - 20, ImGui::GetCursorScreenPos().y + 60),
            ImGui::GetColorU32(bg_color), 
            5.0f
        );
        
        // Add border
        draw->AddRect(
            ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y), 
            ImVec2(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x - 20, ImGui::GetCursorScreenPos().y + 60),
            ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 0.5f)), 
            5.0f, 0, 1.5f
        );
        
        // Add sample content
        ImVec2 text_pos = ImVec2(ImGui::GetCursorScreenPos().x + 15, ImGui::GetCursorScreenPos().y + 10);
        draw->AddText(text_pos, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)), "Window Title");
        
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 60 + 10);
        
        // Menu Glow
        ImGui::Text("Menu Glow");
        ImGui::SameLine(ImGui::GetWindowWidth() * 0.35f);
        ColorPickerWithHex("##menuglow", temp_menuglowcolor);
        
        // Preview glow effect
        ImGui::InvisibleButton("##glow_preview", ImVec2(ImGui::GetContentRegionAvail().x, 4));
        ImVec2 glow_start = ImGui::GetItemRectMin();
        ImVec2 glow_end = ImVec2(glow_start.x + ImGui::GetItemRectSize().x, glow_start.y + 4);
        
        // Draw gradient glow
        ImU32 glow_color = ImGui::GetColorU32(ImVec4(
            temp_menuglowcolor[0], 
            temp_menuglowcolor[1], 
            temp_menuglowcolor[2], 
            temp_menuglowcolor[3] * 0.8f
        ));
        draw->AddRectFilledMultiColor(
            glow_start, glow_end,
            glow_color, glow_color,
            IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0)
        );

        // Star color (overlay stars toggle remains in Settings tab)
        ImGui::Spacing();
        ImGui::Text("Star Color");
        ImGui::SameLine(ImGui::GetWindowWidth() * 0.35f);
        ColorPickerWithHex("##overlay_star_color", temp_overlay_star_color);

        ImGui::Spacing();
    }
    
    // Accent Colors Section
    if (ImGui::CollapsingHeader("Accent Colors", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        
        // Primary Accent
        ImGui::Text("Primary Accent");
        ImGui::SameLine(ImGui::GetWindowWidth() * 0.35f);
        ColorPickerWithHex("##accent", temp_accent_color);
        
        // Preview accent usage
        ImGui::Spacing();
        ImGui::Text("Accent Preview:");
        
        // Button style preview
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(temp_accent_color[0], temp_accent_color[1], temp_accent_color[2], 0.3f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(temp_accent_color[0], temp_accent_color[1], temp_accent_color[2], 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(temp_accent_color[0], temp_accent_color[1], temp_accent_color[2], 0.7f));
        ImGui::Button("Button", ImVec2(120, 30));
        ImGui::PopStyleColor(3);
        
        // Text preview
        ImGui::Text("Regular text ");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(temp_accent_color[0], temp_accent_color[1], temp_accent_color[2], 1.0f), "Accent text");
        
        ImGui::Spacing();
    }
    
    ImGui::EndChild();
    ImGui::PopStyleVar();
    
    // Bottom buttons
    ImGui::Spacing();
    float button_height = 30.0f;
    float button_spacing = 10.0f;
    float button_width = (ImGui::GetContentRegionAvail().x - button_spacing) / 2.0f;
    
    if (ImGui::Button("Reset to Default", ImVec2(button_width, button_height))) {
        // Reset to default colors
        temp_overlay_color[0] = 12.0f / 255.0f; temp_overlay_color[1] = 13.0f / 255.0f; temp_overlay_color[2] = 12.0f / 255.0f; temp_overlay_color[3] = 1.0f;
        temp_menuglowcolor[0] = 252.0f / 255.0f; temp_menuglowcolor[1] = 253.0f / 255.0f; temp_menuglowcolor[2] = 148.0f / 255.0f; temp_menuglowcolor[3] = 1.0f;
        temp_accent_color[0] = 252.0f / 255.0f; temp_accent_color[1] = 253.0f / 255.0f; temp_accent_color[2] = 148.0f / 255.0f; temp_accent_color[3] = 1.0f;
        temp_overlay_star_color[0] = 1.0f; temp_overlay_star_color[1] = 1.0f; temp_overlay_star_color[2] = 1.0f; temp_overlay_star_color[3] = 1.0f;
        globals::misc::accent_color[1] = temp_accent_color[1];
        globals::misc::accent_color[2] = temp_accent_color[2];
        globals::misc::accent_color[3] = temp_accent_color[3];
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Apply Theme", ImVec2(ImGui::GetContentRegionAvail().x, 25))) {
        // Apply the temporary colors to the actual theme
        globals::misc::overlay_color[0] = temp_overlay_color[0];
        globals::misc::overlay_color[1] = temp_overlay_color[1];
        globals::misc::overlay_color[2] = temp_overlay_color[2];
        globals::misc::overlay_color[3] = temp_overlay_color[3];
        globals::misc::menuglowcolor[0] = temp_menuglowcolor[0];
        globals::misc::menuglowcolor[1] = temp_menuglowcolor[1];
        globals::misc::menuglowcolor[2] = temp_menuglowcolor[2];
        globals::misc::menuglowcolor[3] = temp_menuglowcolor[3];
        globals::misc::accent_color[0] = temp_accent_color[0];
        globals::misc::accent_color[1] = temp_accent_color[1];
        globals::misc::accent_color[2] = temp_accent_color[2];
        globals::misc::accent_color[3] = temp_accent_color[3];
        globals::misc::overlay_star_color[0] = temp_overlay_star_color[0];
        globals::misc::overlay_star_color[1] = temp_overlay_star_color[1];
        globals::misc::overlay_star_color[2] = temp_overlay_star_color[2];
        globals::misc::overlay_star_color[3] = temp_overlay_star_color[3];
    }

    ImGui::End();
}

void apply_theme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(globals::misc::overlay_color[0], globals::misc::overlay_color[1], globals::misc::overlay_color[2], 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(globals::misc::overlay_color[0], globals::misc::overlay_color[1], globals::misc::overlay_color[2], 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.94f);
    colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.20f, 0.50f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(19 / 255.0f, 18 / 255.0f, 18 / 255.0f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(25 / 255.0f, 24 / 255.0f, 24 / 255.0f, 1.00f); // Slightly lighter for hover
    colors[ImGuiCol_FrameBgActive] = ImVec4(30 / 255.0f, 29 / 255.0f, 29 / 255.0f, 1.00f); // Even lighter for active
    colors[ImGuiCol_TitleBg] = ImVec4(19 / 255.0f, 18 / 255.0f, 18 / 255.0f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(25 / 255.0f, 24 / 255.0f, 24 / 255.0f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(19 / 255.0f, 18 / 255.0f, 18 / 255.0f, 0.51f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(globals::misc::accent_color[0], globals::misc::accent_color[1], globals::misc::accent_color[2], 1.00f);
    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f); // Set text color to white
    colors[ImGuiCol_SliderGrabActive] = ImVec4(globals::misc::accent_color[0] * 1.2f, globals::misc::accent_color[1] * 1.2f, globals::misc::accent_color[2] * 1.2f, 1.00f); // Slightly lighter for active
    colors[ImGuiCol_Button] = ImVec4(19 / 255.0f, 18 / 255.0f, 18 / 255.0f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(25 / 255.0f, 24 / 255.0f, 24 / 255.0f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(30 / 255.0f, 29 / 255.0f, 29 / 255.0f, 0.45f); // softer active opacity
    colors[ImGuiCol_Header] = ImVec4(19 / 255.0f, 18 / 255.0f, 18 / 255.0f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(25 / 255.0f, 24 / 255.0f, 24 / 255.0f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(30 / 255.0f, 29 / 255.0f, 29 / 255.0f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(globals::misc::accent_color[0], globals::misc::accent_color[1], globals::misc::accent_color[2], 0.78f); // Use new color for hovered separator
    colors[ImGuiCol_SeparatorActive] = ImVec4(globals::misc::accent_color[0], globals::misc::accent_color[1], globals::misc::accent_color[2], 1.00f); // Use new color for active separator
    colors[ImGuiCol_ResizeGrip] = ImVec4(globals::misc::accent_color[0], globals::misc::accent_color[1], globals::misc::accent_color[2], 0.20f); // Use new color for resize grip
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(globals::misc::accent_color[0], globals::misc::accent_color[1], globals::misc::accent_color[2], 0.67f); // Use new color for resize grip hovered
    colors[ImGuiCol_ResizeGripActive] = ImVec4(globals::misc::accent_color[0], globals::misc::accent_color[1], globals::misc::accent_color[2], 0.95f); // Use new color for resize grip active
    colors[ImGuiCol_TabHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_TabSelectedOverline] = ImVec4(globals::misc::accent_color[0], globals::misc::accent_color[1], globals::misc::accent_color[2], 1.00f); // Use new color for selected tab overline
    colors[ImGuiCol_TabDimmed] = ImVec4(0.10f, 0.10f, 0.10f, 0.97f);
    colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(globals::misc::accent_color[0], globals::misc::accent_color[1], globals::misc::accent_color[2], 1.00f); // Use new color for dimmed selected tab overline
    colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextLink] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 0.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    colors[ImGuiCol_WindowShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
}

using namespace math;
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static StarEffect g_starEffect; // Global instance of StarEffect
static IDXGISwapChain* g_pSwapChain = nullptr;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// Tray icon helpers/state
static const UINT WM_APP_TRAY_ICON = WM_APP + 1;
static const UINT ID_TRAY_TOGGLE = 1001;
static const UINT ID_TRAY_RESTART = 1002;
static const UINT ID_TRAY_EXIT = 1003;
static const wchar_t* kTrayIconUrl = L"https://cdn.discordapp.com/attachments/1201119192288604280/1441723993848680478/0cgIOQAAAAGSURBVAMA9mXpXy8bLx4AAAAASUVORK5CYII.png?ex=6922d59a&is=6921841a&hm=8d41e8072058568c2e75ae8df72c2e2a7ce8928b22ac94eab933519feeccc3bd&";
static NOTIFYICONDATAW g_trayIconData{};
static HMENU g_trayMenu = nullptr;
static HICON g_trayIcon = nullptr;
static ULONG_PTR g_gdiplusToken = 0;
static HANDLE g_singleInstanceMutex = nullptr;


#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "gdiplus.lib")


bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();

static void InitGdiplusOnce()
{
    if (g_gdiplusToken == 0)
    {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr);
    }
}

static void ShutdownGdiplus()
{
    if (g_gdiplusToken != 0)
    {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

static HICON LoadTrayIconFromUrl()
{
    InitGdiplusOnce();

    wchar_t temp_path[MAX_PATH]{};
    wchar_t temp_file[MAX_PATH]{};

    if (!GetTempPathW(MAX_PATH, temp_path))
    {
        return nullptr;
    }

    if (!GetTempFileNameW(temp_path, L"lio", 0, temp_file))
    {
        return nullptr;
    }

    HICON icon = nullptr;
    if (URLDownloadToFileW(nullptr, kTrayIconUrl, temp_file, 0, nullptr) == S_OK)
    {
        Gdiplus::Bitmap bitmap(temp_file);
        if (bitmap.GetLastStatus() == Gdiplus::Ok)
        {
            bitmap.GetHICON(&icon);
        }
    }

    DeleteFileW(temp_file);
    return icon;
}

static void UpdateTrayMenuText()
{
    if (g_trayMenu)
    {
        ModifyMenuW(g_trayMenu, ID_TRAY_TOGGLE, MF_BYCOMMAND | MF_STRING, ID_TRAY_TOGGLE, overlay::visible ? L"Close Menu" : L"Open Menu");
    }
}

static void EnsureTrayMenuCreated()
{
    if (!g_trayMenu)
    {
        g_trayMenu = CreatePopupMenu();
        AppendMenuW(g_trayMenu, MF_STRING, ID_TRAY_TOGGLE, L"Open Menu");
        AppendMenuW(g_trayMenu, MF_STRING, ID_TRAY_RESTART, L"Restart");
        AppendMenuW(g_trayMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");
    }
    UpdateTrayMenuText();
}

static void AddTrayIcon(HWND hwnd)
{
    if (!g_trayIcon)
    {
        g_trayIcon = LoadTrayIconFromUrl();
    }

    ZeroMemory(&g_trayIconData, sizeof(g_trayIconData));
    g_trayIconData.cbSize = sizeof(g_trayIconData);
    g_trayIconData.hWnd = hwnd;
    g_trayIconData.uID = 1;
    g_trayIconData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_trayIconData.uCallbackMessage = WM_APP_TRAY_ICON;
    g_trayIconData.hIcon = g_trayIcon ? g_trayIcon : LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(g_trayIconData.szTip, L"frizy Overlay | V 1.10");

    Shell_NotifyIconW(NIM_ADD, &g_trayIconData);
    EnsureTrayMenuCreated();
}

static void RemoveTrayIcon()
{
    if (g_trayIconData.hWnd != nullptr)
    {
        Shell_NotifyIconW(NIM_DELETE, &g_trayIconData);
        g_trayIconData = {};
    }

    if (g_trayMenu)
    {
        DestroyMenu(g_trayMenu);
        g_trayMenu = nullptr;
    }

    if (g_trayIcon)
    {
        DestroyIcon(g_trayIcon);
        g_trayIcon = nullptr;
    }

    ShutdownGdiplus();
}

static void SetOverlayVisibility(bool should_be_visible)
{
    if (overlay::visible == should_be_visible)
    {
        return;
    }

    overlay::visible = should_be_visible;
    if (overlay::visible) {
        overlay::g_selected_player_address = 0; // Reset selected player when overlay becomes visible
        overlay::g_playerlist_just_opened = true; // Set flag when overlay becomes visible
        ImGui::GetIO().MouseClicked[0] = false; // Clear left mouse click state
        ImGui::GetIO().MouseClicked[1] = false; // Clear right mouse click state
        overlay::g_ignore_mouse_frames = 2; // Ignore mouse input for 2 frames
        if (globals::misc::custom_cursor) {
            ShowCursor(FALSE); // Hide the system cursor when using custom cursor
        }
        else {
            ShowCursor(TRUE);  // Ensure system cursor visible when custom cursor disabled
        }
    }
    else {
        ShowCursor(TRUE); // Show the system cursor
    }

    UpdateTrayMenuText();
}

static void ToggleOverlayVisibility()
{
    SetOverlayVisibility(!overlay::visible);
}

static void ReleaseSingleInstanceMutex()
{
    if (g_singleInstanceMutex)
    {
        ReleaseMutex(g_singleInstanceMutex);
        CloseHandle(g_singleInstanceMutex);
        g_singleInstanceMutex = nullptr;
    }
}

static void RestartApplication()
{
    wchar_t current_path[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, current_path, MAX_PATH))
    {
        ShellExecuteW(nullptr, L"open", current_path, nullptr, nullptr, SW_SHOWNORMAL);
    }
    PostQuitMessage(0);
}

static void ShowTrayMenu(HWND hwnd)
{
    EnsureTrayMenuCreated();
    UpdateTrayMenuText();

    POINT cursor_pos{};
    GetCursorPos(&cursor_pos);
    SetForegroundWindow(hwnd);

    int command = TrackPopupMenu(g_trayMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD, cursor_pos.x, cursor_pos.y, 0, hwnd, nullptr);
    if (command != 0)
    {
        PostMessage(hwnd, WM_COMMAND, static_cast<WPARAM>(command), 0);
    }
}

bool fullsc(HWND windowHandle);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void movewindow(HWND hw)
{
    if (FindWindowA(0, "Roblox") && (GetForegroundWindow() == FindWindowA(0, "Roblox") || GetForegroundWindow() == hw)) {
        if (overlay::visible) {
            static bool isDragging = false;
            static POINT lastCursorPos;

            if (GetAsyncKeyState(VK_LBUTTON) < 0) {
                POINT cursorPos;
                GetCursorPos(&cursorPos);

                if (!isDragging) {
                    RECT rect;
                    GetWindowRect(hw, &rect);
                    // Check if the mouse is over the title bar area (e.g., top 30 pixels)
                    if (cursorPos.y - rect.top < 30 && cursorPos.y > rect.top && cursorPos.x > rect.left && cursorPos.x < rect.right) {
                        isDragging = true;
                        lastCursorPos = cursorPos;
                    }
                }

                if (isDragging) {
                    int xOffset = cursorPos.x - lastCursorPos.x;
                    int yOffset = cursorPos.y - lastCursorPos.y;

                    RECT rect;
                    GetWindowRect(hw, &rect);

                    // Move the native window
                    SetWindowPos(hw, NULL, rect.left + xOffset, rect.top + yOffset, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
                    lastCursorPos = cursorPos;
                }
            }
            else {
                isDragging = false;
            }
        }
    }
}


static ConfigSystem g_config_system;

void render_explorer() {
    if (!globals::misc::explorer) return;

    static roblox::instance selectedPart;
    static std::unordered_map<uint64_t, std::vector<roblox::instance>> nodeCache;
    static std::unordered_map<uint64_t, bool> nodeExpandedCache;
    static std::unordered_map<uint64_t, std::string> nodeNameCache;
    static std::unordered_map<uint64_t, std::string> nodeClassNameCache;
    static std::unordered_map<uint64_t, std::string> nodeTeamCache;
    static std::unordered_map<uint64_t, std::string> nodeUniqueIdCache;
    static std::unordered_map<uint64_t, std::string> nodeModel;
    static char searchQuery[128] = "";
    static std::vector<roblox::instance> searchResults;
    static bool showSearchResults = false;
    static bool isCacheInitialized = false;
    static auto lastCacheRefresh = std::chrono::steady_clock::now();
    static std::unordered_map<uint64_t, std::string> nodePath;
    static bool showProperties = true;
    static int selectedTab = 0;
    static float splitRatio = 0.65f;
    static ImVec2 explorer_pos = ImVec2(GetSystemMetrics(SM_CXSCREEN) - 680, 80);
    static bool first_time = true;

    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = g.Style;
    float rounded = style.WindowRounding;
    style.WindowRounding = 0;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize;

    if (!overlay::visible) {
        window_flags |= ImGuiWindowFlags_NoInputs;
    }

    if (first_time) {
        ImGui::SetNextWindowPos(explorer_pos, ImGuiCond_FirstUseEver);
        first_time = false;
    }

    static std::unordered_map<std::string, std::string> classPrefixes = {
        {"Workspace", "[WS] "},
        {"Players", "[P] "},
        {"Folder", "[F] "},
        {"Part", "[PT] "},
        {"BasePart", "[BP] "},
        {"Script", "[S] "},
        {"LocalScript", "[LS] "},
        {"ModuleScript", "[MS] "}
    };

    auto cacheNode = [&](roblox::instance& node) {
        if (nodeCache.find(node.address) == nodeCache.end()) {
            nodeCache[node.address] = node.get_children();
            nodeNameCache[node.address] = node.get_name();
            nodeClassNameCache[node.address] = node.get_class_name();
            nodeUniqueIdCache[node.address] = std::to_string(node.address); // Store raw address as string

            std::string current_path_segment = node.get_name();
            std::string full_path_code = "findfirstchild(\"" + current_path_segment + "\")";

            roblox::instance parent = node.read_parent();
            while (parent.address != 0) {
                if (nodeCache.find(parent.address) == nodeCache.end()) {
                    nodeCache[parent.address] = parent.get_children();
                    nodeNameCache[parent.address] = parent.get_name();
                }
                std::string parentName = nodeNameCache[parent.address];
                if (!parentName.empty()) {
                    full_path_code = "findfirstchild(\"" + parentName + "\")." + full_path_code;
                }
                parent = parent.read_parent();
            }
            nodePath[node.address] = "globals::datamodel." + full_path_code;
        }
        };

    try {
        auto& datamodel = globals::instances::datamodel;
        roblox::instance root_instance;
		root_instance.address = datamodel.address;

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCacheRefresh).count() >= 2) {
            nodeCache.clear();
            nodeNameCache.clear();
            nodeClassNameCache.clear();
            nodeUniqueIdCache.clear();
            isCacheInitialized = false;
            lastCacheRefresh = now;
        }

        if (!isCacheInitialized) {
            cacheNode(root_instance);
            isCacheInitialized = true;
        }

        float content_width = 650.0f;
        float padding = 8.0f;
        float header_height = 30.0f;
        float total_height = 700.0f;

        ImGui::SetNextWindowSize(ImVec2(content_width + (padding * 2), total_height), ImGuiCond_Always);
        ImGui::Begin("Explorer", nullptr, window_flags);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 window_pos = ImGui::GetWindowPos();
        ImVec2 window_size = ImGui::GetWindowSize();

        // Allow dragging the explorer via the header area
        ImGui::SetCursorScreenPos(window_pos);
        ImGui::InvisibleButton("##ExplorerDragRegion", ImVec2(window_size.x, header_height));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            ImGui::SetWindowPos(ImVec2(window_pos.x + delta.x, window_pos.y + delta.y));
        }

        explorer_pos = window_pos;

        ImU32 text_color = IM_COL32(255, 255, 255, 255);
        ImU32 top_line_color = ImGui::GetColorU32(ImGuiCol_SliderGrab);

        draw->AddRectFilled(window_pos, ImVec2(window_pos.x + window_size.x, window_pos.y + 2), top_line_color, 0.0f);
        draw->AddText(ImVec2(window_pos.x + padding, window_pos.y + 8), text_color, "Explorer");

        ImGui::SetCursorPos(ImVec2(padding, header_height));

        ImGui::PushItemWidth(content_width - 100);
        bool searchChanged = ImGui::InputTextWithHint("##Search", "Search...", searchQuery, IM_ARRAYSIZE(searchQuery), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();

        if (searchChanged) {
            searchResults.clear();
            showSearchResults = strlen(searchQuery) > 0;

            if (showSearchResults && strlen(searchQuery) >= 1) {
                std::string query = searchQuery;
                std::transform(query.begin(), query.end(), query.begin(), ::tolower);

                std::function<void(roblox::instance&)> searchInstance = [&](roblox::instance& instance) {
                    if (searchResults.size() >= 100) return;

                    cacheNode(instance);

                    std::string name = nodeNameCache[instance.address];
                    std::transform(name.begin(), name.end(), name.begin(), ::tolower);

                    if (name.find(query) != std::string::npos) {
                        searchResults.push_back(instance);
                    }

                    for (auto& child : instance.get_children()) {
                        searchInstance(child);
                    }
                    };

                if (auto workspace = root_instance.findfirstchild("Workspace"); workspace.address != 0) {
                    searchInstance(workspace);
                }

                if (globals::instances::players.address != 0) {
                    for (auto& player : globals::instances::players.get_children()) {
                        searchInstance(player);
                    }
                }

                if (auto teams = root_instance.findfirstchild("Teams"); teams.address != 0) {
                    searchInstance(teams);
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Refresh", ImVec2(70, 0))) {
            nodeCache.clear();
            nodeNameCache.clear();
            nodeClassNameCache.clear();
            nodeUniqueIdCache.clear();
            isCacheInitialized = false;
            searchResults.clear();
            showSearchResults = false;
            memset(searchQuery, 0, sizeof(searchQuery));
        }

        float treeHeight = 400.0f;
        ImGui::BeginChild("ExplorerTree", ImVec2(content_width, treeHeight), true);

        if (showSearchResults && strlen(searchQuery) > 0) {
            ImGui::Text("Search Results (%d):", static_cast<int>(searchResults.size()));
            ImGui::Separator();

            if (!searchResults.empty()) {
                for (auto& node : searchResults) {
                    if (!node.address) continue;

                    if (nodeCache.find(node.address) == nodeCache.end()) {
                        cacheNode(node);
                    }

                    ImGui::PushID(nodeUniqueIdCache[node.address].c_str());

                    std::string displayText = nodeNameCache[node.address];
                    std::string className = nodeClassNameCache[node.address];
                    std::string fullText = displayText + " [" + className + "]";

                    bool is_selected = (selectedPart.address == node.address);

                    if (ImGui::Selectable(fullText.c_str(), is_selected)) {
                        selectedPart = node;
                    }

                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                        ImGui::OpenPopup("NodeContextMenu");
                        selectedPart = node;
                    }

                    if (ImGui::BeginPopup("NodeContextMenu")) {
                        ImGui::Text("Options for: %s", displayText.c_str());
                        ImGui::Separator();

                        ImGui::PushID("CopyLocation");
                        if (ImGui::MenuItem("Copy Location")) {
                            ImGui::SetClipboardText(nodePath[node.address].c_str());
                        }
                        ImGui::PopID();

                        ImGui::PushID("SetCollisionTrue");
                        if (ImGui::MenuItem("Set Collision True")) {
                            selectedPart.write_cancollide(true);
                        }
                        ImGui::PopID();

                        ImGui::PushID("SetCollisionFalse");
                        if (ImGui::MenuItem("Set Collision False")) {
                            selectedPart.write_cancollide(false);
                        }
                        ImGui::PopID();

                        ImGui::PushID("TeleportToPart");
                        if (ImGui::MenuItem("Teleport To Part")) {
                            Vector3 partPosition = node.get_pos();
                            const float verticalOffset = 5.0f;
                            partPosition.y += verticalOffset;
                            globals::instances::lp.hrp.write_position(partPosition);
                        }
                        ImGui::PopID();

                        if (overlay::g_spectated_address == node.address) {
                            ImGui::PushID("StopSpectating");
                            if (ImGui::MenuItem("Stop Spectating")) {
                                globals::instances::localplayer.unspectate(); // Corrected to use localplayer instance
                                overlay::g_spectated_address = 0;
                                overlay::g_spectated_player_userid = 0;
                            }
                            ImGui::PopID();
                        }
                        else {
                            ImGui::PushID("SpectatePart");
                            if (ImGui::MenuItem("Spectate Part")) {
                                selectedPart = node;
                                globals::instances::localplayer.spectate(node.address); // Corrected to use localplayer instance
                                overlay::g_spectated_address = node.address;
                                overlay::g_spectated_player_userid = 0;
                            }
                            ImGui::PopID();
                        }

                        ImGui::EndPopup();
                    }

                    ImGui::PopID();
                }
            }
            else {
                ImGui::Text("No results found");
            }
        }
        else {
            for (auto& child : nodeCache[root_instance.address]) {
                std::stack<std::pair<roblox::instance, int>> stack;
                stack.push({ child, 0 });

                while (!stack.empty()) {
                    auto [node, indentLevel] = stack.top();
                    stack.pop();

                    cacheNode(node);

                    ImGui::SetCursorPosX(20.0f * indentLevel);
                    ImGui::PushID(nodeUniqueIdCache[node.address].c_str());

                    const std::vector<roblox::instance>& children = nodeCache[node.address];
                    bool hasChildren = !children.empty();

                    std::string className = nodeClassNameCache[node.address];
                    std::string prefix = "";

                    if (classPrefixes.find(className) != classPrefixes.end()) {
                        prefix = classPrefixes[className];
                    }

                    std::string displayText = prefix + nodeNameCache[node.address] + " [" + className + "]";

                    ImGuiTreeNodeFlags flags = hasChildren ? 0 : ImGuiTreeNodeFlags_Leaf;
                    flags |= ImGuiTreeNodeFlags_OpenOnArrow;

                    if (selectedPart.address == node.address) {
                        flags |= ImGuiTreeNodeFlags_Selected;
                    }

                    bool isExpanded = ImGui::TreeNodeEx(displayText.c_str(), flags);

                    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                        selectedPart = node;
                    }

                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                        ImGui::OpenPopup("NodeContextMenu");
                        selectedPart = node;
                    }

                    if (ImGui::BeginPopup("NodeContextMenu")) {
                        ImGui::Text("%s", nodeNameCache[node.address].c_str());
                        ImGui::Separator();

                        ImGui::PushID("CopyLocation");
                        if (ImGui::MenuItem("Copy Location")) {
                            ImGui::SetClipboardText(nodePath[node.address].c_str());
                        }
                        ImGui::PopID();

                        ImGui::PushID("SetCollisionTrue");
                        if (ImGui::MenuItem("Set Collision True")) {
                            selectedPart.write_cancollide(true);
                        }
                        ImGui::PopID();

                        ImGui::PushID("SetCollisionFalse");
                        if (ImGui::MenuItem("Set Collision False")) {
                            selectedPart.write_cancollide(false);
                        }
                        ImGui::PopID();

                        ImGui::PushID("TeleportToPart");
                        if (ImGui::MenuItem("Teleport To Part")) {
                            Vector3 partPosition = node.get_pos();
                            const float verticalOffset = 5.0f;
                            partPosition.y += verticalOffset;
                            globals::instances::lp.hrp.write_position(partPosition);
                        }
                        ImGui::PopID();

                        if (overlay::g_spectated_address == node.address) {
                            ImGui::PushID("StopSpectating");
                            if (ImGui::MenuItem("Stop Spectating")) {
                                globals::instances::localplayer.unspectate(); // Corrected to use localplayer instance
                                overlay::g_spectated_address = 0;
                            }
                            ImGui::PopID();
                        }
                        else {
                            ImGui::PushID("SpectatePart");
                            if (ImGui::MenuItem("Spectate Part")) {
                                selectedPart = node;
                                globals::instances::localplayer.spectate(node.address); // Corrected to use localplayer instance
                                overlay::g_spectated_address = node.address;
                            }
                            ImGui::PopID();
                        }

                        ImGui::EndPopup();
                    }

                    if (isExpanded) {
                        for (auto it = children.rbegin(); it != children.rend(); ++it) {
                            stack.push({ *it, indentLevel + 1 });
                        }
                        ImGui::TreePop();
                    }

                    ImGui::PopID();
                }
            }
        }

        ImGui::EndChild();

        ImGui::BeginChild("Properties", ImVec2(content_width, 240), true);

        ImGui::Text("Properties");
        ImGui::Separator();

        if (selectedPart.address != 0) {
            if (ImGui::BeginTabBar("PropertiesTabBar")) {
                if (ImGui::BeginTabItem("Basic")) {
                    ImGui::BeginChild("BasicScrollRegion", ImVec2(0, 160), false);

                    const std::string& nodeName = nodeNameCache[selectedPart.address];
                    const std::string& className = nodeClassNameCache[selectedPart.address];
                    roblox::instance parent = selectedPart.read_parent();
                    std::string parentName = nodeNameCache[parent.address];

                    float col1Width = 120.0f;

                    ImGui::Text("Path:");
                    ImGui::SameLine(col1Width);
                    ImGui::TextWrapped("%s", nodePath[selectedPart.address].c_str());

                    ImGui::Text("Name:");
                    ImGui::SameLine(col1Width);
                    ImGui::Text("%s", nodeName.c_str());

                    ImGui::Text("Class:");
                    ImGui::SameLine(col1Width);
                    ImGui::Text("%s", className.c_str());

                    ImGui::Text("Parent:");
                    ImGui::SameLine(col1Width);
                    ImGui::Text("%s", parentName.c_str());

                    ImGui::Text("Address:");
                    ImGui::SameLine(col1Width);
                    ImGui::Text("0x%llX", selectedPart.address);

                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Transform")) {
                    ImGui::BeginChild("TransformScrollRegion", ImVec2(0, 160), false);

                    float col1Width = 120.0f;

                    Vector3 position = selectedPart.get_pos();
                    ImGui::Text("Position:");
                    ImGui::SameLine(col1Width);
                    ImGui::Text("X: %.2f  Y: %.2f  Z: %.2f", position.x, position.y, position.z);

                    Vector3 size = selectedPart.get_part_size();
                    ImGui::Text("Size:");
                    ImGui::SameLine(col1Width);
                    ImGui::Text("X: %.2f  Y: %.2f  Z: %.2f", size.x, size.y, size.z);

                    ImGui::Separator();
                    ImGui::Text("Quick Actions:");

                    if (ImGui::Button("Teleport To", ImVec2(120, 25))) {
                        Vector3 partPosition = selectedPart.get_pos();
                        const float verticalOffset = 5.0f;
                        partPosition.y += verticalOffset;
                        globals::instances::lp.hrp.write_position(partPosition);
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Toggle Collide", ImVec2(120, 25))) {
                        bool currentState = selectedPart.get_cancollide();
                        selectedPart.write_cancollide(!currentState);
                    }

                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        else {
            ImGui::Text("No object selected");
        }

        ImGui::EndChild();

        ImGui::Separator();

        int totalNodes = nodeCache.size();
        int spectatingCount = (overlay::g_spectated_address != 0) ? 1 : 0;

        ImGui::Text("Nodes: %d | Spectating: %d", totalNodes, spectatingCount);

        ImGui::End();
        style.WindowRounding = rounded;
    }
    catch (const std::exception& e) {
        ImGui::Text("Error: %s", e.what());
    }
    catch (...) {
        ImGui::Text("Unknown error occurred");
    }
}


void overlay::initialize_avatar_system() {
    if (g_pd3dDevice && !avatar_manager) {
        avatar_manager = std::make_unique<AvatarManager>(g_pd3dDevice, g_pd3dDeviceContext);

    }
}

void overlay::update_avatars() {
    if (avatar_manager) {
        avatar_manager->update();
    }
}

AvatarManager* overlay::get_avatar_manager() {
    return avatar_manager.get();
}

void overlay::cleanup_avatar_system() {
    if (avatar_manager) {
               avatar_manager.reset();
    }
}

// Keep spectate target synced across respawns without relying on the menu render pass
static void auto_respectate_follow_target(std::vector<roblox::player>& players) {
    if (overlay::g_spectated_player_userid == 0) {
        return;
    }

    for (auto& player : players) {
        if (player.userid.address != overlay::g_spectated_player_userid) {
            continue;
        }

        if (player.hrp.is_valid()) {
            float current_health = player.humanoid.read_health();
            if (current_health > 0 && player.hrp.address != overlay::g_spectated_address) {
                globals::instances::localplayer.spectate(player.hrp.address);
                overlay::g_spectated_address = player.hrp.address;
            }
        }
        break;
    }
}



void render_player_list(ImVec2 main_pos, ImVec2 main_size) {
    if (!globals::misc::playerlist) return;

    if (overlay::g_ignore_mouse_frames > 0) {
        overlay::g_ignore_mouse_frames--;
        return;
    }

    if (!overlay::get_avatar_manager()) {
        overlay::initialize_avatar_system();
    }

    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = g.Style;
    float rounded = style.WindowRounding;
    style.WindowRounding = 0;
    static std::vector<std::string> status_options = { "Enemy", "Friendly", "Client" }; // Removed "Neutral"

    // Initialize player_status map if not already done or if players change
    // This will be handled dynamically based on globals::bools::player_status
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize;

    if (!overlay::visible) {
        window_flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
    }

    ImVec2 playerlist_pos = ImVec2(main_pos.x + main_size.x, main_pos.y);
    ImGui::SetNextWindowPos(playerlist_pos, ImGuiCond_Always);

    std::vector<roblox::player> all_players;
    if (globals::instances::cachedplayers.size() > 0) {
        all_players = globals::instances::cachedplayers;
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
                    all_players.push_back(bot_player);
                }
            }
        }
    }

    std::vector<roblox::player> sorted_players;
    roblox::player local_player_data;
    bool local_player_found = false;

    // Separate local player and add others to a temporary list for sorting
    for (const auto& player : all_players) {
        if (player.name == globals::instances::lp.name) {
            local_player_data = player;
            local_player_found = true;
        } else {
            sorted_players.push_back(player);
        }
    }

    // Sort the remaining players: letters first (alphabetical), then numbers (numerical, then alphabetical)
    std::sort(sorted_players.begin(), sorted_players.end(), [](const roblox::player& a, const roblox::player& b) {
        std::string name_a_lower = a.name;
        std::string name_b_lower = b.name;
        std::transform(name_a_lower.begin(), name_a_lower.end(), name_a_lower.begin(), ::tolower);
        std::transform(name_b_lower.begin(), name_b_lower.end(), name_b_lower.begin(), ::tolower);

        bool is_digit_a = !a.name.empty() && std::isdigit(a.name[0]);
        bool is_digit_b = !b.name.empty() && std::isdigit(b.name[0]);

        if (is_digit_a && !is_digit_b) {
            return false; // b (letter) comes before a (number)
        }
        if (!is_digit_a && is_digit_b) {
            return true; // a (letter) comes before b (number)
        }

        // If both start with letters or both start with numbers
        if (!is_digit_a && !is_digit_b) {
            // Both start with letters, sort alphabetically
            return name_a_lower < name_b_lower;
        } else {
            // Both start with numbers, sort numerically then alphabetically
            int num_a = 0;
            int num_b = 0;

            size_t i_a = 0;
            while (i_a < a.name.length() && std::isdigit(a.name[i_a])) {
                i_a++;
            }
            if (i_a > 0) {
                num_a = std::stoi(a.name.substr(0, i_a));
            }

            size_t i_b = 0;
            while (i_b < b.name.length() && std::isdigit(b.name[i_b])) {
                i_b++;
            }
            if (i_b > 0) {
                num_b = std::stoi(b.name.substr(0, i_b));
            }

            if (num_a != num_b) {
                return num_a < num_b;
            } else {
                // If numbers are equal, sort by the rest of the name alphabetically
                return name_a_lower < name_b_lower;
            }
        }
    });

    // If local player was found, add them to the beginning of the list
    if (local_player_found) {
        sorted_players.insert(sorted_players.begin(), local_player_data);
    }

    // Use the newly sorted list for rendering
    std::vector<roblox::player>& players = sorted_players;
    auto_respectate_follow_target(players);

    // Helper to get player status (true for friendly, false for enemy)
    auto get_player_status = [&](const std::string& name_or_displayname) {
        auto it = globals::bools::player_status.find(name_or_displayname);
        if (it != globals::bools::player_status.end()) {
            return it->second; // true for friendly, false for enemy
        }
        return false; // Default to enemy if not found
    };

    // Helper to set player status
    auto set_player_status = [&](const std::string& name_or_displayname, bool is_friendly) {
        globals::bools::player_status[name_or_displayname] = is_friendly;
    };

    // Helper to remove player status
    auto remove_player_status = [&](const std::string& name_or_displayname) {
        globals::bools::player_status.erase(name_or_displayname);
    };
    float content_width = 300.0f; // Adjusted width for a more compact list
    float padding = 5.5f;
    float header_height = 30.0f;
    float player_item_height = 52.0f; // Slightly taller to fit extra lines

    float total_width = content_width + (padding * 2);
    float total_height = main_size.y;

    ImGui::SetNextWindowSize(ImVec2(total_width, total_height), ImGuiCond_Always);
    ImGui::Begin("PlayerList", nullptr, window_flags);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetWindowPos();
    auto* avatar_mgr = overlay::get_avatar_manager();
    ImU32 text_color = IM_COL32(255, 255, 255, 255);
    ImU32 enemy_color = IM_COL32(64, 64, 64, 255);
    ImU32 friendly_color = IM_COL32(64, 64, 64, 255);
    ImU32 neutral_color = IM_COL32(64, 64, 64, 255);
    ImU32 client_color = IM_COL32(64, 64, 64, 255);
    ImU32 top_line_color = ImGui::GetColorU32(ImGuiCol_SliderGrab);
    
    // Draw glow effect around the playerlist if enabled
    if (globals::misc::menuglow) {
        ImGui::GetBackgroundDrawList()->AddShadowRect(window_pos, ImVec2(window_pos.x + total_width, window_pos.y + total_height), ImGui::ColorConvertFloat4ToU32(ImVec4(globals::misc::menuglowcolor[0], globals::misc::menuglowcolor[1], globals::misc::menuglowcolor[2], globals::misc::menuglowcolor[3])), 35, ImVec2(0, 0), ImDrawFlags_ShadowCutOutShapeBackground, 0.0f);
    }

    // Calculate visible players count for the header
    int total_visible_players = 0;
    if (!players.empty()) {
        std::string search_lower_count = player_search;
        std::transform(search_lower_count.begin(), search_lower_count.end(), search_lower_count.begin(),
            [](unsigned char c) { return std::tolower(c); });

        for (auto& player : players) {
            std::string display_lower_count = get_best_display(player);
            std::transform(display_lower_count.begin(), display_lower_count.end(), display_lower_count.begin(),
                [](unsigned char c) { return std::tolower(c); });
            std::string username_lower_count = player.name;
            std::transform(username_lower_count.begin(), username_lower_count.end(), username_lower_count.begin(),
                [](unsigned char c) { return std::tolower(c); });

            bool matches = search_lower_count.empty() ||
                display_lower_count.find(search_lower_count) != std::string::npos ||
                username_lower_count.find(search_lower_count) != std::string::npos;
            if (matches) {
                total_visible_players++;
            }
        }
    }
    char player_count_text[64];
    sprintf_s(player_count_text, "Players (%d)", total_visible_players);
    draw->AddText(ImVec2(window_pos.x + padding, window_pos.y + 8), text_color, player_count_text);

    ImGui::SetCursorPos(ImVec2(padding, header_height));
    ImGui::PushItemWidth(total_width - padding * 2);

    // Add a search icon inside the input field
    ImVec2 search_start = ImGui::GetCursorScreenPos();
    ImGuiStyle& s = ImGui::GetStyle();
    float icon_radius = 5.0f;
    float icon_spacing = 6.0f;
    float left_padding = s.FramePadding.x + (icon_radius * 2.0f) + icon_spacing;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(left_padding, s.FramePadding.y));
    ImGui::InputTextWithHint("##playersearch", "Search...", player_search, IM_ARRAYSIZE(player_search));
    ImGui::PopStyleVar();

    ImVec2 search_size = ImGui::GetItemRectSize();
    ImVec2 icon_center = ImVec2(search_start.x + s.FramePadding.x + icon_radius, search_start.y + search_size.y * 0.5f + 0.5f);
    ImU32 icon_color = IM_COL32(255, 255, 255, 255);

    ImDrawList* search_draw = ImGui::GetWindowDrawList();
    search_draw->AddCircle(icon_center, icon_radius, icon_color, 20, 1.4f);
    search_draw->AddLine(ImVec2(icon_center.x + icon_radius * 0.7f, icon_center.y + icon_radius * 0.7f),
                         ImVec2(icon_center.x + icon_radius * 1.5f, icon_center.y + icon_radius * 1.5f),
                         icon_color, 1.4f);

    ImGui::PopItemWidth();

    // Calculate dynamic height for the player list child window
    const float line_h = ImGui::GetTextLineHeight();
    float player_card_height = line_h * 4.0f + 14.0f; // Dynamic height to fit four lines plus padding
    float spacing_between_cards = 0.0f; // No spacing between cards
    float estimated_content_height = 0;

    if (!players.empty()) {
        std::string search_lower = player_search;
        std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(),
            [](unsigned char c) { return std::tolower(c); });

        int visible_players_count = 0;
        for (auto& player : players) {
            std::string display_lower = get_best_display(player);
            std::transform(display_lower.begin(), display_lower.end(), display_lower.begin(),
                [](unsigned char c) { return std::tolower(c); });
            std::string username_lower = player.name;
            std::transform(username_lower.begin(), username_lower.end(), username_lower.begin(),
                [](unsigned char c) { return std::tolower(c); });

            bool matches = search_lower.empty() ||
                display_lower.find(search_lower) != std::string::npos ||
                username_lower.find(search_lower) != std::string::npos;
            if (matches) {
                visible_players_count++;
            }
        }
        // Calculate total height needed for all visible player cards and their spacing
        estimated_content_height = (player_card_height * visible_players_count); // no spacing between cards
    } else {
        estimated_content_height = ImGui::CalcTextSize("No players found").y;
    }

    // Ensure the child window doesn't exceed the main window's available height
    float max_child_height = total_height - header_height - padding - ImGui::GetStyle().WindowPadding.y * 2 - 10; // Added extra buffer
    float actual_child_height = max_child_height; // Fill available height even with few players

    // Align child to search box edges
    ImVec2 child_pos = ImVec2(padding, header_height + search_size.y + s.ItemSpacing.y * 0.2f);
    ImGui::SetCursorPos(child_pos);
    ImGui::BeginChild("PlayerList", ImVec2(search_size.x, actual_child_height), true);
    if (players.empty()) {
        ImGui::Text("No players found");
    }
    else {
        std::string search_lower = player_search;
        std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(),
            [](unsigned char c) { return std::tolower(c); });

        bool any_match = false;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
        for (size_t i = 0; i < players.size(); i++) {
            auto& player = players[i];

            std::string display_lower = get_best_display(player);
            std::transform(display_lower.begin(), display_lower.end(), display_lower.begin(),
                [](unsigned char c) { return std::tolower(c); });
            std::string username_lower = player.name;
            std::transform(username_lower.begin(), username_lower.end(), username_lower.begin(),
                [](unsigned char c) { return std::tolower(c); });

            bool matches = search_lower.empty() ||
                display_lower.find(search_lower) != std::string::npos ||
                username_lower.find(search_lower) != std::string::npos;
            if (matches) {
                any_match = true;
                ImGui::PushID(static_cast<int>(i));

                bool is_client = (player.name == globals::instances::lp.name);
                bool is_spectating_this_player = (overlay::g_spectated_address == player.hrp.address);

                // Player Card dimensions (no background box)
                float current_card_width = ImGui::GetContentRegionAvail().x;
                ImVec2 card_start_pos = ImGui::GetCursorScreenPos();
                float card_height = player_card_height + 1.0f; // Minimal bump to avoid trailing gap
                ImVec2 card_end_pos = ImVec2(card_start_pos.x + current_card_width, card_start_pos.y + card_height);
                ImDrawList* draw_list = ImGui::GetWindowDrawList();

                // Advance cursor for content inside the card
                ImGui::SetCursorScreenPos(ImVec2(card_start_pos.x + 5, card_start_pos.y + 5));

                // Avatar
                ImVec2 avatar_start = ImGui::GetCursorScreenPos();
                ImVec2 avatar_size = ImVec2(30, 30); // Smaller avatar size
                ImVec2 avatar_end = ImVec2(avatar_start.x + avatar_size.x, avatar_start.y + avatar_size.y);
                ImTextureID avatar_texture = avatar_mgr->getAvatarTexture(player.userid.address);

                draw_list->AddRectFilled(avatar_start, avatar_end, IM_COL32(40, 40, 40, 255), 4.0f);
                draw_list->AddRect(avatar_start, avatar_end, IM_COL32(80, 80, 80, 255), 4.0f);
                ImGui::SetCursorScreenPos(avatar_start);
                if (avatar_texture) {
                    ImGui::Image(avatar_texture, avatar_size);
                } else {
                    draw_list->AddText(ImVec2(avatar_start.x + avatar_size.x / 2 - ImGui::CalcTextSize("IMG").x / 2,
                        avatar_start.y + avatar_size.y / 2 - ImGui::CalcTextSize("IMG").y / 2), IM_COL32(120, 120, 120, 255), "IMG");
                }

                // Player Name, ID, and @username
                float info_x = avatar_end.x + 8;
                float name_y = card_start_pos.y + 5;
                
                ImGui::SetCursorScreenPos(ImVec2(info_x, name_y));
                {
                    std::string display = get_best_display(player);
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(text_color), "%s", display.c_str());
                }

                // Get player health
                float current_health = player.humanoid.read_health();
                float max_health = player.humanoid.read_maxhealth();

                // Format health string
                char health_text[32];
                sprintf_s(health_text, "[%.0f]", current_health);

                // Position health text near the display name with a small clamp to the card edge
                ImVec2 health_text_size = ImGui::CalcTextSize(health_text);
                ImVec2 display_text_size = ImGui::CalcTextSize(get_best_display(player).c_str());
                float health_padding = 6.0f;
                float desired_x = info_x + display_text_size.x + health_padding;
                float max_x = card_end_pos.x - health_padding - health_text_size.x;
                ImGui::SetCursorScreenPos(ImVec2(std::min(desired_x, max_x), name_y));

                // Calculate health percentage
                float health_percentage = (max_health > 0.0f) ? (current_health / max_health) : 0.0f;

                // Interpolate color from red (low health) to green (full health)
                ImVec4 health_color_vec;
                health_color_vec.x = globals::visuals::healthbarcolor1[0] + (globals::visuals::healthbarcolor[0] - globals::visuals::healthbarcolor1[0]) * health_percentage;
                health_color_vec.y = globals::visuals::healthbarcolor1[1] + (globals::visuals::healthbarcolor[1] - globals::visuals::healthbarcolor1[1]) * health_percentage;
                health_color_vec.z = globals::visuals::healthbarcolor1[2] + (globals::visuals::healthbarcolor[2] - globals::visuals::healthbarcolor1[2]) * health_percentage;
                health_color_vec.w = 1.0f; // Alpha

                float prev_scale = ImGui::GetCurrentWindow()->FontWindowScale;
                ImGui::SetWindowFontScale(prev_scale * 0.9f);
                ImGui::TextColored(health_color_vec, "%s", health_text); // Color changes based on health
                ImGui::SetWindowFontScale(prev_scale);

                ImGui::SetCursorScreenPos(ImVec2(info_x, name_y + ImGui::GetTextLineHeight() + 1));
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(IM_COL32(180, 180, 180, 255)), "@%s", player.name.c_str()); // Username just below display

                ImGui::SetCursorScreenPos(ImVec2(info_x, name_y + ImGui::GetTextLineHeight() * 2 + 1));
                prev_scale = ImGui::GetCurrentWindow()->FontWindowScale;
                ImGui::SetWindowFontScale(prev_scale * 0.9f);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(IM_COL32(180, 180, 180, 255)), "ID: %llu", static_cast<unsigned long long>(player.userid.address));
                ImGui::SetWindowFontScale(prev_scale);
	
                // Determine if this player is currently in the target list
                bool is_in_target_list_for_status = std::find(
                    globals::visuals::target_only_list.begin(),
                    globals::visuals::target_only_list.end(),
                    player.name
                ) != globals::visuals::target_only_list.end();
	
                bool status_friendly = get_player_status(player.displayname);
                ImU32 status_col = status_friendly ? IM_COL32(0, 190, 0, 255) : IM_COL32(200, 40, 40, 255);
                const char* status_label = nullptr;
                if (is_client) {
                    status_col = IM_COL32(255, 255, 255, 255);
                    status_label = "Self";
                } else if (status_friendly) {
                    status_label = "Friendly";
                } else {
                    // Enemy by default, but if in target list show "Target"
                    status_label = is_in_target_list_for_status ? "Target" : "Enemy";
                }
                ImGui::SetCursorScreenPos(ImVec2(info_x, name_y + ImGui::GetTextLineHeight() * 3 - 2));
                prev_scale = ImGui::GetCurrentWindow()->FontWindowScale;
                ImGui::SetWindowFontScale(prev_scale * 0.9f);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(status_col), "%s", status_label);
                ImGui::SetWindowFontScale(prev_scale);
                // Make avatar and name clickable
                ImGui::SetCursorScreenPos(card_start_pos);
                ImGui::InvisibleButton("##player_card_interaction", ImVec2(current_card_width, card_height)); 
                bool card_hovered = ImGui::IsItemHovered();
                if (card_hovered) {
                    // Soft outline on hover
                    draw_list->AddRect(card_start_pos - ImVec2(1.0f, 1.0f), card_end_pos + ImVec2(1.0f, 1.0f), IM_COL32(120, 120, 120, 140), 6.0f, 0, 1.8f);
                }
                if (!is_client && card_hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) { // Handle right-click for context menu, but not for local player
                    ImGui::OpenPopup("PlayerContextMenu");
                    overlay::g_selected_player_address = player.hrp.address; // Select this player for context menu options
                }
                // Left-click handling is intentionally removed to prevent dropdown/selection.
                if (!is_client && ImGui::BeginPopup("PlayerContextMenu")) { // Only show context menu if not local player
                    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 6.0f);
                    // Bold-ish header for "Options for:" then inline @username without extra spacing
                    const char* options_label = "Options for:";
                    ImVec2 header_pos = ImGui::GetCursorScreenPos();
                    ImDrawList* header_dl = ImGui::GetWindowDrawList();
                    ImU32 header_col = ImGui::GetColorU32(ImGuiCol_Text);
                    header_dl->AddText(header_pos, header_col, options_label);
                    header_dl->AddText(header_pos + ImVec2(0.6f, 0.0f), header_col, options_label); // slight offset pass to fake bold
                    ImVec2 header_size = ImGui::CalcTextSize(options_label);
                    ImGui::Dummy(header_size);
                    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 0.6f);
                    ImGui::Text(" @%s", player.name.c_str());
                    ImVec2 header_max = ImGui::GetItemRectMax();
                    ImGui::Separator();

                    // Custom menu entry helper to add hover gray text and click handling
                    auto menu_entry = [&](const char* id, const char* label, const std::function<void()>& on_activate, ImVec2& options_max) {
                        ImGui::PushID(id);
                        ImVec2 padding = ImGui::GetStyle().FramePadding;
                        ImVec2 text_size = ImGui::CalcTextSize(label);
                        ImVec2 item_size(text_size.x + padding.x * 2.0f, text_size.y + padding.y * 2.0f);
                        if (ImGui::InvisibleButton("##btn", item_size)) {
                            if (on_activate) on_activate();
                        }
                        bool hovered = ImGui::IsItemHovered();
                        ImVec2 min = ImGui::GetItemRectMin();
                        ImVec2 max = ImGui::GetItemRectMax();
                        options_max.x = std::max(options_max.x, max.x);
                        options_max.y = std::max(options_max.y, max.y);
                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        if (hovered) {
                            dl->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_HeaderHovered), ImGui::GetStyle().FrameRounding * 0.25f);
                        }
                        ImU32 text_col = hovered ? IM_COL32(200, 200, 200, 255) : ImGui::GetColorU32(ImGuiCol_Text);
                        dl->AddText(ImVec2(min.x + padding.x, min.y + padding.y), text_col, label);
                        ImGui::PopID();
                    };

                    ImVec2 options_min = header_pos;
                    ImVec2 options_max = header_max;

                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

                    bool is_in_target_list = std::find(globals::visuals::target_only_list.begin(), globals::visuals::target_only_list.end(), player.name) != globals::visuals::target_only_list.end();
                    bool current_status_is_friendly = get_player_status(player.displayname);
                    double now_time = ImGui::GetTime();

                    menu_entry("SpectatePlayer", is_spectating_this_player ? "Stop Spectating" : "Spectate", [&]() {
                        if (is_spectating_this_player) {
                            globals::instances::localplayer.unspectate();
                            overlay::g_spectated_address = 0;
                            overlay::g_spectated_player_userid = 0;
                        } else {
                            globals::instances::localplayer.spectate(player.hrp.address);
                            overlay::g_spectated_address = player.hrp.address;
                            overlay::g_spectated_player_userid = player.userid.address;
                        }
                    }, options_max);

                    menu_entry("ToggleTarget", is_in_target_list ? "Remove Target" : "Add Target", [&]() {
                        if (is_in_target_list) {
                            globals::visuals::target_only_list.erase(
                                std::remove(globals::visuals::target_only_list.begin(), globals::visuals::target_only_list.end(), player.name),
                                globals::visuals::target_only_list.end());
                        } else {
                            globals::visuals::target_only_list.push_back(player.name);
                        }
                    }, options_max);

                    menu_entry("ToggleStatus", current_status_is_friendly ? "Set as Enemy" : "Set as Friendly", [&]() {
                        if (current_status_is_friendly) {
                            set_player_status(player.displayname, false);
                        } else {
                            set_player_status(player.displayname, true);
                        }
                    }, options_max);

                    std::string username_label = "Copy Username";
                    auto user_until_it = g_copy_username_timeout.find(player.userid.address);
                    if (user_until_it != g_copy_username_timeout.end() && user_until_it->second > now_time) {
                        username_label = "Copied: @" + player.name;
                    }

                    menu_entry("CopyUsername", username_label.c_str(), [&]() {
                        if (OpenClipboard(nullptr)) {
                            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, player.name.size() + 1);
                            if (hMem) {
                                memcpy(GlobalLock(hMem), player.name.c_str(), player.name.size() + 1);
                                GlobalUnlock(hMem);
                                EmptyClipboard();
                                SetClipboardData(CF_TEXT, hMem);
                                CloseClipboard();
                                g_copy_username_timeout[player.userid.address] = ImGui::GetTime() + 3.0;
                                // Optional: Show a notification that the username was copied
                                // notification::add("Copied username to clipboard");
                            }
                        }
                    }, options_max);

                    std::string user_id_str = std::to_string(player.userid.address);
                    std::string userid_label = "Copy User ID";
                    auto userid_until_it = g_copy_userid_timeout.find(player.userid.address);
                    if (userid_until_it != g_copy_userid_timeout.end() && userid_until_it->second > now_time) {
                        userid_label = "Copied: " + user_id_str;
                    }

                    menu_entry("CopyUserID", userid_label.c_str(), [&]() {
                        if (OpenClipboard(nullptr)) {
                            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, user_id_str.size() + 1);
                            if (hMem) {
                                memcpy(GlobalLock(hMem), user_id_str.c_str(), user_id_str.size() + 1);
                                GlobalUnlock(hMem);
                                EmptyClipboard();
                                SetClipboardData(CF_TEXT, hMem);
                                CloseClipboard();
                                g_copy_userid_timeout[player.userid.address] = ImGui::GetTime() + 3.0;
                                // Optional: Show a notification that the ID was copied
                                // notification::add("Copied user ID to clipboard");
                            }
                        }
                    }, options_max);

                    menu_entry("TeleportToPlayer", "Teleport To Player", [&]() {
                        globals::instances::lp.hrp.write_position(player.hrp.get_pos());
                    }, options_max);

                    ImGui::PopStyleVar();

                    // Outline around option group
                    ImVec2 pad(6.0f, 4.0f);
                    ImGui::GetWindowDrawList()->AddRect(options_min - pad, options_max + pad, IM_COL32(80, 80, 80, 200), ImGui::GetStyle().FrameRounding * 0.35f);

                    ImGui::PopStyleVar(); // popup rounding
                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }
        }
        ImGui::PopStyleVar(); // reset item spacing
        if (!any_match) {
            ImVec2 area = ImGui::GetContentRegionAvail();
            const char* no_msg = "No players found";
            ImVec2 text_size = ImGui::CalcTextSize(no_msg);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (area.x - text_size.x) * 0.5f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (area.y - text_size.y) * 0.5f);
            ImGui::TextUnformatted(no_msg);
        }
    }
    ImGui::EndChild();

    // Inner outline around search + list area
    ImVec2 outline_min = window_pos + ImVec2(padding - 1.0f, header_height - 1.0f);
    ImVec2 outline_max = outline_min + ImVec2(search_size.x + 2.0f, search_size.y + s.ItemSpacing.y * 0.2f + actual_child_height + 2.0f);
    draw->AddRect(outline_min, outline_max, IM_COL32(70, 70, 70, 200), 2.0f);

    style.WindowRounding = rounded;
    ImGui::End();

    if (overlay::g_playerlist_just_opened) {
        overlay::g_playerlist_just_opened = false; // Reset the flag after the first frame
    }

    // Removed left-click deselect logic as left-click functionality is removed.
}

void render_watermark() {
    if (!globals::misc::watermark) return;

    static ImVec2 watermark_pos = ImVec2(10, 10);
    static bool first_time = true;

    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = g.Style;
    float rounded = style.WindowRounding;
    style.WindowRounding = 0;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_AlwaysAutoResize;

    if (!overlay::visible) {
        window_flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
    }

    if (first_time || !overlay::visible) {
        ImGui::SetNextWindowPos(watermark_pos, ImGuiCond_Always);
    }

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    struct tm local_time;
    localtime_s(&local_time, &time_t);

    char time_str[64];
    std::strftime(time_str, sizeof(time_str), "%H:%M:%S", &local_time);
    char date_str[64];
    std::strftime(date_str, sizeof(date_str), "%d-%m-%Y", &local_time);

    ImGuiIO& io = ImGui::GetIO();
    int fps = (int)(1.0f / io.DeltaTime);

    std::string watermark_text = "Number (N)ine";
    std::vector<std::string> info_parts;

    if (first_time && globals::misc::watermarkstuff != nullptr) {
        (*globals::misc::watermarkstuff)[0] = 1;
        (*globals::misc::watermarkstuff)[1] = 1;
    }

    if (globals::misc::watermarkstuff != nullptr) {
        if ((*globals::misc::watermarkstuff)[1] == 1) {
            info_parts.push_back(globals::instances::username);
        }
        if ((*globals::misc::watermarkstuff)[2] == 1) {
            info_parts.push_back(std::string(date_str));
        }
        if ((*globals::misc::watermarkstuff)[0] == 1) {
            info_parts.push_back("FPS: " + std::to_string(fps));
        }
    }

    // Always append current aim part info if available (helps verify airpart behavior)
    if (!globals::combat::last_used_aimpart.empty()) {
        std::string part_info = std::string("Part: ") + globals::combat::last_used_aimpart;
        if (globals::combat::last_used_aimpart_air) part_info += " (Air)";
        info_parts.push_back(part_info);
    }

    if (!info_parts.empty()) {
        watermark_text += " | ";
        for (size_t i = 0; i < info_parts.size(); i++) {
            if (i > 0) watermark_text += " | ";
            watermark_text += info_parts[i];
        }
    }

    ImVec2 text_size = ImGui::CalcTextSize(watermark_text.c_str());
    float padding_x = 3.0f;
    float padding_y = 3.0f;
    float total_width = text_size.x + (padding_x * 2) + 3.0f;
    float total_height = text_size.y + (padding_y * 2) + 1.0f;

    ImGui::SetNextWindowSize(ImVec2(total_width, total_height), ImGuiCond_Always);
    ImGui::Begin("Watermark", nullptr, window_flags);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();

    if (overlay::visible) {
        watermark_pos = window_pos;
    }

    ImU32 bg_color = IM_COL32(15, 15, 15, 200);
    ImU32 text_color = IM_COL32(255, 255, 255, 255);
    ImU32 outline_color = IM_COL32(60, 60, 60, 255);
    ImU32 top_line_color = ImGui::GetColorU32(ImGuiCol_SliderGrab);

    ImVec2 text_pos = ImVec2(
        window_pos.x + padding_x,
        window_pos.y + padding_y
    );
    draw->AddText(text_pos, text_color, watermark_text.c_str());

    if (first_time) {
        first_time = false;
    }

    style.WindowRounding = rounded;
    ImGui::End();
}

void render_keybind_list() {
    if (!globals::misc::keybinds) return;


    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = g.Style;

    float rounded;
    rounded = style.WindowRounding;
    style.WindowRounding = 0;



    static ImVec2 keybind_pos = ImVec2(5, GetSystemMetrics(SM_CYSCREEN) / 2 - 10);
    static bool first_time = true;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_AlwaysAutoResize;

    if (!overlay::visible) {
        window_flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
    }

    if (first_time || (!overlay::visible)) {
        ImGui::SetNextWindowPos(keybind_pos, ImGuiCond_Always);
        first_time = false;
    }

    std::vector<std::pair<std::string, std::string>> active_keybinds;

    if (globals::combat::aimbot && globals::combat::aimbotkeybind.enabled) {
        active_keybinds.push_back({ "Aimbot", globals::combat::aimbotkeybind.get_key_name() });
    }

    if (globals::combat::silentaim && globals::combat::silentaimkeybind.enabled) {
        active_keybinds.push_back({ "Silent Aim", globals::combat::silentaimkeybind.get_key_name() });
    }

    if (globals::combat::orbit && globals::combat::orbitkeybind.enabled) {
        active_keybinds.push_back({ "Orbit", globals::combat::orbitkeybind.get_key_name() });
    }

    if (globals::combat::triggerbot && globals::combat::triggerbotkeybind.enabled) {
        active_keybinds.push_back({ "Triggerbot", globals::combat::triggerbotkeybind.get_key_name() });
    }


    if (globals::misc::speed && globals::misc::speedkeybind.enabled) {
        active_keybinds.push_back({ "Speed", globals::misc::speedkeybind.get_key_name() });
    }

    // No Jump Cooldown is always on, no keybind to display

    if (globals::misc::flight && globals::misc::flightkeybind.enabled) {
        active_keybinds.push_back({ "Flight", globals::misc::flightkeybind.get_key_name() });
    }

    if (globals::misc::keybindsstyle == 1) {
        struct KeybindInfo {
            std::string name;
            keybind* bind;
            bool* enabled;
        };

        std::vector<KeybindInfo> all_keybinds = {
            {"Aimbot", &globals::combat::aimbotkeybind, &globals::combat::aimbot},
            {"Silent Aim", &globals::combat::silentaimkeybind, &globals::combat::silentaim},
            {"Orbit", &globals::combat::orbitkeybind, &globals::combat::orbit},
            {"Triggerbot", &globals::combat::triggerbotkeybind, &globals::combat::triggerbot},
            {"Speed", &globals::misc::speedkeybind, &globals::misc::speed},
            {"Flight", &globals::misc::flightkeybind, &globals::misc::flight}
        };

        active_keybinds.clear();
        for (const auto& info : all_keybinds) {
            if (*info.enabled) {
                active_keybinds.push_back({ info.name, info.bind->get_key_name() });
            }
        }
    }

    ImVec2 title_size = ImGui::CalcTextSize("Keybinds");
    float content_width = title_size.x;

    for (const auto& bind : active_keybinds) {
        std::string full_text = bind.first + ": " + bind.second;
        ImVec2 text_size = ImGui::CalcTextSize(full_text.c_str());
        content_width = std::max(content_width, text_size.x);
    }

    float padding_x = 3.0f;
    float padding_y = 3.0f;
    float line_spacing = ImGui::GetTextLineHeight() + 2.0f;

    float total_width = content_width + (padding_x * 2) + 1.0f;
    float total_height = padding_y * 2 + title_size.y + 2;

    if (!active_keybinds.empty()) {
        total_height += active_keybinds.size() * line_spacing;
    }

    ImGui::SetNextWindowSize(ImVec2(total_width, total_height), ImGuiCond_Always);

    ImGui::Begin("Keybinds", nullptr, window_flags);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();

    if (overlay::visible) {
        keybind_pos = window_pos;
    }

    ImU32 bg_color = IM_COL32(15, 15, 15, 200);
    ImU32 text_color = ImGui::GetColorU32(ImGuiCol_Text);
    ImU32 outline_color = ImGui::GetColorU32(ImGuiCol_Border);
    ImU32 top_line_color = ImGui::GetColorU32(ImGuiCol_SliderGrab);
    ImU32 active_color = ImGui::GetColorU32(ImGuiCol_SliderGrab);

                        
                                
    draw->AddRectFilled(
        window_pos,
        ImVec2(window_pos.x + window_size.x, window_pos.y + 2),
        top_line_color,
        0.0f
    );

    ImVec2 title_pos = ImVec2(
        window_pos.x + padding_x,
        window_pos.y + padding_y
    );

    draw->AddText(
        title_pos,
        text_color,
        "Keybinds"
    );

    if (!active_keybinds.empty()) {
        float current_y = title_pos.y + title_size.y + 4.0f;

        std::sort(active_keybinds.begin(), active_keybinds.end(),
            [](const std::pair<std::string, std::string>& a, const std::pair<std::string, std::string>& b) {
                std::string full_a = a.first + ": " + a.second;
                std::string full_b = b.first + ": " + b.second;
                return full_a.length() > full_b.length();
            });

        for (const auto& bind : active_keybinds) {
            std::string full_text = bind.first + ": " + bind.second;

            ImVec2 keybind_pos = ImVec2(window_pos.x + padding_x, current_y);

            if (globals::misc::keybindsstyle == 1) {
                bool is_active = false;
                if (bind.first == "Aimbot") is_active = globals::combat::aimbotkeybind.enabled;
                else if (bind.first == "Silent Aim") is_active = globals::combat::silentaimkeybind.enabled;
                else if (bind.first == "Orbit") is_active = globals::combat::orbitkeybind.enabled;
                else if (bind.first == "Triggerbot") is_active = globals::combat::triggerbotkeybind.enabled;
                else if (bind.first == "Speed") is_active = globals::misc::speedkeybind.enabled;
                // Removed No Jump Cooldown keybind check
                else if (bind.first == "Flight") is_active = globals::misc::flightkeybind.enabled;

                draw->AddText(keybind_pos, is_active ? active_color : text_color, full_text.c_str());
            }
            else {
                draw->AddText(keybind_pos, text_color, full_text.c_str());
            }

            current_y += line_spacing;
        }
    }
    style.WindowRounding = rounded;
    ImGui::End();
}

bool Bind(keybind* keybind, const ImVec2& size_arg = ImVec2(0, 0), bool clicked = false, ImGuiButtonFlags flags = 0) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(keybind->get_name().c_str());

    ImVec2 pos = window->DC.CursorPos;
    if ((flags & ImGuiButtonFlags_AlignTextBaseLine) &&
        style.FramePadding.y < window->DC.CurrLineTextBaseOffset)
        pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;

    std::string key_text = keybind->get_key_name();
    if (keybind->waiting_for_input) {
        key_text = "Waiting";
    }

    std::string display_text = "[" + key_text + "]";

    ImVec2 text_size = ImGui::CalcTextSize(display_text.c_str());
    const float padding_x = style.FramePadding.x;
    const float padding_y = style.FramePadding.y * 0.75f;
    ImVec2 calculated_size = ImVec2(
        text_size.x + padding_x * 2.0f,
        text_size.y + padding_y * 2.0f
    );

    // Keep a consistent pill width so the bind sits flush to the right of labels
    const float min_width = size_arg.x > 0 ? size_arg.x : 90.0f;
    const float min_height = size_arg.y > 0 ? size_arg.y : ImGui::GetTextLineHeight() + padding_y * 2.0f;

    const float remaining_width = ImGui::GetContentRegionAvail().x;

    ImVec2 size = ImVec2(
        ImMax(ImMax(min_width, calculated_size.x), remaining_width),
        ImMax(min_height, calculated_size.y)
    );

    const ImRect bb(pos, pos + size);
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    if (g.CurrentItemFlags & ImGuiItemFlags_ButtonRepeat)
        flags |= ImGuiButtonFlags_PressedOnRelease;

    bool hovered = false, held = false;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);

    bool value_changed = false;
    int key = keybind->key;

    auto io = ImGui::GetIO();

    if (ImGui::GetIO().MouseClicked[0] && hovered)
    {
        if (g.ActiveId == id)
        {
            keybind->waiting_for_input = true;
        }
    }
    else if (ImGui::GetIO().MouseClicked[1] && hovered)
    {
        ImGui::OpenPopup(keybind->get_name().c_str());
    }
    else if (ImGui::GetIO().MouseClicked[0] && !hovered)
    {
        if (g.ActiveId == id)
            ImGui::ClearActiveID();
    }

    if (keybind->waiting_for_input)
    {
        if (ImGui::GetIO().MouseClicked[0] && !hovered)
        {
            keybind->key = VK_LBUTTON;
            ImGui::ClearActiveID();
            keybind->waiting_for_input = false;
        }
        else
        {
            if (keybind->set_key())
            {
                ImGui::ClearActiveID();
                keybind->waiting_for_input = false;
            }
        }
    }

    // Render only the text so the background stays transparent
    ImGui::RenderNavHighlight(bb, id);

    // Render text
    ImVec4 textColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    ImVec2 text_pos = ImVec2(bb.Max.x - padding_x - text_size.x, bb.Min.y + (size.y - text_size.y) * 0.5f);
    window->DrawList->AddText(text_pos, ImGui::GetColorU32(textColor), display_text.c_str());

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_Popup | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar;

    if (ImGui::BeginPopup(keybind->get_name().c_str()))
    {
        {
            if (ImGui::Selectable("Hold", keybind->type == keybind::HOLD)) keybind->type = keybind::HOLD;
            if (ImGui::Selectable("Toggle", keybind->type == keybind::TOGGLE)) keybind->type = keybind::TOGGLE;
            if (ImGui::Selectable("Always", keybind->type == keybind::ALWAYS)) keybind->type = keybind::ALWAYS;
        }
        ImGui::EndPopup();
    }

    return pressed;
}

void draw_shadowed_text(const char* label) {
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    ImDrawList* pDrawList = ImGui::GetWindowDrawList();

    ImVec2 current_cursor_pos = ImGui::GetCursorScreenPos();
    ImU32 shadow_color = ImGui::GetColorU32(ImGuiCol_TitleBg); // Using TitleBg for shadow color

    // Draw shadow text with slight offsets
    pDrawList->AddText(ImVec2(current_cursor_pos.x + 1, current_cursor_pos.y + 1), shadow_color, label);
    pDrawList->AddText(ImVec2(current_cursor_pos.x - 1, current_cursor_pos.y - 1), shadow_color, label);
    pDrawList->AddText(ImVec2(current_cursor_pos.x + 1, current_cursor_pos.y - 1), shadow_color, label);
    pDrawList->AddText(ImVec2(current_cursor_pos.x - 1, current_cursor_pos.y + 1), shadow_color, label);

    // Draw the main text using ImGui::Text, which handles cursor advancement
    ImGui::Text("%s", label);
}

void overlay::load_interface()
{
    g_singleInstanceMutex = CreateMutexW(nullptr, TRUE, L"frizy_keyless_overlay_mutex");
    if (!g_singleInstanceMutex)
    {
        ExitProcess(0);
    }
    DWORD single_instance_error = GetLastError();
    if (single_instance_error == ERROR_ALREADY_EXISTS)
    {
        ReleaseSingleInstanceMutex();
        ExitProcess(0);
    }


    ImGui_ImplWin32_EnableDpiAwareness();

    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowExW(WS_EX_TOPMOST, wc.lpszClassName, L"SUNK OVERLAY", WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), nullptr, nullptr, wc.hInstance, nullptr);

    wc.cbClsExtra = NULL;
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.cbWndExtra = NULL;
    wc.hbrBackground = NULL; // Set background brush to NULL to prevent native background drawing
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = L"base";
    wc.lpszMenuName = nullptr;
    wc.style = CS_VREDRAW | CS_HREDRAW;

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);
    MARGINS margin = { -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margin);



    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        ReleaseSingleInstanceMutex();
        return;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.MouseDrawCursor = false; // Always hide ImGui's default cursor
    
    ImGuiStyle& style = ImGui::GetStyle();
    apply_theme();


    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    ImVec4 clear_color = ImVec4(0.f, 0.f, 0.f, 0.f);

    bool done = false;

    initialize_avatar_system();

    // Initialize star effect
    g_starEffect.Init(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));

    AddTrayIcon(hwnd);

    static bool silentaim_state_before_abort = false;
        static bool was_aborted = false;

    while (done == false)
    {
        try {
            if (!globals::firstreceived) { ReleaseSingleInstanceMutex(); return; }

            apply_theme();
            static bool last_tray_visible_state = overlay::visible;
            if (last_tray_visible_state != overlay::visible) {
                UpdateTrayMenuText();
                last_tray_visible_state = overlay::visible;
            }


        // Avatar management with exception handling
        try {
            auto avatar_mgr = overlay::get_avatar_manager();
            for (roblox::player entity : globals::instances::cachedplayers) {

                if (avatar_mgr) {
                    if (!entity.pictureDownloaded) {
                        avatar_mgr->requestAvatar(entity.userid.address);
                    }
                    else {
                        continue;
                    }

                }
                else {
                    break;
                }
            }
        } catch (...) {
            // Silently handle avatar-related exceptions
        }

        // Roblox window detection with exception handling
        static HWND robloxWindow = FindWindowA(0, "Roblox");
        try {
            robloxWindow = FindWindowA(0, "Roblox");

            update_avatars();
        } catch (...) {
            // Silently handle window/avatar update exceptions
        }

        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);

            if (msg.message == WM_QUIT)
            {
                done = true;
                break;
            }
        }

        if (done == true)
        {
            break;
        }

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Update star effect (optional)
        if (globals::misc::overlay_stars) {
            g_starEffect.Update(ImGui::GetIO().DeltaTime);
        }

        // Render blurred background and (optional) star effect behind everything
        if (overlay::visible) {
            ImDrawList* background_draw_list = ImGui::GetBackgroundDrawList();
            background_draw_list->AddRectFilled(ImVec2(0, 0), ImVec2((float)GetSystemMetrics(SM_CXSCREEN), (float)GetSystemMetrics(SM_CYSCREEN)), IM_COL32(0, 0, 0, 150)); // Semi-transparent black for blur effect
        if (globals::misc::overlay_stars) {
            g_starEffect.Render(background_draw_list, globals::misc::overlay_star_color);
        }
    }

        // Set initial position for the main ImGui window once, or when it becomes visible
        if (globals::misc::main_window_first_time || overlay::visible) {
            ImGui::SetNextWindowPos(ImVec2(GetSystemMetrics(SM_CXSCREEN) / 2 - (900 / 2), GetSystemMetrics(SM_CYSCREEN) / 2 - (550 / 2)), ImGuiCond_Always);
            globals::misc::main_window_first_time = false;
        }


        const bool team = (*globals::combat::flags)[0] != 0;
        const bool knock = (*globals::combat::flags)[1] != 0;
        const bool range = (*globals::combat::flags)[2] != 0;
        const bool walls = (*globals::combat::flags)[3] != 0; // Health removed, so walls is now at index 3

        globals::misc::speedkeybind.update();
        // globals::misc::nojumpcooldownkeybind.update(); // Removed keybind update
        globals::misc::flightkeybind.update();
        globals::misc::jumppowerkeybind.update(); // Update jumppower keybind

        globals::combat::teamcheck = team;
        globals::combat::knockcheck = knock;
        globals::combat::rangecheck = range;
        globals::combat::wallcheck = walls;
        globals::combat::healthcheck = false; // Explicitly set healthcheck to false or manage it separately
        auto_respectate_follow_target(globals::instances::cachedplayers); // Keep spectate target updated even when the menu is closed


        static float original_jumppower_value = 50.0f; // Store original jumppower value
        static bool jumppower_modified = false;
        if (globals::misc::jumppower && globals::misc::jumppowerkeybind.enabled) {
            if (!jumppower_modified) {
                original_jumppower_value = globals::instances::lp.humanoid.read_jumppower(); // Store original jumppower
                jumppower_modified = true;
            }
            globals::instances::lp.humanoid.write_jumppower(globals::misc::jumppowervalue); // Set jumppower to desired value
        }
        else if (jumppower_modified) {
            globals::instances::lp.humanoid.write_jumppower(original_jumppower_value); // Revert to original jumppower
            jumppower_modified = false;
        }

        static float original_nojumpcooldown_jumppower = 50.0f; // Keep this for potential future use or if jumppower is re-introduced
        static bool nojumpcooldown_modified = false;
        if (globals::misc::nojumpcooldown) { // No keybind check needed
            if (!nojumpcooldown_modified) {
                original_nojumpcooldown_jumppower = globals::instances::lp.humanoid.read_jumppower(); // Store original jumppower
                nojumpcooldown_modified = true;
            }
            frizy::misc::nojumpcooldown(); // Call the new function
        }
        else if (nojumpcooldown_modified) {
            globals::instances::lp.humanoid.write_jumppower(original_nojumpcooldown_jumppower); // Revert to original jumppower
            nojumpcooldown_modified = false;
        }

        frizy::misc::jumppower(); // Call the jumppower function

        if (FindWindowA(0, "Roblox") && (GetForegroundWindow() == FindWindowA(0, "Roblox") || GetForegroundWindow() == hwnd)) {
            globals::focused = true;
        }
        else {
            globals::focused = false;
        }
        if (FindWindowA(0, "Roblox"))
        {


            auto drawbglist = ImGui::GetBackgroundDrawList();
            POINT cursor_pos;
            GetCursorPos(&cursor_pos);
            ScreenToClient(robloxWindow, &cursor_pos);
            ImVec2 mousepos = ImVec2((float)cursor_pos.x, (float)cursor_pos.y);
            
            // Always render lightweight overlays independent of menu visibility
            // Watermark and (optionally) keybind list respect their own enable flags
            render_watermark();
            render_keybind_list();

            if (overlay::visible)
            {
                static ImVec2 current_dimensions;
                render_explorer();
                current_dimensions = ImVec2(globals::instances::visualengine.GetDimensins().x, globals::instances::visualengine.GetDimensins().y);
            }
            // Only draw FOV circles if Roblox is focused
            if (keybind::is_roblox_focused()) {
                float current_time = ImGui::GetTime();
                float rotation_angle_aimbot = globals::combat::spin_fov_aimbot ? current_time * globals::combat::spin_fov_aimbot_speed : 0.0f;
                float rotation_angle_silentaim = globals::combat::spin_fov_silentaim ? current_time * globals::combat::spin_fov_silentaim_speed : 0.0f;

                // Helper function for rotating a point around a center
                auto rotate_point = [&](ImVec2 p, ImVec2 center, float angle) {
                    float s = sin(angle);
                    float c = cos(angle);
                    p.x -= center.x;
                    p.y -= center.y;
                    float xnew = p.x * c - p.y * s;
                    float ynew = p.x * s + p.y * c;
                    return ImVec2(xnew + center.x, ynew + center.y);
                };

                // Helper function for drawing a rotated N-gon with optional fill
                auto AddRotatedNgon = [&](ImDrawList* draw_list, ImVec2 center, float radius, ImU32 outline_color, int num_segments, float thickness, float angle_offset = 0.0f, bool fill = false, ImU32 fill_color = 0) {
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
                };

                if (globals::combat::drawfov) {
                    ImVec4 fov_color_vec = ImVec4(globals::combat::fovcolor[0], globals::combat::fovcolor[1], globals::combat::fovcolor[2], globals::combat::fovtransparency);
                    ImU32 fov_color_with_transparency = ImGui::ColorConvertFloat4ToU32(fov_color_vec);
                    ImU32 fov_fill_color = 0;
                    if (globals::combat::fovfill) {
                        ImVec4 fov_fill_vec = ImVec4(globals::combat::fovfillcolor[0], globals::combat::fovfillcolor[1], globals::combat::fovfillcolor[2], globals::combat::fovfillcolor[3]);
                        float fill_alpha_scale = ImClamp(globals::combat::fovfilltransparency, 0.0f, 1.0f);
                        fov_fill_vec.w *= fill_alpha_scale;
                        fov_fill_color = ImGui::ColorConvertFloat4ToU32(fov_fill_vec);
                    }
                    const char* fovShapes[] = { "Circle", "Square", "Triangle", "Pentagon", "Hexagon", "Octagon" };

                    switch (globals::combat::fovshape) {
                    case 0: // Circle
                        if (globals::combat::spin_fov_aimbot && globals::combat::fovshape != 0) { // Added condition to prevent spin with circle
                            float orbit_radius = globals::combat::fovsize * 0.2f; // Small orbit radius relative to FOV size
                            ImVec2 orbiting_center = ImVec2(mousepos.x + orbit_radius * cos(rotation_angle_aimbot), mousepos.y + orbit_radius * sin(rotation_angle_aimbot));
                            if (globals::combat::fovfill && fov_fill_color != 0) {
                                drawbglist->AddCircleFilled(orbiting_center, globals::combat::fovsize, fov_fill_color);
                            }
                            drawbglist->AddCircle(orbiting_center, globals::combat::fovsize, fov_color_with_transparency, globals::combat::fovthickness);
                        } else {
                            if (globals::combat::fovfill && fov_fill_color != 0) {
                                drawbglist->AddCircleFilled(mousepos, globals::combat::fovsize, fov_fill_color);
                            }
                            drawbglist->AddCircle(mousepos, globals::combat::fovsize, fov_color_with_transparency, globals::combat::fovthickness);
                        }
                        break;
                    case 1: // Square
                        AddRotatedNgon(drawbglist, mousepos, globals::combat::fovsize, fov_color_with_transparency, 4, globals::combat::fovthickness, globals::combat::spin_fov_aimbot ? rotation_angle_aimbot + IM_PI / 4.0f : IM_PI / 4.0f, globals::combat::fovfill, fov_fill_color); // Rotate square by 45 degrees initially
                        break;
                    case 2: // Triangle
                        AddRotatedNgon(drawbglist, mousepos, globals::combat::fovsize, fov_color_with_transparency, 3, globals::combat::fovthickness, globals::combat::spin_fov_aimbot ? rotation_angle_aimbot - IM_PI / 2.0f : -IM_PI / 2.0f, globals::combat::fovfill, fov_fill_color);
                        break;
                    case 3: // Pentagon
                        AddRotatedNgon(drawbglist, mousepos, globals::combat::fovsize, fov_color_with_transparency, 5, globals::combat::fovthickness, globals::combat::spin_fov_aimbot ? rotation_angle_aimbot - IM_PI / 2.0f : -IM_PI / 2.0f, globals::combat::fovfill, fov_fill_color);
                        break;
                    case 4: // Hexagon
                        AddRotatedNgon(drawbglist, mousepos, globals::combat::fovsize, fov_color_with_transparency, 6, globals::combat::fovthickness, globals::combat::spin_fov_aimbot ? rotation_angle_aimbot : 0.0f, globals::combat::fovfill, fov_fill_color);
                        break;
                    case 5: // Octagon
                        AddRotatedNgon(drawbglist, mousepos, globals::combat::fovsize, fov_color_with_transparency, 8, globals::combat::fovthickness, globals::combat::spin_fov_aimbot ? rotation_angle_aimbot : 0.0f, globals::combat::fovfill, fov_fill_color);
                        break;
                    }
                }
                if (globals::combat::drawsilentaimfov) {
                    POINT cursor_pos;
                    GetCursorPos(&cursor_pos);
                    ScreenToClient(robloxWindow, &cursor_pos);
                    ImVec2 mousepos = ImVec2((float)cursor_pos.x, (float)cursor_pos.y);
                    ImVec4 silent_aim_fov_color_vec = ImVec4(globals::combat::silentaimfovcolor[0], globals::combat::silentaimfovcolor[1], globals::combat::silentaimfovcolor[2], globals::combat::silentaimfovtransparency);
                    ImU32 silent_aim_fov_color_with_transparency = ImGui::ColorConvertFloat4ToU32(silent_aim_fov_color_vec);
                    ImU32 silent_aim_fov_fill_color = 0;
                    if (globals::combat::silentaimfovfill) {
                        ImVec4 fill_vec = ImVec4(globals::combat::silentaimfovfillcolor[0], globals::combat::silentaimfovfillcolor[1], globals::combat::silentaimfovfillcolor[2], globals::combat::silentaimfovfillcolor[3]);
                        float fill_alpha_scale = ImClamp(globals::combat::silentaimfovfilltransparency, 0.0f, 1.0f);
                        fill_vec.w *= fill_alpha_scale;
                        silent_aim_fov_fill_color = ImGui::ColorConvertFloat4ToU32(fill_vec);
                    }
                    const char* fovShapes[] = { "Circle", "Square", "Triangle", "Pentagon", "Hexagon", "Octagon" };

                    switch (globals::combat::silentaimfovshape) {
                    case 0: // Circle
                        if (globals::combat::spin_fov_silentaim && globals::combat::silentaimfovshape != 0) { // Added condition to prevent spin with circle
                            float orbit_radius = globals::combat::silentaimfovsize * 0.2f; // Small orbit radius relative to FOV size
                            ImVec2 orbiting_center = ImVec2(mousepos.x + orbit_radius * cos(rotation_angle_silentaim), mousepos.y + orbit_radius * sin(rotation_angle_silentaim));
                            if (globals::combat::silentaimfovfill && silent_aim_fov_fill_color != 0) {
                                drawbglist->AddCircleFilled(orbiting_center, globals::combat::silentaimfovsize, silent_aim_fov_fill_color);
                            }
                            drawbglist->AddCircle(orbiting_center, globals::combat::silentaimfovsize, silent_aim_fov_color_with_transparency, globals::combat::silentaimfovthickness);
                        } else {
                            if (globals::combat::silentaimfovfill && silent_aim_fov_fill_color != 0) {
                                drawbglist->AddCircleFilled(mousepos, globals::combat::silentaimfovsize, silent_aim_fov_fill_color);
                            }
                            drawbglist->AddCircle(mousepos, globals::combat::silentaimfovsize, silent_aim_fov_color_with_transparency, globals::combat::silentaimfovthickness);
                        }
                        break;
                    case 1: // Square
                        AddRotatedNgon(drawbglist, mousepos, globals::combat::silentaimfovsize, silent_aim_fov_color_with_transparency, 4, globals::combat::silentaimfovthickness, globals::combat::spin_fov_silentaim ? rotation_angle_silentaim + IM_PI / 4.0f : IM_PI / 4.0f, globals::combat::silentaimfovfill, silent_aim_fov_fill_color);
                        break;
                    case 2: // Triangle
                        AddRotatedNgon(drawbglist, mousepos, globals::combat::silentaimfovsize, silent_aim_fov_color_with_transparency, 3, globals::combat::silentaimfovthickness, globals::combat::spin_fov_silentaim ? rotation_angle_silentaim - IM_PI / 2.0f : -IM_PI / 2.0f, globals::combat::silentaimfovfill, silent_aim_fov_fill_color);
                        break;
                    case 3: // Pentagon
                        AddRotatedNgon(drawbglist, mousepos, globals::combat::silentaimfovsize, silent_aim_fov_color_with_transparency, 5, globals::combat::silentaimfovthickness, globals::combat::spin_fov_silentaim ? rotation_angle_silentaim - IM_PI / 2.0f : -IM_PI / 2.0f, globals::combat::silentaimfovfill, silent_aim_fov_fill_color);
                        break;
                    case 4: // Hexagon
                        AddRotatedNgon(drawbglist, mousepos, globals::combat::silentaimfovsize, silent_aim_fov_color_with_transparency, 6, globals::combat::silentaimfovthickness, globals::combat::spin_fov_silentaim ? rotation_angle_silentaim : 0.0f, globals::combat::silentaimfovfill, silent_aim_fov_fill_color);
                        break;
                    case 5: // Octagon
                        AddRotatedNgon(drawbglist, mousepos, globals::combat::silentaimfovsize, silent_aim_fov_color_with_transparency, 8, globals::combat::silentaimfovthickness, globals::combat::spin_fov_silentaim ? rotation_angle_silentaim : 0.0f, globals::combat::silentaimfovfill, silent_aim_fov_fill_color);
                        break;
                    }
                }
            }
            // Use the exact key from menu_hotkey for toggling, including specific modifier keys
            if ((GetAsyncKeyState(globals::misc::menu_hotkey.key) & 1))
            {
                ToggleOverlayVisibility();
            }
            if (overlay::visible)
            {
                // The native window should remain static, so no need to get its position here.
                ImGui::SetNextWindowPos(ImVec2(GetSystemMetrics(SM_CXSCREEN) / 2 - (745 / 2), GetSystemMetrics(SM_CYSCREEN) / 2 - (545 / 2)), ImGuiCond_Once);
                ImGui::SetNextWindowSize(ImVec2(745, 570), ImGuiCond_Once); // Slightly taller initial size

                // The main ImGui window should always have its own background, so no SetNextWindowBgAlpha here.
                // The ImGuiCol_WindowBg is already set to a dark grey in the style setup.
                ImGui::Begin("Number (N)ine", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize); // Added NoResize flag
                ImDrawList* draw = ImGui::GetWindowDrawList();
                ImVec2 window_pos = ImGui::GetWindowPos();
                float window_height = ImGui::GetWindowHeight();
                float window_width = ImGui::GetWindowWidth();
                ImVec2 window_size = ImVec2(window_width, window_height); // Use actual current window size
                ImU32 accent_color = ImGui::ColorConvertFloat4ToU32(ImVec4(globals::misc::accent_color[0], globals::misc::accent_color[1], globals::misc::accent_color[2], globals::misc::accent_color[3]));

                // Draw glow effect around the main window if enabled
                if (globals::misc::menuglow) {
                    ImGui::GetBackgroundDrawList()->AddShadowRect(window_pos, ImVec2(window_pos.x + window_width, window_pos.y + window_height), ImGui::ColorConvertFloat4ToU32(ImVec4(globals::misc::menuglowcolor[0], globals::misc::menuglowcolor[1], globals::misc::menuglowcolor[2], globals::misc::menuglowcolor[3])), 35, ImVec2(0, 0), ImDrawFlags_ShadowCutOutShapeBackground, 0.0f);
                }

                // Adjust playerlist offset to bring it closer to the main overlay
                float playerlist_offset_x = -730.0f; // Adjusted offset for new main window width (1000 - 300 (playerlist width) - some padding)
                render_player_list(ImVec2(window_pos.x + window_size.x + playerlist_offset_x, window_pos.y), window_size);

                render_theme_window();

                static int iTabID = 0; // Keep this declaration
                const char* tabs[] = { "Aimbot", "Silent", "Visuals", "Movement", "Settings" }; // Removed "World" tab
                float tab_bar_height = 30.0f;
                float available_tab_width = window_width - style.WindowPadding.x * 2;
                int tab_count = IM_ARRAYSIZE(tabs);
                float uniform_tab_width = available_tab_width / tab_count;

                // Calculate the Y position for the title and tabs, considering the top border and window padding
                float header_content_y = style.WindowPadding.y;

                // Align "frizy.lol" title to the left
                ImGui::SetCursorPos(ImVec2(style.WindowPadding.x, header_content_y));
                ImAdd::Text(ImGui::ColorConvertU32ToFloat4(ImGui::GetColorU32(ImGuiCol_Text)), "frizy");
                ImGui::SameLine(0, 0); // No spacing between "frizy" and ".lol"
                ImAdd::Text(ImVec4(globals::misc::accent_color[0], globals::misc::accent_color[1], globals::misc::accent_color[2], 1.0f), ".fvck");

                // Calculate total width of tabs
                float tabs_total_width = 0;
                for (int i = 0; i < tab_count; i++) {
                    tabs_total_width += ImGui::CalcTextSize(tabs[i]).x + style.FramePadding.x * 2; // Add padding for each tab
                }
                tabs_total_width += (tab_count - 1) * style.ItemSpacing.x; // Add spacing between tabs

                // Position tabs to the right, on the same row as the title
                ImGui::SetCursorPos(ImVec2(window_width - tabs_total_width - style.WindowPadding.x, header_content_y));
                ImGui::BeginGroup();
                {
                    for (int i = 0; i < tab_count; i++) {
                        ImGui::PushID(i);
                        ImVec2 label_size = ImGui::CalcTextSize(tabs[i]);
                        ImVec2 tab_button_size = ImVec2(label_size.x + style.FramePadding.x * 2, ImGui::GetTextLineHeight() + style.FramePadding.y * 2);

                        ImVec2 tab_min = ImGui::GetCursorScreenPos();
                        ImVec2 tab_max = ImVec2(tab_min.x + tab_button_size.x, tab_min.y + tab_button_size.y);

                        bool hovered = ImGui::IsMouseHoveringRect(tab_min, tab_max);
                        bool clicked = ImGui::InvisibleButton(tabs[i], tab_button_size);

                        if (clicked) {
                            iTabID = i;
                        }

                        // Only change text color on hover/selection, no background fill
                        ImU32 text_color = ImGui::GetColorU32(ImGuiCol_Text); // Default text color
                        if (iTabID == i) {
                            text_color = ImGui::GetColorU32(ImGuiCol_SliderGrab); // Selected tab color
                        } else if (hovered) {
                            text_color = ImGui::GetColorU32(ImGuiCol_TabHovered); // Hovered tab color (using TabHovered for text color)
                        }
                        ImVec2 text_pos = ImVec2(tab_min.x + style.FramePadding.x, tab_min.y + style.FramePadding.y);
                        draw->AddText(text_pos, text_color, tabs[i]);

                        if (i < tab_count - 1) {
                            ImGui::SameLine();
                        }
                        ImGui::PopID();
                    }
                }
                ImGui::EndGroup();

                // Calculate the Y position for the horizontal line below the title and tabs
                // This should be based on the maximum Y position occupied by the title or tabs, plus additional spacing
                float max_header_element_height = ImMax(ImGui::CalcTextSize("frizy").y, ImGui::GetTextLineHeight() + style.FramePadding.y * 2);
                float y_after_header = header_content_y + max_header_element_height + 10.0f; // Increased spacing above the line for hover effect

                // Draw the horizontal line below the title and tabs
                draw->AddLine(ImVec2(window_pos.x, window_pos.y + y_after_header), ImVec2(window_pos.x + window_size.x, window_pos.y + y_after_header), ImGui::GetColorU32(ImGuiCol_SliderGrab), 1.0f);

                // Adjust Y position for the main content area
                ImGui::SetCursorPosY(y_after_header + style.WindowPadding.y + 5.0f); // Reduced padding below the line
                // Content bounds for inner layout
                ImVec2 content_frame_min = ImGui::GetCursorScreenPos() + ImVec2(1.0f, 1.0f);
                ImVec2 content_frame_max = ImVec2(window_pos.x + window_size.x - style.WindowPadding.x - 1.0f, window_pos.y + window_size.y - style.WindowPadding.y - 1.0f);

                // Position content inside the frame with padding and balanced columns
                ImVec2 frame_padding = ImVec2(3.0f, 3.0f);
                ImVec2 content_start = content_frame_min + frame_padding;
                ImVec2 content_end = content_frame_max - frame_padding;
                ImVec2 content_size = ImVec2(content_end.x - content_start.x, content_end.y - content_start.y);

                ImGui::SetCursorScreenPos(content_start);
                ImGui::BeginChild("MainContentFrame", content_size, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                float frame_width = content_size.x;
                float column_spacing = style.ItemSpacing.x; // tighter to pull columns toward the frame
                float column_width = (frame_width - column_spacing) / 2.0f;

                ImGui::Columns(2, "##tab_columns", false);
                ImGui::SetColumnWidth(0, column_width);
                ImGui::SetColumnWidth(1, column_width);

                if (iTabID == 0) { // Aimbot Tab
                    // Left Column: Aimbot
                    ImGui::BeginChild("Aimbot", ImVec2(ImGui::GetContentRegionAvail().x, 495.0f), true); // Slightly smaller fixed height
                    {
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, 2.0f)); // tighter vertical spacing
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, style.FramePadding.y * 0.8f));
                        float aimbot_full_width = ImGui::GetContentRegionAvail().x;
                        ImGui::PushItemWidth(aimbot_full_width - 18.0f);
                        ImVec4 accent = ImVec4(globals::misc::accent_color[0], globals::misc::accent_color[1], globals::misc::accent_color[2], 1.0f);
                        ImGui::PushStyleColor(ImGuiCol_SliderGrab, accent);
                        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, accent);
                        draw_shadowed_text("Aimbot");
                        // Reduced spacing above the line
                        ImDrawList* draw = ImGui::GetWindowDrawList();
                        ImVec2 window_pos = ImGui::GetWindowPos();
                        ImVec2 content_start = ImGui::GetCursorScreenPos();
                        draw->AddLine(ImVec2(content_start.x, content_start.y), ImVec2(content_start.x + ImGui::GetContentRegionAvail().x, content_start.y), ImGui::GetColorU32(ImGuiCol_SliderGrab), 1.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f); // add breathing room below header
                        ImAdd::CheckBox("Aimbot", &globals::combat::aimbot); ImGui::SameLine();
                        Bind(&globals::combat::aimbotkeybind, ImVec2(0,0), false, ImGuiButtonFlags_AlignTextBaseLine);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                        ImAdd::CheckBox("Sticky Aim", &globals::combat::stickyaim);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                        ImAdd::Combo("Type", &globals::combat::aimbottype, "Camera\0Mouse\0");
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
                        ImAdd::MultiCombo("Hit Part", globals::combat::aimpart, std::vector<const char*>(globals::combat::hit_parts, globals::combat::hit_parts + IM_ARRAYSIZE(globals::combat::hit_parts)));
                        ImAdd::MultiCombo("Air Part", globals::combat::airaimpart, std::vector<const char*>(globals::combat::hit_parts, globals::combat::hit_parts + IM_ARRAYSIZE(globals::combat::hit_parts)));
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f); 
                        ImAdd::CheckBox("Smoothing", &globals::combat::smoothing);
                        ImAdd::SliderFloat("Smoothing X", &globals::combat::smoothingx, 1, 100, "%.0f");
                        ImAdd::SliderFloat("Smoothing Y", &globals::combat::smoothingy, 1, 100, "%.0f");
                        ImAdd::Combo("Smoothing Style", &globals::combat::smoothing_style, globals::combat::smoothing_styles, IM_ARRAYSIZE(globals::combat::smoothing_styles));
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f); 
                        ImAdd::CheckBox("Predictions", &globals::combat::predictions);
                        ImAdd::SliderFloat("Predictions X", &globals::combat::predictionsx, 1, 25, "%.0f");
                        ImAdd::SliderFloat("Predictions Y", &globals::combat::predictionsy, 1, 25, "%.0f");
                        ImGui::PopStyleColor(2);
                        ImGui::PopItemWidth();
                        ImGui::PopStyleVar(2);
                    }
                    ImGui::EndChild();

                    ImGui::NextColumn();

                    // Right Column: Controls
                    ImGui::BeginChild("Controls", ImVec2(ImGui::GetContentRegionAvail().x, 495.0f), true); // Slightly smaller fixed height
                    {
                        draw_shadowed_text("Controls");
                        // Reduced spacing above the line
                        ImDrawList* draw = ImGui::GetWindowDrawList();
                        ImVec2 window_pos = ImGui::GetWindowPos();
                        ImVec2 content_start = ImGui::GetCursorScreenPos();
                        draw->AddLine(ImVec2(content_start.x, content_start.y), ImVec2(content_start.x + ImGui::GetContentRegionAvail().x, content_start.y), ImGui::GetColorU32(ImGuiCol_SliderGrab), 1.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y + 5.0f); // Increased spacing below the line
                        float controls_full_width = ImGui::GetContentRegionAvail().x;
                        ImGui::PushItemWidth(controls_full_width - 18.0f);
                        ImVec4 accent = ImVec4(globals::misc::accent_color[0], globals::misc::accent_color[1], globals::misc::accent_color[2], 1.0f);
                        ImGui::PushStyleColor(ImGuiCol_SliderGrab, accent);
                        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, accent);
                        ImGui::BeginGroup();
                        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 40); // Leave space for color picker
                        ImAdd::CheckBox("Use FOV", &globals::combat::usefov);
                        ImGui::PopItemWidth();
                        if (globals::combat::usefov) {
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30);
                            ColorPickerWithHex("##AimbotFOVColor", globals::combat::fovcolor);
                        }
                        ImGui::EndGroup();

                        ImGui::BeginGroup();
                        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 40);
                        ImAdd::CheckBox("Fill FOV", &globals::combat::fovfill);
                        ImGui::PopItemWidth();
                        if (globals::combat::fovfill) {
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30);
                            ColorPickerWithHex("##AimbotFOVFillColor", globals::combat::fovfillcolor);
                        }
                        ImGui::EndGroup();

                        ImGui::PushID("AimbotFOVControls");
                        ImAdd::CheckBox("Spin FOV", &globals::combat::spin_fov_aimbot);
                        ImGui::Spacing();
                        ImAdd::SliderFloat("FOV Radius", &globals::combat::fovsize, 10, 1000, "%.0f");
                        float aimbot_fov_step = ImClamp(globals::combat::fovtransparency, 0.0f, 1.0f) * 3.0f;
                        float aimbot_fill_step = ImClamp(globals::combat::fovfilltransparency, 0.0f, 1.0f) * 3.0f;
                        if (ImAdd::SliderFloat("FOV Transparency", &aimbot_fov_step, 0.0f, 3.0f, "%.0f")) {
                            aimbot_fov_step = std::round(aimbot_fov_step);
                            globals::combat::fovtransparency = ImClamp(aimbot_fov_step / 3.0f, 0.0f, 1.0f);
                        }
                        if (ImAdd::SliderFloat("Fill Transparency", &aimbot_fill_step, 0.0f, 3.0f, "%.0f")) {
                            aimbot_fill_step = std::round(aimbot_fill_step);
                            globals::combat::fovfilltransparency = ImClamp(aimbot_fill_step / 3.0f, 0.0f, 1.0f);
                        }
                        ImAdd::SliderFloat("Spin Speed", &globals::combat::spin_fov_aimbot_speed, 1.0f, 5.0f, "%.0f");
                        ImGui::Spacing(); // Added spacing for better separation

                        if (globals::combat::flags->size() < 4) {
                            globals::combat::flags->resize(4, 0);
                        }
                        ImAdd::CheckBox("Team Check", (bool*)&(*globals::combat::flags)[0]);
                        ImGui::Spacing();
                        ImAdd::CheckBox("Knocked Check", (bool*)&(*globals::combat::flags)[1]);
                        ImGui::Spacing();
                        ImAdd::CheckBox("Range Check", (bool*)&(*globals::combat::flags)[2]);
                        ImGui::Spacing();
                        ImAdd::CheckBox("Wall Check", (bool*)&(*globals::combat::flags)[3]);
                        ImGui::Spacing();
                        ImAdd::CheckBox("Triggerbot", &globals::combat::triggerbot); ImGui::SameLine();
                        Bind(&globals::combat::triggerbotkeybind, ImVec2(0,0), false, ImGuiButtonFlags_AlignTextBaseLine);
                        if (globals::combat::triggerbot) {
                            ImAdd::SliderFloat("Delay (ms)", &globals::combat::triggerbot_delay, 0, 1000, "%.0f");
                        }
                        if (globals::combat::rangecheck) {
                        ImAdd::SliderFloat("Aim Distance", &globals::combat::aim_distance, 10, 5000, "%.0fm");
                    }
                    const char* fovShapes[] = { "Circle", "Square", "Triangle", "Pentagon", "Hexagon", "Octagon" };
                    ImAdd::Combo("Shape", &globals::combat::fovshape, fovShapes, IM_ARRAYSIZE(fovShapes));
                    if (globals::combat::triggerbot) {
                        static const std::vector<const char*> triggerbot_options = { "Spray", "Wallet", "Food", "Knife" };
                        std::vector<int> triggerbot_selection(triggerbot_options.size(), 0);
                        for (size_t i = 0; i < triggerbot_options.size(); ++i) {
                            triggerbot_selection[i] = globals::combat::triggerbot_item_checks[i] ? 1 : 0;
                        }

                        bool triggerbot_checks_changed = false;
                        ImAdd::MultiCombo("Triggerbot Checks", &triggerbot_selection, triggerbot_options, -0.1f, &triggerbot_checks_changed);

                        if (triggerbot_checks_changed) {
                            for (size_t i = 0; i < triggerbot_selection.size(); ++i) {
                                globals::combat::triggerbot_item_checks[i] = triggerbot_selection[i] == 1;
                            }
                        }
                    }
                    ImGui::PopID();
                    ImGui::Spacing();
                    ImGui::PopStyleColor(2);
                    ImGui::PopItemWidth();
                }
                    ImGui::EndChild();
                }
                else if (iTabID == 1) { // Silent Tab
                    ImGui::Columns(2, "##silent_columns", false);
                    ImGui::SetColumnWidth(0, column_width);
                    ImGui::SetColumnWidth(1, column_width);

                    // Left Column: Silent
                    ImGui::BeginChild("Silent", ImVec2(ImGui::GetContentRegionAvail().x, 495.0f), true);
                    {
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, 2.0f));
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, style.FramePadding.y * 0.8f));
                        float silent_full_width = ImGui::GetContentRegionAvail().x;
                        ImGui::PushItemWidth(silent_full_width - 18.0f);
                        ImVec4 accent = ImVec4(globals::misc::accent_color[0], globals::misc::accent_color[1], globals::misc::accent_color[2], 1.0f);
                        ImGui::PushStyleColor(ImGuiCol_SliderGrab, accent);
                        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, accent);
                        draw_shadowed_text("Silent");
                        ImDrawList* draw = ImGui::GetWindowDrawList();
                        ImVec2 content_start = ImGui::GetCursorScreenPos();
                        draw->AddLine(ImVec2(content_start.x, content_start.y), ImVec2(content_start.x + ImGui::GetContentRegionAvail().x, content_start.y), ImGui::GetColorU32(ImGuiCol_SliderGrab), 1.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f); // match Aimbot spacing

                        ImAdd::CheckBox("Silent", &globals::combat::silentaim); ImGui::SameLine();
                        Bind(&globals::combat::silentaimkeybind, ImVec2(0,0), false, ImGuiButtonFlags_AlignTextBaseLine);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                        ImAdd::CheckBox("Sticky Aim  ", &globals::combat::stickyaimsilent);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                        ImAdd::Combo("Type", &globals::combat::silentaimtype, "FreeAim\0Mouse\0");
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f); // match Aimbot dropdown spacing
                        bool silent_aim_hit_part_changed = false;
                        bool any_hit_part_selected = false;
                        ImAdd::MultiCombo("Hit Part ", globals::combat::silentaimpart, std::vector<const char*>(globals::combat::hit_parts, globals::combat::hit_parts + IM_ARRAYSIZE(globals::combat::hit_parts)), -0.1f, &silent_aim_hit_part_changed);
                        ImAdd::MultiCombo("Air Part", globals::combat::airsilentaimpart, std::vector<const char*>(globals::combat::hit_parts, globals::combat::hit_parts + IM_ARRAYSIZE(globals::combat::hit_parts)));
                        if (silent_aim_hit_part_changed) {
                            for (int part_state : *globals::combat::silentaimpart) {
                                if (part_state == 1) {
                                    any_hit_part_selected = true;
                                    break;
                                }
                            }
                        }
                        if (any_hit_part_selected) {
                            globals::combat::silentaim = true;
                        }
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
                        ImAdd::CheckBox("Predictions", &globals::combat::silentpredictions);
                        ImAdd::SliderFloat("Predictions X", &globals::combat::silentpredictionsx, 1, 25, "%.0f");
                        ImAdd::SliderFloat("Predictions Y", &globals::combat::silentpredictionsy, 1, 25, "%.0f");
                        ImGui::PopStyleColor(2);
                        ImGui::PopItemWidth();
                        ImGui::PopStyleVar(2);
                    }
                    ImGui::EndChild();

                    ImGui::NextColumn();

                    // Right Column: Controls
                    ImGui::BeginChild("Controls", ImVec2(ImGui::GetContentRegionAvail().x, 495.0f), true);
                    {
                        draw_shadowed_text("Controls");
                        ImDrawList* draw = ImGui::GetWindowDrawList();
                        ImVec2 content_start = ImGui::GetCursorScreenPos();
                        draw->AddLine(ImVec2(content_start.x, content_start.y), ImVec2(content_start.x + ImGui::GetContentRegionAvail().x, content_start.y), ImGui::GetColorU32(ImGuiCol_SliderGrab), 1.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y + 5.0f);
                        float silent_controls_width = ImGui::GetContentRegionAvail().x;
                        ImGui::PushItemWidth(silent_controls_width - 18.0f);
                        ImVec4 accent = ImVec4(globals::misc::accent_color[0], globals::misc::accent_color[1], globals::misc::accent_color[2], 1.0f);
                        ImGui::PushStyleColor(ImGuiCol_SliderGrab, accent);
                        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, accent);

                        ImGui::BeginGroup();
                        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 40); // Leave space for color picker
                        ImAdd::CheckBox("Use Silent FOV", &globals::combat::silentaimfov);
                        ImGui::PopItemWidth();
                        if (globals::combat::silentaimfov) {
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30);
                            ColorPickerWithHex("##SilentFOVColor", globals::combat::silentaimfovcolor);
                        }
                        ImGui::EndGroup();

                        ImGui::BeginGroup();
                        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 40);
                        ImAdd::CheckBox("Fill Silent FOV", &globals::combat::silentaimfovfill);
                        ImGui::PopItemWidth();
                        if (globals::combat::silentaimfovfill) {
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30);
                            ColorPickerWithHex("##SilentFOVFillColor", globals::combat::silentaimfovfillcolor);
                        }
                        ImGui::EndGroup();

                        ImGui::PushID("SilentFOVControls");
                        ImAdd::CheckBox("Spin Silent FOV", &globals::combat::spin_fov_silentaim);
                        ImGui::Spacing();
                        ImAdd::SliderFloat("FOV Radius", &globals::combat::silentaimfovsize, 10, 1000, "%.0f");
                        float silent_fov_step = ImClamp(globals::combat::silentaimfovtransparency, 0.0f, 1.0f) * 3.0f;
                        float silent_fill_step = ImClamp(globals::combat::silentaimfovfilltransparency, 0.0f, 1.0f) * 3.0f;
                        if (ImAdd::SliderFloat("FOV Transparency", &silent_fov_step, 0.0f, 3.0f, "%.0f")) {
                            silent_fov_step = std::round(silent_fov_step);
                            globals::combat::silentaimfovtransparency = ImClamp(silent_fov_step / 3.0f, 0.0f, 1.0f);
                        }
                        if (ImAdd::SliderFloat("Fill Transparency", &silent_fill_step, 0.0f, 3.0f, "%.0f")) {
                            silent_fill_step = std::round(silent_fill_step);
                            globals::combat::silentaimfovfilltransparency = ImClamp(silent_fill_step / 3.0f, 0.0f, 1.0f);
                        }
                        ImAdd::SliderFloat("Spin Speed", &globals::combat::spin_fov_silentaim_speed, 1.0f, 5.0f, "%.0f");
                        ImGui::Spacing(); // Added spacing for better separation

                        if (globals::combat::flags->size() < 4) {
                            globals::combat::flags->resize(4, 0);
                        }
                        ImAdd::CheckBox("Team Check", (bool*)&(*globals::combat::flags)[0]);
                        ImGui::Spacing();
                        ImAdd::CheckBox("Knocked Check", (bool*)&(*globals::combat::flags)[1]);
                        ImGui::Spacing();
                        ImAdd::CheckBox("Range Check", (bool*)&(*globals::combat::flags)[2]);
                        ImGui::Spacing();
                        ImAdd::CheckBox("Wall Check", (bool*)&(*globals::combat::flags)[3]);
                        if (globals::combat::rangecheck) {
                            ImAdd::SliderFloat("Aim Distance", &globals::combat::aim_distance, 10, 5000, "%.0fm");
                        }
                        const char* silentFovShapes[] = { "Circle", "Square", "Triangle", "Pentagon", "Hexagon", "Octagon" };
                        ImAdd::Combo("Shape", &globals::combat::silentaimfovshape, silentFovShapes, IM_ARRAYSIZE(silentFovShapes));
                        ImGui::PopID();
                        ImGui::PopStyleColor(2);
                        ImGui::PopItemWidth();
                    }
                    ImGui::EndChild();
                    ImGui::Columns(1);
                }
                else if (iTabID == 2) { // Visuals tab
                    ImGui::Columns(2, "##visuals_columns", false);
                    ImGui::SetColumnWidth(0, column_width);
                    ImGui::SetColumnWidth(1, column_width);

                    // Left Column: Main Visuals (ESP features)
                    ImGui::BeginChild("VisualsMain", ImVec2(ImGui::GetContentRegionAvail().x, 485.0f), true);
                    {
                        draw_shadowed_text("Visuals");
                        ImDrawList* draw = ImGui::GetWindowDrawList();
                        ImVec2 content_start = ImGui::GetCursorScreenPos();
                        draw->AddLine(ImVec2(content_start.x, content_start.y), ImVec2(content_start.x + ImGui::GetContentRegionAvail().x, content_start.y), ImGui::GetColorU32(ImGuiCol_SliderGrab), 1.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y + 5.0f);

                        struct VisualsToggleCache {
                            bool boxes = false;
                            bool healthbar = false;
                            bool name = false;
                            bool toolesp = false;
                            bool skeletons = false;
                            bool snapline = false;
                            bool distance = false;
                            bool chams = false;
                            bool fog_changer = false;
                            bool target_only_esp = false;
                            bool lockedesp = false;
                            bool sonar = false;
                            bool has_backup = false;
                        };
                        static VisualsToggleCache visuals_backup{};

                        bool visuals_was_enabled = globals::visuals::visuals;
                        ImAdd::CheckBox(globals::visuals::visuals ? "Disable Visuals" : "Enable Visuals", &globals::visuals::visuals);

                        if (visuals_was_enabled && !globals::visuals::visuals) {
                            visuals_backup.boxes = globals::visuals::boxes;
                            visuals_backup.healthbar = globals::visuals::healthbar;
                            visuals_backup.name = globals::visuals::name;
                            visuals_backup.toolesp = globals::visuals::toolesp;
                            visuals_backup.skeletons = globals::visuals::skeletons;
                            visuals_backup.snapline = globals::visuals::snapline;
                            visuals_backup.distance = globals::visuals::distance;
                            visuals_backup.chams = globals::visuals::chams;
                            visuals_backup.fog_changer = globals::visuals::fog_changer;
                            visuals_backup.target_only_esp = globals::visuals::target_only_esp;
                            visuals_backup.lockedesp = globals::visuals::lockedesp;
                            visuals_backup.sonar = globals::visuals::sonar;
                            visuals_backup.has_backup = true;

                            globals::visuals::boxes = false;
                            globals::visuals::healthbar = false;
                            globals::visuals::name = false;
                            globals::visuals::toolesp = false;
                            globals::visuals::skeletons = false;
                            globals::visuals::snapline = false;
                            globals::visuals::distance = false;
                            globals::visuals::chams = false;
                            globals::visuals::fog_changer = false;
                            globals::visuals::target_only_esp = false;
                            globals::visuals::lockedesp = false;
                            globals::visuals::sonar = false;
                        }
                        else if (!visuals_was_enabled && globals::visuals::visuals && visuals_backup.has_backup) {
                            globals::visuals::boxes = visuals_backup.boxes;
                            globals::visuals::healthbar = visuals_backup.healthbar;
                            globals::visuals::name = visuals_backup.name;
                            globals::visuals::toolesp = visuals_backup.toolesp;
                            globals::visuals::skeletons = visuals_backup.skeletons;
                            globals::visuals::snapline = visuals_backup.snapline;
                            globals::visuals::distance = visuals_backup.distance;
                            globals::visuals::chams = visuals_backup.chams;
                            globals::visuals::fog_changer = visuals_backup.fog_changer;
                            globals::visuals::target_only_esp = visuals_backup.target_only_esp;
                            globals::visuals::lockedesp = visuals_backup.lockedesp;
                            globals::visuals::sonar = visuals_backup.sonar;
                        }
                        ImGui::Spacing(); // Added spacing for better separation
                        ImAdd::CheckBox("Boxes", &globals::visuals::boxes); ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
                        ImGui::PushItemWidth(120.0f);
                        ColorPickerWithHex("##BoxColor", globals::visuals::boxcolors);
                        ImGui::PopItemWidth();
                        if (globals::visuals::boxes) {
                            ImAdd::Combo("Box Type", &globals::visuals::boxtype, "Bounding\0Corners\0");
                            std::vector<const char*> box_overlays = { "Outline", "Glow", "Fill" };
                            // Keep storage in sync with the options
                            if (globals::visuals::box_overlay_flags->size() < box_overlays.size()) {
                                globals::visuals::box_overlay_flags->resize(box_overlays.size(), 0);
                            }

                            ImAdd::MultiCombo("Box Overlays", globals::visuals::box_overlay_flags, box_overlays);
                            ImGui::SameLine();
                            if (ImAdd::Button("None", ImVec2(50.0f, 0))) {
                                for (int i = 0; i < globals::visuals::box_overlay_flags->size(); ++i) {
                                    (*globals::visuals::box_overlay_flags)[i] = 0;
                                }
                            }

                            // Show color pickers only for selected overlays
                            if ((*globals::visuals::box_overlay_flags)[2]) { // Fill
                                ImGui::Spacing();
                                ImGui::BeginGroup();
                                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 40);
                                ImGui::Text("Fill Color");
                                ImGui::PopItemWidth();
                                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 25);
                                ColorPickerWithHex("##FillColor", globals::visuals::boxfillcolor);
                                ImGui::EndGroup();
                            }

                            if ((*globals::visuals::box_overlay_flags)[1]) { // Glow
                                ImGui::Spacing();
                                ImGui::BeginGroup();
                                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 40);
                                ImGui::Text("Glow Color");
                                ImGui::PopItemWidth();
                                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 25);
                                ColorPickerWithHex("##GlowColor", globals::visuals::glowcolor);
                                ImGui::EndGroup();
                            }
                        }
                        ImGui::Spacing(); // Added spacing for better separation
                        ImGui::BeginGroup();
                        ImAdd::CheckBox("Health Bar", &globals::visuals::healthbar);
                        ImGui::EndGroup();
                        
                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 48.0f); // Position for color pickers
                        
                        // First color picker
                        ImGui::PushID("health1");
                        ColorPickerWithHex("##healthcolor1", globals::visuals::healthbarcolor);
                        ImGui::PopID();
                        
                        ImGui::SameLine();
                        
                        // Second color picker
                        ImGui::PushID("health2");
                        ColorPickerWithHex("##healthcolor2", globals::visuals::healthbarcolor1);
                        ImGui::PopID();
                        if (globals::visuals::healthbar) {
                            std::vector<const char*> health_overlays = { "Outline", "Gradient" };

                            // Keep storage in sync with the options
                            if (globals::visuals::healthbar_overlay_flags->size() < health_overlays.size()) {
                                globals::visuals::healthbar_overlay_flags->resize(health_overlays.size(), 0);
                            } else if (globals::visuals::healthbar_overlay_flags->size() > health_overlays.size()) {
                                globals::visuals::healthbar_overlay_flags->resize(health_overlays.size());
                            }

                            ImAdd::MultiCombo("Health Overlays", globals::visuals::healthbar_overlay_flags, health_overlays);
                            ImGui::SameLine();
                            if (ImAdd::Button("None", ImVec2(50.0f, 0))) {
                                for (int i = 0; i < globals::visuals::healthbar_overlay_flags->size(); ++i) {
                                    (*globals::visuals::healthbar_overlay_flags)[i] = 0;
                                }
                            }
                        }
                        ImGui::Spacing(); // Added spacing for better separation
                        ImAdd::CheckBox("Name ESP", &globals::visuals::name); ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
                        ImGui::PushItemWidth(120.0f);
                        ColorPickerWithHex("##NameColor", globals::visuals::namecolor);
                        ImGui::PopItemWidth();
                        if (globals::visuals::name) {
                            ImAdd::Combo("Name Type", &globals::visuals::nametype, "Username\0Display Name\0");
                        }
                        ImGui::Spacing(); // Added spacing for better separation
                        ImAdd::CheckBox("Tool Name", &globals::visuals::toolesp); ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
                        ImGui::PushItemWidth(120.0f);
                        ColorPickerWithHex("##ToolColor", globals::visuals::toolespcolor);
                        ImGui::PopItemWidth();
                        ImGui::Spacing(); // Added spacing for better separation
                        ImAdd::CheckBox("Skeleton", &globals::visuals::skeletons); ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
                        ImGui::PushItemWidth(120.0f);
                        ColorPickerWithHex("##SkeletonColor", globals::visuals::skeletonscolor);
                        ImGui::PopItemWidth();
                        ImGui::Spacing(); // Added spacing for better separation
                        ImAdd::CheckBox("Chams", &globals::visuals::chams); ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
                        ImGui::PushItemWidth(120.0f);
                        ColorPickerWithHex("##ChamsColor", globals::visuals::chamscolor);
                        ImGui::PopItemWidth();
                        if (globals::visuals::chams) {
                            std::vector<const char*> chams_overlays = { "Outline", "Fill" };
                            if (globals::visuals::chams_overlay_flags->size() < chams_overlays.size()) {
                                globals::visuals::chams_overlay_flags->resize(chams_overlays.size(), 0);
                            } else if (globals::visuals::chams_overlay_flags->size() > chams_overlays.size()) {
                                globals::visuals::chams_overlay_flags->resize(chams_overlays.size());
                            }

                            ImAdd::MultiCombo("Chams Overlays", globals::visuals::chams_overlay_flags, chams_overlays);
                            ImGui::SameLine();
                            if (ImAdd::Button("None##ChamsOverlay", ImVec2(50.0f, 0))) {
                                for (int i = 0; i < globals::visuals::chams_overlay_flags->size(); ++i) {
                                    (*globals::visuals::chams_overlay_flags)[i] = 0;
                                }
                            }
                        }
                        ImGui::Spacing(); // Added spacing for better separation
                        ImAdd::CheckBox("Snaplines", &globals::visuals::snapline); ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
                        ImGui::PushItemWidth(120.0f);
                        ColorPickerWithHex("##SnaplineColor", globals::visuals::snaplinecolor);
                        ImGui::PopItemWidth();
                        if (globals::visuals::snapline) {
                            const char* snapline_types[] = { "None", "Top", "Bottom", "Center", "Crosshair" };
                            const int snapline_type_count = IM_ARRAYSIZE(snapline_types);
                            if (globals::visuals::snaplinetype < 0 || globals::visuals::snaplinetype >= snapline_type_count) {
                                globals::visuals::snaplinetype = 0; // Default to "None" if out of range
                            }
                            ImAdd::Combo("Snapline Type", &globals::visuals::snaplinetype, snapline_types, snapline_type_count);
                            const char* snapline_overlay_types[] = { "Straight", "Spiderweb" };
                            ImAdd::Combo("Snapline Overlays", &globals::visuals::snaplineoverlaytype, snapline_overlay_types, IM_ARRAYSIZE(snapline_overlay_types));
                        }
                        ImGui::Spacing(); // Added spacing for better separation
                        ImAdd::CheckBox("Distance", &globals::visuals::distance); ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
                        ImGui::PushItemWidth(120.0f);
                        ColorPickerWithHex("##DistanceColor", globals::visuals::distancecolor);
                        ImGui::PopItemWidth();
                        if (globals::visuals::distance) {
                            ImAdd::SliderFloat("Distance", &globals::visuals::visual_range, 50, 5000, "%.0f"); // Increased max slider value
                        }
                        // Targets will still be managed internally via the PlayerList.
                    }
                    ImGui::EndChild();

                    ImGui::NextColumn();

                    // Right Column: Other Settings (combining Fog Changer and previous "Other Settings")
                    ImGui::BeginChild("VisualsSide", ImVec2(ImGui::GetContentRegionAvail().x, 485.0f), true);
                    {
                        draw_shadowed_text("Other Settings");
                        ImDrawList* draw = ImGui::GetWindowDrawList();
                        ImVec2 content_start = ImGui::GetCursorScreenPos();
                        draw->AddLine(ImVec2(content_start.x, content_start.y), ImVec2(content_start.x + ImGui::GetContentRegionAvail().x, content_start.y), ImGui::GetColorU32(ImGuiCol_SliderGrab), 1.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y + 5.0f);

                        // Fog Changer
                        ImAdd::CheckBox("Fog Changer", &globals::visuals::fog_changer);
                        if (globals::visuals::fog_changer) {
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
                            ImGui::PushItemWidth(120.0f); // Set a fixed width for the color picker
                            ColorPickerWithHex("##FogColor", globals::visuals::fog_color);
                            ImGui::PopItemWidth();
                            ImAdd::SliderFloat("Fog Start", &globals::visuals::fog_start, 0.0f, 500.0f, "%.0f");
                            ImAdd::SliderFloat("Fog End", &globals::visuals::fog_end, 0.0f, 500.0f, "%.0f");
                        }
                        ImGui::Spacing();

                        ImAdd::CheckBox("Sonar", &globals::visuals::sonar);
                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16.0f);
                        ImGui::PushItemWidth(120.0f);
                        ColorPickerWithHex("##SonarWaveColor", globals::visuals::sonarcolor);
                        ImGui::PopItemWidth();

                        if (globals::visuals::sonar) {
                            // Detect Players row styled like other feature rows
                            ImAdd::CheckBox("Detect Players", &globals::visuals::sonar_detect_players);
                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 48.0f);
                            ImGui::PushItemWidth(60.0f);
                            ColorPickerWithHex("##SonarDetectOut", globals::visuals::sonar_detect_color_out);
                            ImGui::PopItemWidth();
                            ImGui::SameLine();
                            ImGui::PushItemWidth(60.0f);
                            ColorPickerWithHex("##SonarDetectIn", globals::visuals::sonar_detect_color_in);
                            ImGui::PopItemWidth();
                            ImGui::PushItemWidth(220.0f);
                            ImAdd::SliderFloat("Radius", &globals::visuals::sonar_range, 10.0f, 100.0f, "%.0f");
                            ImAdd::SliderFloat("Speed", &globals::visuals::sonar_speed, 1.0f, 5.0f, "%.0f");
                            ImAdd::SliderFloat("Thickness", &globals::visuals::sonar_thickness, 1.0f, 5.0f, "%.0f");
                            ImGui::PopItemWidth();
                        }
                        ImGui::Spacing();

                        // Previous "Other Settings" content
                          ImAdd::CheckBox("Target Only ESP", &globals::visuals::target_only_esp);
                          ImGui::Spacing();
                          ImAdd::CheckBox("MM2 ESP", &globals::visuals::mm2esp);
                        ImGui::Spacing();
                        ImAdd::CheckBox("Locked ESP", &globals::visuals::lockedesp); ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16, 0);
                        ImGui::PushItemWidth(120.0f); // Set a fixed width for the color picker
                        ColorPickerWithHex("##LockedESPColor", globals::visuals::lockedespcolor);
                        ImGui::PopItemWidth();
                        ImGui::Spacing();
                    }
                    ImGui::EndChild();

                    ImGui::Columns(1);
                }
                else if (iTabID == 3) { // Movement tab (now index 3 instead of 4)
                    // Left Column: Movement
                    ImGui::BeginChild("Movement", ImVec2(ImGui::GetContentRegionAvail().x, 415), true);
                    {
                        draw_shadowed_text("Movement");
                        ImDrawList* draw = ImGui::GetWindowDrawList();
                        ImVec2 content_start = ImGui::GetCursorScreenPos();
                        draw->AddLine(ImVec2(content_start.x, content_start.y), ImVec2(content_start.x + ImGui::GetContentRegionAvail().x, content_start.y), ImGui::GetColorU32(ImGuiCol_SliderGrab), 1.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y + 5.0f);
                        
                        // Speed Section
                        ImAdd::CheckBox("Speed   ", &globals::misc::speed); ImGui::SameLine();
                        Bind(&globals::misc::speedkeybind, ImVec2(0,0), false, ImGuiButtonFlags_AlignTextBaseLine);
                        ImAdd::SliderFloat("Speed", &globals::misc::speedvalue, 1, 500, "%.0f");
                        ImAdd::Combo("Mode", &globals::misc::speedtype, "WalkSpeed\0Velocity\0");
                        ImGui::Spacing();
                        
                        // Flight Section
                        ImAdd::CheckBox("Flight", &globals::misc::flight); ImGui::SameLine();
                        Bind(&globals::misc::flightkeybind, ImVec2(0,0), false, ImGuiButtonFlags_AlignTextBaseLine);
                        ImAdd::SliderFloat("Speed         ", &globals::misc::flightvalue, 1, 750, "%.0f");
                        ImAdd::Combo("Mode   ", &globals::misc::flighttype, "Position\0Velocity\0");
                        ImGui::Spacing();
                        
                        // Jump Power Section
                        ImAdd::CheckBox("Jump Power", &globals::misc::jumppower); ImGui::SameLine();
                        Bind(&globals::misc::jumppowerkeybind, ImVec2(0,0), false, ImGuiButtonFlags_AlignTextBaseLine);
                        ImAdd::SliderFloat("Jump Power Value", &globals::misc::jumppowervalue, 1, 750, "%.0f"); // Changed max value to 750
                    }
                    ImGui::EndChild();

                    ImGui::NextColumn();

                    // Right Column: Controls
                    ImGui::BeginChild("Controls", ImVec2(ImGui::GetContentRegionAvail().x, 415), true);
                    {
                        draw_shadowed_text("Controls");
                        ImDrawList* draw = ImGui::GetWindowDrawList();
                        ImVec2 content_start = ImGui::GetCursorScreenPos();
                        draw->AddLine(ImVec2(content_start.x, content_start.y), ImVec2(content_start.x + ImGui::GetContentRegionAvail().x, content_start.y), ImGui::GetColorU32(ImGuiCol_SliderGrab), 1.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y + 5.0f);
                        
                        // Orbit
                        ImAdd::CheckBox("Orbit", &globals::combat::orbit); ImGui::SameLine();
                        Bind(&globals::combat::orbitkeybind, ImVec2(0,0), false, ImGuiButtonFlags_AlignTextBaseLine);
                        std::vector<const char*> stuff = { "X-Axis", "Y-Axis", "Random" };
                        if (globals::combat::orbittype.size() != stuff.size()) {
                            globals::combat::orbittype.resize(stuff.size(), 0);
                        }
                        ImAdd::MultiCombo("Orbit Axis", &globals::combat::orbittype, stuff);
                        ImAdd::SliderFloat("Speed                                 ", &globals::combat::orbitspeed, 0.1f, 24, "%.0f");
                        ImAdd::SliderFloat("Height      ", &globals::combat::orbitheight, -10, 25, "%.0f");
                        ImAdd::SliderFloat("Range       ", &globals::combat::orbitrange, 1, 25, "%.0f");
                        ImGui::Spacing();
                        
                        // No Jump Cooldown
                        ImAdd::CheckBox("No Jump Cooldown", &globals::misc::nojumpcooldown);
                        ImGui::Spacing();
                        
                        // 360 Camera
                        ImAdd::CheckBox("360 Camera", &globals::misc::rotate360); ImGui::SameLine();
                        Bind(&globals::misc::rotate360keybind, ImVec2(0,0), false, ImGuiButtonFlags_AlignTextBaseLine);
                        if (globals::misc::rotate360) {
                            ImAdd::SliderFloat("Rotation Speed", &globals::misc::rotate360_speed, 1.0f, 30.0f, "%.0f Speed");
                        }
                    }
                    ImGui::EndChild();

                    ImGui::Columns(1);
                }
                else if (iTabID == 4) { // Misc tab (now index 4 instead of 5)
                    // Left Column: Settings
                    ImGui::BeginChild("Settings", ImVec2(ImGui::GetContentRegionAvail().x, 475.0f), true);
                    {
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, 2.0f));
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, style.FramePadding.y * 0.8f));
                        draw_shadowed_text("Settings");
                        ImDrawList* draw = ImGui::GetWindowDrawList();
                        ImVec2 content_start = ImGui::GetCursorScreenPos();
                        draw->AddLine(ImVec2(content_start.x, content_start.y), ImVec2(content_start.x + ImGui::GetContentRegionAvail().x, content_start.y), ImGui::GetColorU32(ImGuiCol_SliderGrab), 1.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
                        
                        // UI Settings
                        ImAdd::CheckBox("Players", &globals::misc::playerlist);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                        ImAdd::CheckBox("Stream Mode", &globals::misc::streamproof);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                        ImAdd::CheckBox("Explorer", &globals::misc::explorer);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                        // Notifications toggle removed per request
                        ImAdd::CheckBox("Overlay Stars", &globals::misc::overlay_stars);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
                        ImAdd::CheckBox("Cursor", &globals::misc::custom_cursor);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
                        
                        // Auto-Friend Group Members - inline and compact
                        ImAdd::CheckBox("Auto Add Friends", &globals::misc::autofriend_enabled);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
                        
                        if (globals::misc::autofriend_enabled) {
                            // Group ID input and button on same line
                            ImGui::Text("Group ID:"); ImGui::SameLine();
                            static char group_id_buffer[32];
                            strcpy_s(group_id_buffer, globals::misc::autofriend_group_id.c_str());
                            ImGui::SetNextItemWidth(100);
                            if (ImGui::InputText("##group_id", group_id_buffer, sizeof(group_id_buffer))) {
                                globals::misc::autofriend_group_id = group_id_buffer;
                            }
                            
                            ImGui::SameLine();
                            if (ImAdd::Button("Load Group", ImVec2(100, 20))) {
                                if (!autofriend::g_auto_friend_manager.isLoading()) {
                                    globals::misc::loading_group_members = true;
                                    globals::misc::autofriend_status = "Loading...";
                                    autofriend::g_auto_friend_manager.fetchGroupMembers(globals::misc::autofriend_group_id);
                                }
                            }
                            
                            // Status and clear button on same line
                            if (autofriend::g_auto_friend_manager.isLoading()) {
                                ImGui::Text("Status: %s", globals::misc::autofriend_status.c_str());
                            } else if (globals::misc::group_members_loaded) {
                                ImGui::Text("Status: %s", globals::misc::autofriend_status.c_str());
                                ImGui::SameLine();
                                ImGui::Text("(%zu members)", autofriend::g_auto_friend_manager.getMemberCount());
                                ImGui::SameLine();
                                if (ImAdd::Button("Clear", ImVec2(50, 20))) {
                                    // Clear the friendly status for all cached members
                                    const auto& members = autofriend::g_auto_friend_manager.getCachedMembers();
                                    for (const auto& member : members) {
                                        globals::bools::player_status.erase(member.username);
                                        if (member.displayName != member.username) {
                                            globals::bools::player_status.erase(member.displayName);
                                        }
                                    }
                                    
                                    autofriend::g_auto_friend_manager.clearCache();
                                    globals::misc::group_members_loaded = false;
                                    globals::misc::autofriend_status = "Cleared";
                                }
                            } else {
                                ImGui::Text("Status: %s", globals::misc::autofriend_status.c_str());
                            }
                        }
                        
                        // Menu Hotkey - inline
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
                        ImGui::Text("Menu Hotkey:"); ImGui::SameLine();
                        Bind(&globals::misc::menu_hotkey, ImVec2(100, 0), false, ImGuiButtonFlags_AlignTextBaseLine);
                        ImGui::PopStyleVar(2);
                    }
                    ImGui::EndChild();

                    ImGui::NextColumn();

                    // Right Column: Configs
                    ImGui::BeginChild("Configs", ImVec2(ImGui::GetContentRegionAvail().x, 475.0f), true);
                    {
                        draw_shadowed_text("Configs");
                        ImDrawList* draw = ImGui::GetWindowDrawList();
                        ImVec2 content_start = ImGui::GetCursorScreenPos();
                        draw->AddLine(ImVec2(content_start.x, content_start.y), ImVec2(content_start.x + ImGui::GetContentRegionAvail().x, content_start.y), ImGui::GetColorU32(ImGuiCol_SliderGrab), 1.0f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y + 5.0f);
                        
                        g_config_system.render_config_ui(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
                    }
                    ImGui::EndChild();
                }

                ImGui::Columns(1);
                ImGui::EndChild();
                style.FrameBorderSize = 0;
                ImGui::End();
            }

            globals::combat::drawfov = globals::combat::usefov;
            globals::combat::drawsilentaimfov = globals::combat::silentaimfov;

            // Apply cursor visibility if setting changed while overlay is open
            {
                static bool last_custom_cursor_state = globals::misc::custom_cursor;
                if (overlay::visible && last_custom_cursor_state != globals::misc::custom_cursor) {
                    if (globals::misc::custom_cursor) {
                        ShowCursor(FALSE);
                    } else {
                        ShowCursor(TRUE);
                    }
                    last_custom_cursor_state = globals::misc::custom_cursor;
                }
            }

            // Custom spinning cross cursor
            if (overlay::visible && globals::misc::custom_cursor) { // Only draw when enabled
                ImGuiIO& io = ImGui::GetIO();
                ImVec2 mouse_pos = io.MousePos;
                ImDrawList* draw_list = ImGui::GetForegroundDrawList();
                float cursor_size = 10.0f; // Size of each arm of the cross
                float thickness = 1.5f;
                ImU32 cursor_color = ImGui::GetColorU32(ImGuiCol_Text); // White cursor

                float current_time = ImGui::GetTime();
                float rotation_speed = 5.0f; // Adjust speed as needed
                float angle = current_time * rotation_speed;

                // Draw the spinning cross
                // Horizontal line
                draw_list->AddLine(
                    ImVec2(mouse_pos.x - cursor_size * cos(angle), mouse_pos.y - cursor_size * sin(angle)),
                    ImVec2(mouse_pos.x + cursor_size * cos(angle), mouse_pos.y + cursor_size * sin(angle)),
                    cursor_color, thickness
                );
                // Vertical line
                draw_list->AddLine(
                    ImVec2(mouse_pos.x - cursor_size * cos(angle + IM_PI / 2), mouse_pos.y - cursor_size * sin(angle + IM_PI / 2)),
                    ImVec2(mouse_pos.x + cursor_size * cos(angle + IM_PI / 2), mouse_pos.y + cursor_size * sin(angle + IM_PI / 2)),
                    cursor_color, thickness
                );
            }

            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::Begin("esp", NULL,
                ImGuiWindowFlags_NoBackground
                |
                ImGuiWindowFlags_NoResize
                |
                ImGuiWindowFlags_NoCollapse
                |
                ImGuiWindowFlags_NoTitleBar
                |
                ImGuiWindowFlags_NoInputs
                |
                ImGuiWindowFlags_NoMouseInputs
                |
                ImGuiWindowFlags_NoDecoration
                |
                ImGuiWindowFlags_NoMove); {

                globals::instances::draw = ImGui::GetBackgroundDrawList();
                visuals::run();
                visuals::apply_fog_settings(); // Apply fog settings here
            }

            ImGui::End();


            if (overlay::visible) {
                // When visible, remove WS_EX_TRANSPARENT to allow interaction and proper rendering
                SetWindowLong(hwnd, GWL_EXSTYLE, (GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW) & ~WS_EX_TRANSPARENT);
            }
            else
            {
                // When not visible, add WS_EX_TRANSPARENT to make it click-through
                SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW);
            }

            if (globals::misc::streamproof)
            {
                SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
            }
            else
            {
                SetWindowDisplayAffinity(hwnd, WDA_NONE);
            }



            // Notification system with exception handling
            try {
                Notifications::Update();
                // Set and render notifications relative to the main overlay window
                if (overlay::visible) {
                    ImGuiIO& io = ImGui::GetIO();
                    ImVec2 screen_size = io.DisplaySize;

                    // Estimate notification dimensions for centering
                    const float estimated_notification_width = 300.0f; // Max width from NotificationSystem
                    const float estimated_notification_height = 30.0f; // Base height from NotificationSystem

                    // Calculate origin to center the first notification horizontally and place it further down
                    // Calculate origin to center the first notification horizontally and place it further down and to the right
                    // Calculate origin to center the first notification horizontally and place it further down and to the right
                    float origin_x = (screen_size.x / 2.0f) - (estimated_notification_width / 2.0f) + 90.0f; // Increased offset to move further right
                    float origin_y = screen_size.y * 0.8f; // Place notifications 80% down the screen

                    Notifications::SetDisplayOrigin(ImVec2(origin_x, origin_y));
                    Notifications::Render();
                } else {
                    Notifications::SetDisplayOrigin(ImVec2(0, 0)); // Reset origin if overlay is not visible
                    Notifications::Render(); // Render at default position if overlay is not visible
                }
            } catch (...) {
                // Silently handle notification-related exceptions
            }
        }
        else {
            exit(0);
        }




        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(0, 0);

        // Welcome notification with exception handling
        try {
            static bool welcome_notification_sent = false;
            if (!welcome_notification_sent && overlay::visible) {
                Notifications::Info("welcome to frizy.lol");
                welcome_notification_sent = true;
            }
        } catch (...) {
            // Silently handle welcome notification exceptions
        }

        static LARGE_INTEGER frequency;
        static LARGE_INTEGER lastTime;
        static bool timeInitialized = false;

        if (!timeInitialized) {
            QueryPerformanceFrequency(&frequency);
            QueryPerformanceCounter(&lastTime);
            timeBeginPeriod(1);
            timeInitialized = true;
        }

        const double targetFrameTime = 1.0 / 120.0; // Target 60 FPS

        LARGE_INTEGER currentTime;
        QueryPerformanceCounter(&currentTime);
        double elapsedSeconds = static_cast<double>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;

        if (elapsedSeconds < targetFrameTime) {
            DWORD sleepMilliseconds = static_cast<DWORD>((targetFrameTime - elapsedSeconds) * 1000.0);
            if (sleepMilliseconds > 0) {
                Sleep(sleepMilliseconds);
            }
        }

        do {
            QueryPerformanceCounter(&currentTime);
            elapsedSeconds = static_cast<double>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
        } while (elapsedSeconds < targetFrameTime);

        lastTime = currentTime;
        } catch (...) {
            // If any exception occurs in the main loop, continue to prevent crashes
            // This ensures the overlay keeps running even if individual operations fail
        }
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanup_avatar_system();
    CleanupDeviceD3D();
    RemoveTrayIcon();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    ReleaseSingleInstanceMutex();
}

bool fullsc(HWND windowHandle)
{
    MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
    if (GetMonitorInfo(MonitorFromWindow(windowHandle, MONITOR_DEFAULTTOPRIMARY), &monitorInfo))
    {
        RECT rect;
        if (GetWindowRect(windowHandle, &rect))
        {
            return rect.left == monitorInfo.rcMonitor.left
                && rect.right == monitorInfo.rcMonitor.right
                && rect.top == monitorInfo.rcMonitor.top
                && rect.bottom == monitorInfo.rcMonitor.bottom;
        }
    }
    return false;
}
bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_ERASEBKGND: // Handle WM_ERASEBKGND to prevent default background drawing
        return 1; // Indicate that the background has been erased
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case ID_TRAY_TOGGLE:
            ToggleOverlayVisibility();
            return 0;
        case ID_TRAY_RESTART:
            RestartApplication();
            return 0;
        case ID_TRAY_EXIT:
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }
        break;
    }
    case WM_APP_TRAY_ICON:
        if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP || lParam == WM_CONTEXTMENU)
        {
            ShowTrayMenu(hWnd);
            return 0;
        }
        break;

    default:
        break;
    }

    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
