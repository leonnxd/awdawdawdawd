#pragma once
#include <Windows.h>
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include "../../imgui/imgui.h"
#include "../../imgui/imgui_internal.h"
#include <iostream> // Added for debugging

extern const char* const key_names[];

class keybind
{
private:
    static std::unordered_map<int, bool> previous_key_states;
    bool last_key_state = false;

public:
    static std::unordered_map<int, bool> key_states_for_is_pressed; // Add this line

    static bool is_roblox_focused() {
        HWND foreground_window = GetForegroundWindow();
        if (foreground_window == NULL) {
            return false;
        }

        char window_title[256];
        GetWindowTextA(foreground_window, window_title, sizeof(window_title));
        std::string title_str(window_title);

        // Check for common Roblox window titles
        return title_str.find("Roblox") != std::string::npos ||
               title_str.find("RobloxPlayerBeta") != std::string::npos;
    }

    static enum c_keybind_type : int {
        TOGGLE,
        HOLD,
        ALWAYS
    };

    int key = 0;
    c_keybind_type type = TOGGLE;
    const char* name = "[None]";
    bool enabled = false;
    bool waiting_for_input = false;

    keybind(const char* _name)
    {
        this->name = _name;
        this->type = TOGGLE;
    }

    keybind(const char* _name, int default_key, c_keybind_type default_type = TOGGLE)
    {
        this->name = _name;
        this->key = default_key;
        this->type = default_type;
    }

    void set_key_code(int new_key) {
        key = new_key;
    }

    void update()
    {
        // std::cout << "[KEYBIND_DEBUG] Updating keybind: " << name << ", Key: " << key << ", Type: " << type << std::endl;

        if (key == 0) {
            if (type != ALWAYS) enabled = false;
            return;
        }

        bool current_key_state = (GetAsyncKeyState(key) & 0x8000) != 0;
        bool key_just_pressed = current_key_state && !last_key_state;

        if (!is_roblox_focused()) {
            if (type == HOLD) {
                enabled = false; // Only force-disable hold binds when Roblox is unfocused
            }
            last_key_state = false; // Reset last key state to prevent false triggers when refocusing
            return;
        }

        switch (type)
        {
        case TOGGLE:
            if (key_just_pressed) {
                enabled = !enabled;
                // std::cout << "[KEYBIND_DEBUG] Toggle keybind " << name << " enabled: " << enabled << std::endl;
            }
            break;
        case HOLD:
            enabled = current_key_state;
            // std::cout << "[KEYBIND_DEBUG] Hold keybind " << name << " enabled: " << enabled << std::endl;
            break;
        case ALWAYS:
            enabled = true;
            // std::cout << "[KEYBIND_DEBUG] Always keybind " << name << " enabled: " << enabled << std::endl;
            break;
        }

        last_key_state = current_key_state;
    }

    bool is_pressed() {
        if (!is_roblox_focused()) {
            return false; // Keybinds are not pressed if Roblox is not focused
        }
        bool current_key_state = (GetAsyncKeyState(key) & 0x8000) != 0;
        bool key_just_pressed = current_key_state && !previous_key_states[key];
        previous_key_states[key] = current_key_state;
        return key_just_pressed;
    }

    std::string get_key_name()
    {
        if (waiting_for_input)
            return "Waiting...";

        if (!key)
            return "None";

        if (key < 0 || key >= 256)
            return "Invalid";

        std::string key_str = key_names[key];
        std::transform(key_str.begin(), key_str.end(), key_str.begin(),
            [](unsigned char c) { return std::toupper(c); });
        return key_str;
    }

    std::string get_name()
    {
        return name;
    }

    std::string get_type()
    {
        switch (type)
        {
        case TOGGLE: return "toggle";
        case HOLD: return "hold";
        case ALWAYS: return "always";
        default: return "none";
        }
    }

    bool set_key()
    {
        if (ImGui::IsKeyChordPressed(ImGuiKey_Escape))
        {
            key = 0;
            ImGui::ClearActiveID();
            return true;
        }

        for (int i = 1; i < 5; i++)
        {
            if (ImGui::GetIO().MouseDown[i])
            {
                switch (i)
                {
                case 1: key = VK_RBUTTON; break;
                case 2: key = VK_MBUTTON; break;
                case 3: key = VK_XBUTTON1; break;
                case 4: key = VK_XBUTTON2; break;
                }
                return true;
            }
        }

        // Prioritize specific modifier keys
        if (GetAsyncKeyState(VK_LSHIFT) & 0x8000) { key = VK_LSHIFT; return true; }
        if (GetAsyncKeyState(VK_RSHIFT) & 0x8000) { key = VK_RSHIFT; return true; }
        if (GetAsyncKeyState(VK_LCONTROL) & 0x8000) { key = VK_LCONTROL; return true; }
        if (GetAsyncKeyState(VK_RCONTROL) & 0x8000) { key = VK_RCONTROL; return true; }
        if (GetAsyncKeyState(VK_LMENU) & 0x8000) { key = VK_LMENU; return true; } // Left Alt
        if (GetAsyncKeyState(VK_RMENU) & 0x8000) { key = VK_RMENU; return true; } // Right Alt
        if (GetAsyncKeyState(VK_LWIN) & 0x8000) { key = VK_LWIN; return true; }
        if (GetAsyncKeyState(VK_RWIN) & 0x8000) { key = VK_RWIN; return true; }

        // Then check other keys, excluding generic modifiers if specific ones are prioritized
        for (int i = 1; i < 256; i++) // Iterate through all possible virtual key codes
        {
            // Skip mouse buttons already handled
            if (i == VK_LBUTTON || i == VK_RBUTTON || i == VK_MBUTTON || i == VK_XBUTTON1 || i == VK_XBUTTON2) continue;

            // Skip generic modifier keys if their specific counterparts are prioritized
            if (i == VK_SHIFT || i == VK_CONTROL || i == VK_MENU) continue;

            // Skip specific modifier keys already handled
            if (i == VK_LSHIFT || i == VK_RSHIFT || i == VK_LCONTROL || i == VK_RCONTROL ||
                i == VK_LMENU || i == VK_RMENU || i == VK_LWIN || i == VK_RWIN) continue;

            if (GetAsyncKeyState(i) & 0x8000)
            {
                key = i;
                return true;
            }
        }

        return false;
    }
};
