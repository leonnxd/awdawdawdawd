#include "configsystem.h"
#include <iostream>
#include <algorithm>
#include "../notification/notification.h"
#include "../globals.h"

// Function to remove invalid characters from a string
std::string sanitize_filename(const std::string& name) {
    std::string sanitized_name = name;
    // Remove invalid characters
    sanitized_name.erase(std::remove_if(sanitized_name.begin(), sanitized_name.end(), [](char c) {
        return c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|';
    }), sanitized_name.end());
    return sanitized_name;
}

ConfigSystem::ConfigSystem() {
        char* appdata_path;
    size_t len;
    _dupenv_s(&appdata_path, &len, "APPDATA");

    if (appdata_path) {
        config_directory = std::string(appdata_path) + "\\nine\\config";
        free(appdata_path);
    }

        if (!fs::exists(config_directory)) {
        fs::create_directories(config_directory);
    }

    refresh_config_list();
    autoload_config_name = load_autoload_setting(); // Load autoload setting on startup

    if (!autoload_config_name.empty()) {
        std::cout << "[CONFIG] Attempting to autoload config: " << autoload_config_name << "\n";
        load_config(autoload_config_name);
    }       
}

void ConfigSystem::refresh_config_list() {
    config_files.clear();
    // Also clear autoload_config_name if the file it points to no longer exists
    if (!autoload_config_name.empty()) {
        std::string autoload_filepath = config_directory + "\\" + autoload_config_name + ".json";
        if (!fs::exists(autoload_filepath)) {
            autoload_config_name.clear();
            save_autoload_setting(""); // Clear the autoload setting if the config file is gone
        }
    }

    if (fs::exists(config_directory)) {
        for (const auto& entry : fs::directory_iterator(config_directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                config_files.push_back(entry.path().stem().string());
            }
        }
    }
}

bool ConfigSystem::save_config(const std::string& name) {
    if (name.empty()) return false;

    std::string sanitized_name = sanitize_filename(name);
    if (sanitized_name.empty()) return false;

    json config_json;

        std::cout << "[COMBAT] Saving combat settings..." << "\n";
    config_json["combat"]["rapidfire"] = globals::combat::rapidfire;
    config_json["combat"]["autostomp"] = globals::combat::autostomp;
    config_json["combat"]["aimbot"] = globals::combat::aimbot;
    config_json["combat"]["stickyaim"] = globals::combat::stickyaim;
    config_json["combat"]["aimbottype"] = globals::combat::aimbottype;
    config_json["combat"]["usefov"] = globals::combat::usefov;
    config_json["combat"]["drawfov"] = globals::combat::drawfov;
    config_json["combat"]["fovsize"] = globals::combat::fovsize;
    config_json["combat"]["glowfov"] = globals::combat::glowfov;
    config_json["combat"]["fovcolor"] = std::vector<float>(globals::combat::fovcolor, globals::combat::fovcolor + 4);
    config_json["combat"]["fovtransparency"] = globals::combat::fovtransparency;
    config_json["combat"]["fovfill"] = globals::combat::fovfill;
    config_json["combat"]["fovfilltransparency"] = globals::combat::fovfilltransparency;
    config_json["combat"]["fovfillcolor"] = std::vector<float>(globals::combat::fovfillcolor, globals::combat::fovfillcolor + 4);
    config_json["combat"]["fovglowcolor"] = std::vector<float>(globals::combat::fovglowcolor, globals::combat::fovglowcolor + 4);
    config_json["combat"]["fovshape"] = globals::combat::fovshape;
    config_json["combat"]["fovthickness"] = globals::combat::fovthickness;
    config_json["combat"]["smoothing"] = globals::combat::smoothing;
    config_json["combat"]["smoothingx"] = globals::combat::smoothingx;
    config_json["combat"]["smoothingy"] = globals::combat::smoothingy;
    config_json["combat"]["smoothing_style"] = globals::combat::smoothing_style;
    config_json["combat"]["aimbot_shake"] = globals::combat::aimbot_shake;
    config_json["combat"]["aimbot_shake_x"] = globals::combat::aimbot_shake_x;
    config_json["combat"]["aimbot_shake_y"] = globals::combat::aimbot_shake_y;
    config_json["combat"]["predictions"] = globals::combat::predictions;
    config_json["combat"]["predictionsx"] = globals::combat::predictionsx;
    config_json["combat"]["predictionsy"] = globals::combat::predictionsy;
    config_json["combat"]["silentpredictions"] = globals::combat::silentpredictions;
    config_json["combat"]["silentpredictionsx"] = globals::combat::silentpredictionsx;
    config_json["combat"]["silentpredictionsy"] = globals::combat::silentpredictionsy;
    config_json["combat"]["silent_shake"] = globals::combat::silent_shake;
    config_json["combat"]["silent_shake_x"] = globals::combat::silent_shake_x;
    config_json["combat"]["silent_shake_y"] = globals::combat::silent_shake_y;
    config_json["combat"]["teamcheck"] = globals::combat::teamcheck;
    config_json["combat"]["knockcheck"] = globals::combat::knockcheck;
    config_json["combat"]["rangecheck"] = globals::combat::rangecheck;
    config_json["combat"]["healthcheck"] = globals::combat::healthcheck;
    config_json["combat"]["wallcheck"] = globals::combat::wallcheck; // Added wallcheck
    config_json["combat"]["teamcheck_flag"] = (*globals::combat::flags)[0];
    config_json["combat"]["knockcheck_flag"] = (*globals::combat::flags)[1];
    config_json["combat"]["rangecheck_flag"] = (*globals::combat::flags)[2];
    config_json["combat"]["wallcheck_flag"] = (*globals::combat::flags)[3];
    config_json["combat"]["range"] = globals::combat::range;
    config_json["combat"]["aim_distance"] = globals::combat::aim_distance; // Added aim_distance
    config_json["combat"]["healththreshhold"] = globals::combat::healththreshhold;
    config_json["combat"]["aimpart"] = *globals::combat::aimpart;
    config_json["combat"]["airaimpart"] = *globals::combat::airaimpart;
    config_json["combat"]["silentaimpart"] = *globals::combat::silentaimpart;
    config_json["combat"]["airsilentaimpart"] = *globals::combat::airsilentaimpart;
    config_json["combat"]["silentaim"] = globals::combat::silentaim;
    config_json["combat"]["stickyaimsilent"] = globals::combat::stickyaimsilent;
    config_json["combat"]["silentaimtype"] = globals::combat::silentaimtype;
    config_json["combat"]["silentaimfov"] = globals::combat::silentaimfov;
    config_json["combat"]["drawsilentaimfov"] = globals::combat::drawsilentaimfov;
    config_json["combat"]["silentaimfovsize"] = globals::combat::silentaimfovsize;
    config_json["combat"]["glowsilentaimfov"] = globals::combat::glowsilentaimfov;
    config_json["combat"]["silentaimfovcolor"] = std::vector<float>(globals::combat::silentaimfovcolor, globals::combat::silentaimfovcolor + 4);
    config_json["combat"]["silentaimfovtransparency"] = globals::combat::silentaimfovtransparency;
    config_json["combat"]["silentaimfovfill"] = globals::combat::silentaimfovfill;
    config_json["combat"]["silentaimfovfilltransparency"] = globals::combat::silentaimfovfilltransparency;
    config_json["combat"]["silentaimfovfillcolor"] = std::vector<float>(globals::combat::silentaimfovfillcolor, globals::combat::silentaimfovfillcolor + 4);
    config_json["combat"]["silentaimfovglowcolor"] = std::vector<float>(globals::combat::silentaimfovglowcolor, globals::combat::silentaimfovglowcolor + 4);
    config_json["combat"]["silentaimfovshape"] = globals::combat::silentaimfovshape;
    config_json["combat"]["silentaimfovthickness"] = globals::combat::silentaimfovthickness;
    config_json["combat"]["spin_fov_aimbot"] = globals::combat::spin_fov_aimbot;
    config_json["combat"]["spin_fov_aimbot_speed"] = globals::combat::spin_fov_aimbot_speed;
    config_json["combat"]["spin_fov_silentaim"] = globals::combat::spin_fov_silentaim;
    config_json["combat"]["spin_fov_silentaim_speed"] = globals::combat::spin_fov_silentaim_speed;
    config_json["combat"]["orbit"] = globals::combat::orbit;
    config_json["combat"]["orbitspeed"] = globals::combat::orbitspeed;
    config_json["combat"]["orbitrange"] = globals::combat::orbitrange;
    config_json["combat"]["orbitheight"] = globals::combat::orbitheight;
    config_json["combat"]["drawradiusring"] = globals::combat::drawradiusring;
    config_json["combat"]["orbittype"] = globals::combat::orbittype;
    config_json["combat"]["antiaim"] = globals::combat::antiaim;
    config_json["combat"]["underground_antiaim"] = globals::combat::underground_antiaim;
    config_json["combat"]["triggerbot"] = globals::combat::triggerbot;
    config_json["combat"]["triggerbot_delay"] = globals::combat::triggerbot_delay;
    std::vector<int> triggerbot_checks_vec;
    triggerbot_checks_vec.reserve(globals::combat::triggerbot_item_checks.size());
    for (bool check : globals::combat::triggerbot_item_checks) {
        triggerbot_checks_vec.push_back(check ? 1 : 0);
    }
    config_json["combat"]["triggerbot_item_checks"] = triggerbot_checks_vec;

        std::cout << "[COMBAT][KEYBIND] Saving combat keybinds..." << "\n";
    std::cout << "[COMBAT][KEYBIND] Aimbot key: " << globals::combat::aimbotkeybind.key << "\n";
    config_json["combat"]["keybinds"]["aimbotkeybind"]["key"] = globals::combat::aimbotkeybind.key;
    config_json["combat"]["keybinds"]["aimbotkeybind"]["type"] = static_cast<int>(globals::combat::aimbotkeybind.type);
    std::cout << "[COMBAT][KEYBIND] Aimbot type: " << globals::combat::aimbotkeybind.type << "\n";
    config_json["combat"]["keybinds"]["silentaimkeybind"]["key"] = globals::combat::silentaimkeybind.key;
    config_json["combat"]["keybinds"]["silentaimkeybind"]["type"] = static_cast<int>(globals::combat::silentaimkeybind.type);
    config_json["combat"]["keybinds"]["orbitkeybind"]["key"] = globals::combat::orbitkeybind.key;
    config_json["combat"]["keybinds"]["orbitkeybind"]["type"] = static_cast<int>(globals::combat::orbitkeybind.type);
    config_json["combat"]["keybinds"]["locktargetkeybind"]["key"] = globals::combat::locktargetkeybind.key;
    config_json["combat"]["keybinds"]["locktargetkeybind"]["type"] = static_cast<int>(globals::combat::locktargetkeybind.type);
    config_json["combat"]["keybinds"]["triggerbotkeybind"]["key"] = globals::combat::triggerbotkeybind.key;
    config_json["combat"]["keybinds"]["triggerbotkeybind"]["type"] = static_cast<int>(globals::combat::triggerbotkeybind.type);

        std::cout << "[VISUALS] Saving visual settings..." << "\n";
    config_json["visuals"]["visuals"] = globals::visuals::visuals;
    config_json["visuals"]["boxes"] = globals::visuals::boxes;
    config_json["visuals"]["boxfill"] = globals::visuals::boxfill;
    config_json["visuals"]["lockedindicator"] = globals::visuals::lockedindicator;
    config_json["visuals"]["oofarrows"] = globals::visuals::oofarrows;
    config_json["visuals"]["lockedesp"] = globals::visuals::lockedesp; // Added lockedesp
    config_json["visuals"]["lockedespcolor"] = std::vector<float>(globals::visuals::lockedespcolor, globals::visuals::lockedespcolor + 4); // Added lockedespcolor
    config_json["visuals"]["snapline"] = globals::visuals::snapline;
    config_json["visuals"]["snaplinetype"] = globals::visuals::snaplinetype;
    config_json["visuals"]["snaplineoverlaytype"] = globals::visuals::snaplineoverlaytype;
    config_json["visuals"]["glowesp"] = globals::visuals::glowesp;
    config_json["visuals"]["boxtype"] = globals::visuals::boxtype;
    config_json["visuals"]["health"] = globals::visuals::health;
    config_json["visuals"]["healthbar"] = globals::visuals::healthbar;
    config_json["visuals"]["name"] = globals::visuals::name;
    config_json["visuals"]["nametype"] = globals::visuals::nametype;
    config_json["visuals"]["toolesp"] = globals::visuals::toolesp;
    config_json["visuals"]["distance"] = globals::visuals::distance;
    config_json["visuals"]["teamcheck_visual_flag"] = (*globals::visuals::visuals_flags)[0];
    config_json["visuals"]["knockcheck_visual_flag"] = (*globals::visuals::visuals_flags)[1];
    config_json["visuals"]["distance_visual_flag"] = (*globals::visuals::visuals_flags)[2];
    config_json["visuals"]["wallcheck_visual_flag"] = (*globals::visuals::visuals_flags)[3];
    config_json["visuals"]["chams"] = globals::visuals::chams;
    config_json["visuals"]["skeletons"] = globals::visuals::skeletons;
    config_json["visuals"]["localplayer"] = globals::visuals::localplayer;
    config_json["visuals"]["aimviewer"] = globals::visuals::aimviewer;
    config_json["visuals"]["esppreview"] = globals::visuals::esppreview;
    config_json["visuals"]["esp_preview_mode"] = globals::visuals::esp_preview_mode;
    config_json["visuals"]["predictionsdot"] = globals::visuals::predictionsdot;
    config_json["visuals"]["boxcolors"] = std::vector<float>(globals::visuals::boxcolors, globals::visuals::boxcolors + 4);
    config_json["visuals"]["boxfillcolor"] = std::vector<float>(globals::visuals::boxfillcolor, globals::visuals::boxfillcolor + 4);
    config_json["visuals"]["glowcolor"] = std::vector<float>(globals::visuals::glowcolor, globals::visuals::glowcolor + 4);
    config_json["visuals"]["lockedcolor"] = std::vector<float>(globals::visuals::lockedcolor, globals::visuals::lockedcolor + 4);
    config_json["visuals"]["oofcolor"] = std::vector<float>(globals::visuals::oofcolor, globals::visuals::oofcolor + 4);
    config_json["visuals"]["snaplinecolor"] = std::vector<float>(globals::visuals::snaplinecolor, globals::visuals::snaplinecolor + 4);
    config_json["visuals"]["healthbarcolor"] = std::vector<float>(globals::visuals::healthbarcolor, globals::visuals::healthbarcolor + 4);
    config_json["visuals"]["healthbarcolor1"] = std::vector<float>(globals::visuals::healthbarcolor1, globals::visuals::healthbarcolor1 + 4);
    config_json["visuals"]["namecolor"] = std::vector<float>(globals::visuals::namecolor, globals::visuals::namecolor + 4);
    config_json["visuals"]["toolespcolor"] = std::vector<float>(globals::visuals::toolespcolor, globals::visuals::toolespcolor + 4);
    config_json["visuals"]["distancecolor"] = std::vector<float>(globals::visuals::distancecolor, globals::visuals::distancecolor + 4);
    config_json["visuals"]["chamscolor"] = std::vector<float>(globals::visuals::chamscolor, globals::visuals::chamscolor + 4);
    config_json["visuals"]["skeletonscolor"] = std::vector<float>(globals::visuals::skeletonscolor, globals::visuals::skeletonscolor + 4);
    config_json["visuals"]["fortniteindicator"] = globals::visuals::fortniteindicator;
    config_json["visuals"]["hittracer"] = globals::visuals::hittracer;
    config_json["visuals"]["trail"] = globals::visuals::trail;
    config_json["visuals"]["hitbubble"] = globals::visuals::hitbubble;
    config_json["visuals"]["targetchams"] = globals::visuals::targetchams;
    config_json["visuals"]["targetskeleton"] = globals::visuals::targetskeleton;
    config_json["visuals"]["localplayerchams"] = globals::visuals::localplayerchams;
    config_json["visuals"]["localgunchams"] = globals::visuals::localgunchams;
    config_json["visuals"]["enemycheck"] = globals::visuals::enemycheck;
    config_json["visuals"]["friendlycheck"] = globals::visuals::friendlycheck;
    config_json["visuals"]["teamcheck"] = globals::visuals::teamcheck;
    config_json["visuals"]["rangecheck"] = globals::visuals::rangecheck;
    config_json["visuals"]["range"] = globals::visuals::range;
    config_json["visuals"]["visual_range"] = globals::visuals::visual_range;
    config_json["visuals"]["sonar"] = globals::visuals::sonar;
    config_json["visuals"]["sonar_range"] = globals::visuals::sonar_range;
    config_json["visuals"]["sonar_thickness"] = globals::visuals::sonar_thickness;
    config_json["visuals"]["sonar_speed"] = globals::visuals::sonar_speed;
    config_json["visuals"]["sonarcolor"] = std::vector<float>(globals::visuals::sonarcolor, globals::visuals::sonarcolor + 4);
    config_json["visuals"]["sonar_detect_players"] = globals::visuals::sonar_detect_players;
    config_json["visuals"]["sonar_detect_color_out"] = std::vector<float>(globals::visuals::sonar_detect_color_out, globals::visuals::sonar_detect_color_out + 4);
    config_json["visuals"]["sonar_detect_color_in"] = std::vector<float>(globals::visuals::sonar_detect_color_in, globals::visuals::sonar_detect_color_in + 4);
    config_json["visuals"]["box_overlay_flags"] = *globals::visuals::box_overlay_flags;
    config_json["visuals"]["name_overlay_flags"] = *globals::visuals::name_overlay_flags;
    config_json["visuals"]["healthbar_overlay_flags"] = *globals::visuals::healthbar_overlay_flags;
    config_json["visuals"]["tool_overlay_flags"] = *globals::visuals::tool_overlay_flags;
    config_json["visuals"]["distance_overlay_flags"] = *globals::visuals::distance_overlay_flags;
    config_json["visuals"]["skeleton_overlay_flags"] = *globals::visuals::skeleton_overlay_flags;
    config_json["visuals"]["chams_overlay_flags"] = *globals::visuals::chams_overlay_flags;
    config_json["visuals"]["snapline_overlay_flags"] = *globals::visuals::snapline_overlay_flags;
    config_json["visuals"]["oof_overlay_flags"] = *globals::visuals::oof_overlay_flags;
    config_json["visuals"]["target_only_esp"] = globals::visuals::target_only_esp;
    config_json["visuals"]["target_only_list"] = globals::visuals::target_only_list;
    config_json["visuals"]["fog_changer"] = globals::visuals::fog_changer; // Added fog_changer
    config_json["visuals"]["fog_start"] = globals::visuals::fog_start; // Added fog_start
    config_json["visuals"]["fog_end"] = globals::visuals::fog_end; // Added fog_end
    config_json["visuals"]["fog_color"] = std::vector<float>(globals::visuals::fog_color, globals::visuals::fog_color + 4); // Added fog_color
    config_json["misc"]["menuglowcolor"] = std::vector<float>(globals::misc::menuglowcolor, globals::misc::menuglowcolor + 4); // Added menuglowcolor save
    config_json["misc"]["overlay_color"] = std::vector<float>(globals::misc::overlay_color, globals::misc::overlay_color + 4);
    config_json["misc"]["accent_color"] = std::vector<float>(globals::misc::accent_color, globals::misc::accent_color + 4);
    config_json["misc"]["overlay_star_color"] = std::vector<float>(globals::misc::overlay_star_color, globals::misc::overlay_star_color + 4);
    config_json["misc"]["health_full_color"] = std::vector<float>(globals::misc::health_full_color, globals::misc::health_full_color + 4);
    config_json["misc"]["health_low_color"] = std::vector<float>(globals::misc::health_low_color, globals::misc::health_low_color + 4);
    config_json["misc"]["show_theme_window"] = globals::misc::show_theme_window;

        std::cout << "[MISC] Saving misc settings..." << "\n";
    config_json["misc"]["speed"] = globals::misc::speed;
    config_json["misc"]["speedtype"] = globals::misc::speedtype;
    config_json["misc"]["speedvalue"] = globals::misc::speedvalue;
    config_json["misc"]["nojumpcooldown"] = globals::misc::nojumpcooldown;
    config_json["misc"]["jumppower"] = globals::misc::jumppower;
    config_json["misc"]["jumppowervalue"] = globals::misc::jumppowervalue;
    config_json["misc"]["flight"] = globals::misc::flight;
    config_json["misc"]["flighttype"] = globals::misc::flighttype;
    config_json["misc"]["flightvalue"] = globals::misc::flightvalue;
    config_json["misc"]["hipheight"] = globals::misc::hipheight;
    config_json["misc"]["hipheightvalue"] = globals::misc::hipheightvalue;
    config_json["misc"]["rapidfire"] = globals::misc::rapidfire;
    config_json["misc"]["autoarmor"] = globals::misc::autoarmor;
    config_json["misc"]["autoreload"] = globals::misc::autoreload;
    config_json["misc"]["autostomp"] = globals::misc::autostomp;
    config_json["misc"]["antistomp"] = globals::misc::antistomp;
    config_json["misc"]["bikefly"] = globals::misc::bikefly;
    config_json["misc"]["spectate"] = globals::misc::spectate;
    config_json["misc"]["vsync"] = globals::misc::vsync;
    config_json["misc"]["watermark"] = globals::misc::watermark;
    config_json["misc"]["watermarkstuff"] = *globals::misc::watermarkstuff;
    config_json["misc"]["targethud"] = globals::misc::targethud;
    config_json["misc"]["playerlist"] = globals::misc::playerlist;
    config_json["misc"]["keybindsstyle"] = globals::misc::keybindsstyle;
    config_json["misc"]["keybinds"] = globals::misc::keybinds;
    config_json["misc"]["spotify"] = globals::misc::spotify;
    config_json["misc"]["explorer"] = globals::misc::explorer;
        config_json["misc"]["colors"] = globals::misc::colors;
        config_json["misc"]["streamproof"] = globals::misc::streamproof;
        config_json["misc"]["custom_cursor"] = globals::misc::custom_cursor;
        config_json["misc"]["rotate360"] = globals::misc::rotate360;
        config_json["misc"]["camera_rotation_yaw"] = globals::misc::camera_rotation_yaw;
        config_json["misc"]["rotate360_speed"] = globals::misc::rotate360_speed;
        config_json["misc"]["menuglow"] = globals::misc::menuglow;
        config_json["misc"]["overlay_stars"] = globals::misc::overlay_stars;
        
        // Save misc keybinds
    std::cout << "[COMBAT][KEYBIND] Saving combat keybinds..." << "\n";
    try {
        std::cout << "[COMBAT][KEYBIND] Aimbot key: " << globals::combat::aimbotkeybind.key << "\n";
        config_json["combat"]["keybinds"]["aimbotkeybind"]["key"] = globals::combat::aimbotkeybind.key;
        config_json["combat"]["keybinds"]["aimbotkeybind"]["type"] = static_cast<int>(globals::combat::aimbotkeybind.type);
        std::cout << "[COMBAT][KEYBIND] Aimbot type: " << globals::combat::aimbotkeybind.type << "\n";
        config_json["combat"]["keybinds"]["silentaimkeybind"]["key"] = globals::combat::silentaimkeybind.key;
        config_json["combat"]["keybinds"]["silentaimkeybind"]["type"] = static_cast<int>(globals::combat::silentaimkeybind.type);
        config_json["combat"]["keybinds"]["orbitkeybind"]["key"] = globals::combat::orbitkeybind.key;
        config_json["combat"]["keybinds"]["orbitkeybind"]["type"] = static_cast<int>(globals::combat::orbitkeybind.type);
        config_json["combat"]["keybinds"]["locktargetkeybind"]["key"] = globals::combat::locktargetkeybind.key;
        config_json["combat"]["keybinds"]["locktargetkeybind"]["type"] = static_cast<int>(globals::combat::locktargetkeybind.type);
    }
    catch (const json::exception& e) {
        std::cout << "[COMBAT][KEYBIND] Failed to save combat keybinds (JSON error: " << e.what() << "), using defaults" << "\n";
        // Set default values if saving fails
        config_json["combat"]["keybinds"]["aimbotkeybind"]["key"] = 0;
        config_json["combat"]["keybinds"]["aimbotkeybind"]["type"] = 1;
        config_json["combat"]["keybinds"]["silentaimkeybind"]["key"] = 0;
        config_json["combat"]["keybinds"]["silentaimkeybind"]["type"] = 1;
        config_json["combat"]["keybinds"]["orbitkeybind"]["key"] = 0;
        config_json["combat"]["keybinds"]["orbitkeybind"]["type"] = 1;
        config_json["combat"]["keybinds"]["locktargetkeybind"]["key"] = 0;
        config_json["combat"]["keybinds"]["locktargetkeybind"]["type"] = 1;
        config_json["combat"]["keybinds"]["triggerbotkeybind"]["key"] = 0;
        config_json["combat"]["keybinds"]["triggerbotkeybind"]["type"] = 1;
    }
    catch (...) {
        std::cout << "[COMBAT][KEYBIND] Failed to save combat keybinds (unknown error), using defaults" << "\n";
        // Set default values if saving fails
        config_json["combat"]["keybinds"]["aimbotkeybind"]["key"] = 0;
        config_json["combat"]["keybinds"]["aimbotkeybind"]["type"] = 1;
        config_json["combat"]["keybinds"]["silentaimkeybind"]["key"] = 0;
        config_json["combat"]["keybinds"]["silentaimkeybind"]["type"] = 1;
        config_json["combat"]["keybinds"]["orbitkeybind"]["key"] = 0;
        config_json["combat"]["keybinds"]["orbitkeybind"]["type"] = 1;
        config_json["combat"]["keybinds"]["locktargetkeybind"]["key"] = 0;
        config_json["combat"]["keybinds"]["locktargetkeybind"]["type"] = 1;
        config_json["combat"]["keybinds"]["triggerbotkeybind"]["key"] = 0;
        config_json["combat"]["keybinds"]["triggerbotkeybind"]["type"] = 1;
    }

    std::cout << "[VISUALS] Saving visual settings..." << "\n";
    config_json["visuals"]["visuals"] = globals::visuals::visuals;
    config_json["visuals"]["boxes"] = globals::visuals::boxes;
    config_json["visuals"]["boxfill"] = globals::visuals::boxfill;
    config_json["visuals"]["lockedindicator"] = globals::visuals::lockedindicator;
    config_json["visuals"]["oofarrows"] = globals::visuals::oofarrows;
    config_json["visuals"]["snapline"] = globals::visuals::snapline;
    config_json["visuals"]["glowesp"] = globals::visuals::glowesp;
    config_json["visuals"]["boxtype"] = globals::visuals::boxtype;
    config_json["visuals"]["health"] = globals::visuals::health;
    config_json["visuals"]["healthbar"] = globals::visuals::healthbar; // Added healthbar
    config_json["visuals"]["name"] = globals::visuals::name;
    config_json["visuals"]["nametype"] = globals::visuals::nametype;
    config_json["visuals"]["toolesp"] = globals::visuals::toolesp;
    config_json["visuals"]["distance"] = globals::visuals::distance;
    config_json["visuals"]["visuals_flags"] = *globals::visuals::visuals_flags;
    config_json["visuals"]["chams"] = globals::visuals::chams;
    config_json["visuals"]["skeletons"] = globals::visuals::skeletons;
    config_json["visuals"]["localplayer"] = globals::visuals::localplayer;
    config_json["visuals"]["aimviewer"] = globals::visuals::aimviewer;
    config_json["visuals"]["esppreview"] = globals::visuals::esppreview;
    config_json["visuals"]["predictionsdot"] = globals::visuals::predictionsdot;
    config_json["visuals"]["boxcolors"] = std::vector<float>(globals::visuals::boxcolors, globals::visuals::boxcolors + 4);
    config_json["visuals"]["boxfillcolor"] = std::vector<float>(globals::visuals::boxfillcolor, globals::visuals::boxfillcolor + 4);
    config_json["visuals"]["glowcolor"] = std::vector<float>(globals::visuals::glowcolor, globals::visuals::glowcolor + 4);
    config_json["visuals"]["lockedcolor"] = std::vector<float>(globals::visuals::lockedcolor, globals::visuals::lockedcolor + 4);
    config_json["visuals"]["oofcolor"] = std::vector<float>(globals::visuals::oofcolor, globals::visuals::oofcolor + 4);
    config_json["visuals"]["snaplinecolor"] = std::vector<float>(globals::visuals::snaplinecolor, globals::visuals::snaplinecolor + 4);
    config_json["visuals"]["healthbarcolor"] = std::vector<float>(globals::visuals::healthbarcolor, globals::visuals::healthbarcolor + 4);
    config_json["visuals"]["healthbarcolor1"] = std::vector<float>(globals::visuals::healthbarcolor1, globals::visuals::healthbarcolor1 + 4);
    config_json["visuals"]["namecolor"] = std::vector<float>(globals::visuals::namecolor, globals::visuals::namecolor + 4);
    config_json["visuals"]["toolespcolor"] = std::vector<float>(globals::visuals::toolespcolor, globals::visuals::toolespcolor + 4);
    config_json["visuals"]["distancecolor"] = std::vector<float>(globals::visuals::distancecolor, globals::visuals::distancecolor + 4);
    config_json["visuals"]["chamscolor"] = std::vector<float>(globals::visuals::chamscolor, globals::visuals::chamscolor + 4);
    config_json["visuals"]["skeletonscolor"] = std::vector<float>(globals::visuals::skeletonscolor, globals::visuals::skeletonscolor + 4);
    config_json["visuals"]["fortniteindicator"] = globals::visuals::fortniteindicator;
    config_json["visuals"]["hittracer"] = globals::visuals::hittracer;
    config_json["visuals"]["trail"] = globals::visuals::trail;
    config_json["visuals"]["hitbubble"] = globals::visuals::hitbubble;
    config_json["visuals"]["targetchams"] = globals::visuals::targetchams;
    config_json["visuals"]["targetskeleton"] = globals::visuals::targetskeleton;
    config_json["visuals"]["localplayerchams"] = globals::visuals::localplayerchams;
    config_json["visuals"]["localgunchams"] = globals::visuals::localgunchams;
    config_json["visuals"]["enemycheck"] = globals::visuals::enemycheck;
    config_json["visuals"]["friendlycheck"] = globals::visuals::friendlycheck;
    config_json["visuals"]["teamcheck"] = globals::visuals::teamcheck;
    config_json["visuals"]["rangecheck"] = globals::visuals::rangecheck;
    config_json["visuals"]["range"] = globals::visuals::range;

    std::cout << "[MISC] Saving misc settings..." << "\n";
    try {
        config_json["misc"]["speed"] = globals::misc::speed;
        config_json["misc"]["speedtype"] = globals::misc::speedtype;
        config_json["misc"]["speedvalue"] = globals::misc::speedvalue;
        config_json["misc"]["nojumpcooldown"] = globals::misc::nojumpcooldown;
        config_json["misc"]["flight"] = globals::misc::flight;
        config_json["misc"]["flighttype"] = globals::misc::flighttype;
        config_json["misc"]["flightvalue"] = globals::misc::flightvalue;
        config_json["misc"]["hipheight"] = globals::misc::hipheight;
        config_json["misc"]["hipheightvalue"] = globals::misc::hipheightvalue;
        config_json["misc"]["rapidfire"] = globals::misc::rapidfire;
        config_json["misc"]["autoarmor"] = globals::misc::autoarmor;
        config_json["misc"]["autoreload"] = globals::misc::autoreload;
        config_json["misc"]["autostomp"] = globals::misc::autostomp;
        config_json["misc"]["antistomp"] = globals::misc::antistomp;
        config_json["misc"]["vsync"] = globals::misc::vsync;
        config_json["misc"]["watermark"] = globals::misc::watermark;
        config_json["misc"]["targethud"] = globals::misc::targethud;
        config_json["misc"]["playerlist"] = globals::misc::playerlist;
        config_json["misc"]["keybinds"] = globals::misc::keybinds;
        config_json["misc"]["spotify"] = globals::misc::spotify;
        config_json["misc"]["explorer"] = globals::misc::explorer;
        config_json["misc"]["colors"] = globals::misc::colors;
        config_json["misc"]["streamproof"] = globals::misc::streamproof;
        config_json["misc"]["rotate360"] = globals::misc::rotate360;
        config_json["misc"]["rotate360_speed"] = globals::misc::rotate360_speed;
        
        // Save misc keybinds
        std::cout << "[MISC][KEYBIND] Saving misc keybinds..." << "\n";
        config_json["misc"]["keybinds_data"]["speedkeybind"]["key"] = globals::misc::speedkeybind.key;
        config_json["misc"]["keybinds_data"]["speedkeybind"]["type"] = static_cast<int>(globals::misc::speedkeybind.type);
        config_json["misc"]["keybinds_data"]["jumppowerkeybind"]["key"] = globals::misc::jumppowerkeybind.key;
        config_json["misc"]["keybinds_data"]["jumppowerkeybind"]["type"] = static_cast<int>(globals::misc::jumppowerkeybind.type);
        config_json["misc"]["keybinds_data"]["flightkeybind"]["key"] = globals::misc::flightkeybind.key;
        config_json["misc"]["keybinds_data"]["flightkeybind"]["type"] = static_cast<int>(globals::misc::flightkeybind.type);
        config_json["misc"]["keybinds_data"]["stompkeybind"]["key"] = globals::misc::stompkeybind.key;
        config_json["misc"]["keybinds_data"]["stompkeybind"]["type"] = static_cast<int>(globals::misc::stompkeybind.type);
        config_json["misc"]["keybinds_data"]["spectatebind"]["key"] = globals::misc::spectatebind.key;
        config_json["misc"]["keybinds_data"]["spectatebind"]["type"] = static_cast<int>(globals::misc::spectatebind.type);
        config_json["misc"]["keybinds_data"]["rotate360keybind"]["key"] = globals::misc::rotate360keybind.key;
        config_json["misc"]["keybinds_data"]["rotate360keybind"]["type"] = static_cast<int>(globals::misc::rotate360keybind.type);
        
        // Save menu_hotkey
        config_json["misc"]["keybinds_data"]["menu_hotkey"]["key"] = globals::misc::menu_hotkey.key;
        config_json["misc"]["keybinds_data"]["menu_hotkey"]["type"] = static_cast<int>(globals::misc::menu_hotkey.type);

        std::cout << "[MISC][KEYBIND] Successfully saved misc keybinds" << "\n";
    }
    catch (const json::exception& e) { // Catch specific JSON exceptions
        std::cout << "[MISC][KEYBIND] Failed to save misc settings (JSON error: " << e.what() << "), using defaults" << "\n";
        config_json["misc"]["speed"] = false;
        config_json["misc"]["speedtype"] = 0;
        config_json["misc"]["speedvalue"] = 1.0f;
        config_json["misc"]["jumppower"] = false;
        config_json["misc"]["jumpowervalue"] = 1.0f;
        config_json["misc"]["flight"] = false;
        config_json["misc"]["flighttype"] = 0;
        config_json["misc"]["flightvalue"] = 1.0f;
        config_json["misc"]["hipheight"] = false;
        config_json["misc"]["hipheightvalue"] = 1.0f;
        config_json["misc"]["rapidfire"] = false;
        config_json["misc"]["autoarmor"] = false;
        config_json["misc"]["autoreload"] = false;
        config_json["misc"]["autostomp"] = false;
        config_json["misc"]["antistomp"] = false;
        config_json["misc"]["vsync"] = false;
        config_json["misc"]["watermark"] = false;
        config_json["misc"]["targethud"] = false;
        config_json["misc"]["playerlist"] = false;
        config_json["misc"]["keybinds"] = false;
        config_json["misc"]["spotify"] = false;
        config_json["misc"]["explorer"] = false;
        config_json["misc"]["colors"] = false;
        config_json["misc"]["streamproof"] = false;
        config_json["misc"]["rotate360"] = false;
        config_json["misc"]["rotate360_speed"] = 1.0f;

        config_json["misc"]["keybinds_data"]["speedkeybind"]["key"] = 0;
        config_json["misc"]["keybinds_data"]["speedkeybind"]["type"] = 1;
        // Removed nojumpcooldownkeybind from default values
        config_json["misc"]["keybinds_data"]["flightkeybind"]["key"] = 0;
        config_json["misc"]["keybinds_data"]["flightkeybind"]["type"] = 1;
        config_json["misc"]["keybinds_data"]["stompkeybind"]["key"] = 0;
        config_json["misc"]["keybinds_data"]["stompkeybind"]["type"] = 1;
        config_json["misc"]["keybinds_data"]["rotate360keybind"]["key"] = 0;
        config_json["misc"]["keybinds_data"]["rotate360keybind"]["type"] = 1;
        
        // Default for menu_hotkey if saving fails
        config_json["misc"]["keybinds_data"]["menu_hotkey"]["key"] = VK_RSHIFT;
        config_json["misc"]["keybinds_data"]["menu_hotkey"]["type"] = 0; // TOGGLE
    }
    catch (...) { // Catch any other exceptions
        std::cout << "[MISC][KEYBIND] Failed to save misc settings (unknown error), using defaults" << "\n";
        config_json["misc"]["speed"] = false;
        config_json["misc"]["speedtype"] = 0;
        config_json["misc"]["speedvalue"] = 1.0f;
        config_json["misc"]["jumppower"] = false;
        config_json["misc"]["jumpowervalue"] = 1.0f;
        config_json["misc"]["flight"] = false;
        config_json["misc"]["flighttype"] = 0;
        config_json["misc"]["flightvalue"] = 1.0f;
        config_json["misc"]["hipheight"] = false;
        config_json["misc"]["hipheightvalue"] = 1.0f;
        config_json["misc"]["rapidfire"] = false;
        config_json["misc"]["autoarmor"] = false;
        config_json["misc"]["autoreload"] = false;
        config_json["misc"]["autostomp"] = false;
        config_json["misc"]["antistomp"] = false;
        config_json["misc"]["vsync"] = false;
        config_json["misc"]["watermark"] = false;
        config_json["misc"]["targethud"] = false;
        config_json["misc"]["playerlist"] = false;
        config_json["misc"]["keybinds"] = false;
        config_json["misc"]["spotify"] = false;
        config_json["misc"]["explorer"] = false;
        config_json["misc"]["colors"] = false;
        config_json["misc"]["streamproof"] = false;
        config_json["misc"]["rotate360"] = false;
        config_json["misc"]["rotate360_speed"] = 1.0f;

        config_json["misc"]["keybinds_data"]["speedkeybind"]["key"] = 0;
        config_json["misc"]["keybinds_data"]["speedkeybind"]["type"] = 1;
        config_json["misc"]["keybinds_data"]["jumppowerkeybind"]["key"] = 0;
        config_json["misc"]["keybinds_data"]["jumppowerkeybind"]["type"] = 1;
        config_json["misc"]["keybinds_data"]["flightkeybind"]["key"] = 0;
        config_json["misc"]["keybinds_data"]["flightkeybind"]["type"] = 1;
        config_json["misc"]["keybinds_data"]["stompkeybind"]["key"] = 0;
        config_json["misc"]["keybinds_data"]["stompkeybind"]["type"] = 1;
        config_json["misc"]["keybinds_data"]["rotate360keybind"]["key"] = 0;
        config_json["misc"]["keybinds_data"]["rotate360keybind"]["type"] = 1;
        
        // Default for menu_hotkey if saving fails
        config_json["misc"]["keybinds_data"]["menu_hotkey"]["key"] = VK_RSHIFT;
        config_json["misc"]["keybinds_data"]["menu_hotkey"]["type"] = 0; // TOGGLE
    }

    std::string filepath = config_directory + "\\" + sanitized_name + ".json";
    std::ofstream file(filepath);

    if (file.is_open()) {
        file << config_json.dump(2);         file.close();

        refresh_config_list();
        current_config_name = sanitized_name;
        std::cout << "[CONFIG] Successfully saved config: " << sanitized_name << "\n";
        return true;
    }

    std::cout << "[CONFIG] Failed to save config: " << sanitized_name << "\n";
    return false;
}

bool ConfigSystem::save_autoload_setting(const std::string& name) {
    std::string autoload_filepath = config_directory + "\\autoload.txt";
    std::ofstream file(autoload_filepath);
    if (file.is_open()) {
        file << name;
        file.close();
        autoload_config_name = name;
        std::cout << "[CONFIG] Autoload setting saved: " << name << "\n";
        return true;
    }
    std::cout << "[CONFIG] Failed to save autoload setting.\n";
    return false;
}

std::string ConfigSystem::load_autoload_setting() {
    std::string autoload_filepath = config_directory + "\\autoload.txt";
    std::ifstream file(autoload_filepath);
    std::string name;
    if (file.is_open()) {
        std::getline(file, name);
        file.close();
        std::cout << "[CONFIG] Autoload setting loaded: " << name << "\n";
        return name;
    }
    std::cout << "[CONFIG] No autoload setting found or failed to open autoload.txt.\n";
    return "";
}

bool ConfigSystem::load_config(const std::string& name) {
    if (name.empty()) return false;

    std::string sanitized_name = sanitize_filename(name);
    if (sanitized_name.empty()) return false;

    std::string filepath = config_directory + "\\" + sanitized_name + ".json";
    std::ifstream file(filepath);

    if (file.is_open()) {
        try {
            json config_json;
            file >> config_json;

            std::cout << "[CONFIG] Loading config: " << name << "\n";

                // Reset combat toggles that should disable when not present in config
                globals::combat::fovfill = false;
                globals::combat::silentaimfovfill = false;

                if (config_json.contains("combat")) {
                    std::cout << "[COMBAT] Loading combat settings..." << "\n";
                    auto& combat = config_json["combat"];
                if (combat.contains("rapidfire")) globals::combat::rapidfire = combat["rapidfire"];
                if (combat.contains("autostomp")) globals::combat::autostomp = combat["autostomp"];
                if (combat.contains("aimbot")) globals::combat::aimbot = combat["aimbot"];
                if (combat.contains("stickyaim")) globals::combat::stickyaim = combat["stickyaim"];
                if (combat.contains("aimbottype")) globals::combat::aimbottype = combat["aimbottype"];
                if (combat.contains("usefov")) globals::combat::usefov = combat["usefov"];
                if (combat.contains("drawfov")) globals::combat::drawfov = combat["drawfov"];
                if (combat.contains("fovsize")) globals::combat::fovsize = combat["fovsize"];
                if (combat.contains("glowfov")) globals::combat::glowfov = combat["glowfov"];
                if (combat.contains("fovcolor") && combat["fovcolor"].is_array() && combat["fovcolor"].size() == 4) {
                    for (int i = 0; i < 4; ++i) globals::combat::fovcolor[i] = combat["fovcolor"][i].get<float>();
                }
                if (combat.contains("fovtransparency")) globals::combat::fovtransparency = combat["fovtransparency"];
                if (combat.contains("fovfill")) globals::combat::fovfill = combat["fovfill"];
                if (combat.contains("fovfilltransparency")) {
                    globals::combat::fovfilltransparency = combat["fovfilltransparency"];
                    globals::combat::fovfilltransparency = std::clamp(globals::combat::fovfilltransparency, 0.0f, 1.0f);
                }
                if (combat.contains("fovfillcolor") && combat["fovfillcolor"].is_array() && combat["fovfillcolor"].size() == 4) {
                    for (int i = 0; i < 4; ++i) globals::combat::fovfillcolor[i] = combat["fovfillcolor"][i].get<float>();
                }
                if (combat.contains("fovglowcolor") && combat["fovglowcolor"].is_array() && combat["fovglowcolor"].size() == 4) {
                    for (int i = 0; i < 4; ++i) globals::combat::fovglowcolor[i] = combat["fovglowcolor"][i].get<float>();
                }
                if (combat.contains("fovshape")) globals::combat::fovshape = combat["fovshape"];
                if (combat.contains("fovthickness")) globals::combat::fovthickness = combat["fovthickness"];
                if (combat.contains("smoothing")) globals::combat::smoothing = combat["smoothing"];
                if (combat.contains("smoothingx")) globals::combat::smoothingx = combat["smoothingx"];
                if (combat.contains("smoothingy")) globals::combat::smoothingy = combat["smoothingy"];
                if (combat.contains("smoothing_style")) globals::combat::smoothing_style = combat["smoothing_style"];
                if (combat.contains("aimbot_shake")) globals::combat::aimbot_shake = combat["aimbot_shake"];
                if (combat.contains("aimbot_shake_x")) globals::combat::aimbot_shake_x = combat["aimbot_shake_x"];
                if (combat.contains("aimbot_shake_y")) globals::combat::aimbot_shake_y = combat["aimbot_shake_y"];
                if (combat.contains("predictions")) globals::combat::predictions = combat["predictions"];
                if (combat.contains("predictionsx")) globals::combat::predictionsx = combat["predictionsx"];
                if (combat.contains("predictionsy")) globals::combat::predictionsy = combat["predictionsy"];
                if (combat.contains("silentpredictions")) globals::combat::silentpredictions = combat["silentpredictions"];
                if (combat.contains("silentpredictionsx")) globals::combat::silentpredictionsx = combat["silentpredictionsx"];
                if (combat.contains("silentpredictionsy")) globals::combat::silentpredictionsy = combat["silentpredictionsy"];
                if (combat.contains("silent_shake")) globals::combat::silent_shake = combat["silent_shake"];
                if (combat.contains("silent_shake_x")) globals::combat::silent_shake_x = combat["silent_shake_x"];
                if (combat.contains("silent_shake_y")) globals::combat::silent_shake_y = combat["silent_shake_y"];
                if (combat.contains("teamcheck")) globals::combat::teamcheck = combat["teamcheck"];
                if (combat.contains("knockcheck")) globals::combat::knockcheck = combat["knockcheck"];
                if (combat.contains("rangecheck")) globals::combat::rangecheck = combat["rangecheck"];
                if (combat.contains("healthcheck")) globals::combat::healthcheck = combat["healthcheck"];
                if (combat.contains("wallcheck")) globals::combat::wallcheck = combat["wallcheck"]; // Added wallcheck
                if (combat.contains("teamcheck_flag")) (*globals::combat::flags)[0] = combat["teamcheck_flag"];
                if (combat.contains("knockcheck_flag")) (*globals::combat::flags)[1] = combat["knockcheck_flag"];
                if (combat.contains("rangecheck_flag")) (*globals::combat::flags)[2] = combat["rangecheck_flag"];
                if (combat.contains("wallcheck_flag")) (*globals::combat::flags)[3] = combat["wallcheck_flag"];
                if (combat.contains("range")) globals::combat::range = combat["range"];
                if (combat.contains("aim_distance")) globals::combat::aim_distance = combat["aim_distance"]; // Added aim_distance
                if (combat.contains("healththreshhold")) globals::combat::healththreshhold = combat["healththreshhold"];
                if (combat.contains("aimpart")) *globals::combat::aimpart = combat["aimpart"].get<std::vector<int>>();
                if (combat.contains("airaimpart")) *globals::combat::airaimpart = combat["airaimpart"].get<std::vector<int>>();
                if (combat.contains("silentaimpart")) *globals::combat::silentaimpart = combat["silentaimpart"].get<std::vector<int>>();
                if (combat.contains("airsilentaimpart")) *globals::combat::airsilentaimpart = combat["airsilentaimpart"].get<std::vector<int>>();
                if (combat.contains("silentaim")) globals::combat::silentaim = combat["silentaim"];
                if (combat.contains("stickyaimsilent")) globals::combat::stickyaimsilent = combat["stickyaimsilent"];
                if (combat.contains("silentaimtype")) globals::combat::silentaimtype = combat["silentaimtype"];
                if (combat.contains("silentaimfov")) globals::combat::silentaimfov = combat["silentaimfov"];
                if (combat.contains("drawsilentaimfov")) globals::combat::drawsilentaimfov = combat["drawsilentaimfov"];
                if (combat.contains("silentaimfovsize")) globals::combat::silentaimfovsize = combat["silentaimfovsize"];
                if (combat.contains("glowsilentaimfov")) globals::combat::glowsilentaimfov = combat["glowsilentaimfov"];
                if (combat.contains("silentaimfovcolor") && combat["silentaimfovcolor"].is_array() && combat["silentaimfovcolor"].size() == 4) {
                    for (int i = 0; i < 4; ++i) globals::combat::silentaimfovcolor[i] = combat["silentaimfovcolor"][i].get<float>();
                }
                if (combat.contains("silentaimfovtransparency")) globals::combat::silentaimfovtransparency = combat["silentaimfovtransparency"];
                if (combat.contains("silentaimfovfill")) globals::combat::silentaimfovfill = combat["silentaimfovfill"];
                if (combat.contains("silentaimfovfilltransparency")) {
                    globals::combat::silentaimfovfilltransparency = combat["silentaimfovfilltransparency"];
                    globals::combat::silentaimfovfilltransparency = std::clamp(globals::combat::silentaimfovfilltransparency, 0.0f, 1.0f);
                }
                if (combat.contains("silentaimfovfillcolor") && combat["silentaimfovfillcolor"].is_array() && combat["silentaimfovfillcolor"].size() == 4) {
                    for (int i = 0; i < 4; ++i) globals::combat::silentaimfovfillcolor[i] = combat["silentaimfovfillcolor"][i].get<float>();
                }
                if (combat.contains("silentaimfovglowcolor") && combat["silentaimfovglowcolor"].is_array() && combat["silentaimfovglowcolor"].size() == 4) {
                    for (int i = 0; i < 4; ++i) globals::combat::silentaimfovglowcolor[i] = combat["silentaimfovglowcolor"][i].get<float>();
                }
                if (combat.contains("silentaimfovshape")) globals::combat::silentaimfovshape = combat["silentaimfovshape"];
                if (combat.contains("silentaimfovthickness")) globals::combat::silentaimfovthickness = combat["silentaimfovthickness"];
                if (combat.contains("spin_fov_aimbot")) globals::combat::spin_fov_aimbot = combat["spin_fov_aimbot"];
                if (combat.contains("spin_fov_aimbot_speed")) globals::combat::spin_fov_aimbot_speed = combat["spin_fov_aimbot_speed"];
                if (combat.contains("spin_fov_silentaim")) globals::combat::spin_fov_silentaim = combat["spin_fov_silentaim"];
                if (combat.contains("spin_fov_silentaim_speed")) globals::combat::spin_fov_silentaim_speed = combat["spin_fov_silentaim_speed"];
                if (combat.contains("orbit")) globals::combat::orbit = combat["orbit"];
                if (combat.contains("orbitspeed")) globals::combat::orbitspeed = combat["orbitspeed"];
                if (combat.contains("orbitrange")) globals::combat::orbitrange = combat["orbitrange"];
                if (combat.contains("orbitheight")) globals::combat::orbitheight = combat["orbitheight"];
                if (combat.contains("drawradiusring")) globals::combat::drawradiusring = combat["drawradiusring"];
                if (combat.contains("orbittype")) globals::combat::orbittype = combat["orbittype"].get<std::vector<int>>();
                if (combat.contains("antiaim")) globals::combat::antiaim = combat["antiaim"];
                if (combat.contains("underground_antiaim")) globals::combat::underground_antiaim = combat["underground_antiaim"];
                if (combat.contains("triggerbot")) globals::combat::triggerbot = combat["triggerbot"];
                if (combat.contains("triggerbot_delay")) globals::combat::triggerbot_delay = combat["triggerbot_delay"];
                if (combat.contains("triggerbot_item_checks") && combat["triggerbot_item_checks"].is_array()) {
                    auto& arr = combat["triggerbot_item_checks"];
                    for (size_t i = 0; i < globals::combat::triggerbot_item_checks.size() && i < arr.size(); ++i) {
                        globals::combat::triggerbot_item_checks[i] = arr[i].get<int>() != 0;
                    }
                }

                                if (combat.contains("keybinds")) {
                    std::cout << "[COMBAT][KEYBIND] Loading combat keybinds..." << "\n";
                    auto& keybinds = combat["keybinds"];
                    if (keybinds.contains("aimbotkeybind")) {
                        if (keybinds["aimbotkeybind"].contains("key"))
                            globals::combat::aimbotkeybind.key = keybinds["aimbotkeybind"]["key"];
                        if (keybinds["aimbotkeybind"].contains("type"))
                            globals::combat::aimbotkeybind.type = static_cast<keybind::c_keybind_type>(keybinds["aimbotkeybind"]["type"].get<int>());
                    }
                    if (keybinds.contains("silentaimkeybind")) {
                        if (keybinds["silentaimkeybind"].contains("key"))
                            globals::combat::silentaimkeybind.key = keybinds["silentaimkeybind"]["key"];
                        if (keybinds["silentaimkeybind"].contains("type"))
                            globals::combat::silentaimkeybind.type = static_cast<keybind::c_keybind_type>(keybinds["silentaimkeybind"]["type"].get<int>());
                    }
                    if (keybinds.contains("orbitkeybind")) {
                        if (keybinds["orbitkeybind"].contains("key"))
                            globals::combat::orbitkeybind.key = keybinds["orbitkeybind"]["key"];
                        if (keybinds["orbitkeybind"].contains("type"))
                            globals::combat::orbitkeybind.type = static_cast<keybind::c_keybind_type>(keybinds["orbitkeybind"]["type"].get<int>());
                    }
                    if (keybinds.contains("locktargetkeybind")) {
                        if (keybinds["locktargetkeybind"].contains("key"))
                            globals::combat::locktargetkeybind.key = keybinds["locktargetkeybind"]["key"];
                        if (keybinds["locktargetkeybind"].contains("type"))
                            globals::combat::locktargetkeybind.type = static_cast<keybind::c_keybind_type>(keybinds["locktargetkeybind"]["type"].get<int>());
                    }
                    if (keybinds.contains("triggerbotkeybind")) {
                        if (keybinds["triggerbotkeybind"].contains("key"))
                            globals::combat::triggerbotkeybind.key = keybinds["triggerbotkeybind"]["key"];
                        if (keybinds["triggerbotkeybind"].contains("type"))
                            globals::combat::triggerbotkeybind.type = static_cast<keybind::c_keybind_type>(keybinds["triggerbotkeybind"]["type"].get<int>());
                    }
                    std::cout << "[COMBAT][KEYBIND] Combat keybinds loaded successfully" << "\n";
                }
            }

                        // Reset visuals toggles that should default off if missing
                        globals::visuals::sonar = false;
                        globals::visuals::sonar_detect_players = false;

                        if (config_json.contains("visuals")) {
                std::cout << "[VISUALS] Loading visual settings..." << "\n";
                auto& visuals = config_json["visuals"];
                if (visuals.contains("visuals")) globals::visuals::visuals = visuals["visuals"];
                if (visuals.contains("boxes")) globals::visuals::boxes = visuals["boxes"];
                if (visuals.contains("boxfill")) globals::visuals::boxfill = visuals["boxfill"];
                if (visuals.contains("lockedindicator")) globals::visuals::lockedindicator = visuals["lockedindicator"];
                if (visuals.contains("oofarrows")) globals::visuals::oofarrows = visuals["oofarrows"];
                if (visuals.contains("snapline")) globals::visuals::snapline = visuals["snapline"];
                if (visuals.contains("snaplinetype")) globals::visuals::snaplinetype = visuals["snaplinetype"];
                if (visuals.contains("snaplineoverlaytype")) globals::visuals::snaplineoverlaytype = visuals["snaplineoverlaytype"];
                if (visuals.contains("glowesp")) globals::visuals::glowesp = visuals["glowesp"];
                if (visuals.contains("boxtype")) globals::visuals::boxtype = visuals["boxtype"];
                if (visuals.contains("health")) globals::visuals::health = visuals["health"];
                if (visuals.contains("healthbar")) globals::visuals::healthbar = visuals["healthbar"];
                if (visuals.contains("name")) globals::visuals::name = visuals["name"];
                if (visuals.contains("nametype")) globals::visuals::nametype = visuals["nametype"];
                if (visuals.contains("toolesp")) globals::visuals::toolesp = visuals["toolesp"];
                if (visuals.contains("distance")) globals::visuals::distance = visuals["distance"];
                if (visuals.contains("teamcheck_visual_flag")) (*globals::visuals::visuals_flags)[0] = visuals["teamcheck_visual_flag"];
                if (visuals.contains("knockcheck_visual_flag")) (*globals::visuals::visuals_flags)[1] = visuals["knockcheck_visual_flag"];
                if (visuals.contains("distance_visual_flag")) (*globals::visuals::visuals_flags)[2] = visuals["distance_visual_flag"];
                if (visuals.contains("wallcheck_visual_flag")) (*globals::visuals::visuals_flags)[3] = visuals["wallcheck_visual_flag"];
                if (visuals.contains("chams")) globals::visuals::chams = visuals["chams"];
                if (visuals.contains("skeletons")) globals::visuals::skeletons = visuals["skeletons"];
                if (visuals.contains("localplayer")) globals::visuals::localplayer = visuals["localplayer"];
                if (visuals.contains("aimviewer")) globals::visuals::aimviewer = visuals["aimviewer"];
                if (visuals.contains("esppreview")) globals::visuals::esppreview = visuals["esppreview"];
                if (visuals.contains("predictionsdot")) globals::visuals::predictionsdot = visuals["predictionsdot"];
                if (visuals.contains("esp_preview_mode")) globals::visuals::esp_preview_mode = visuals["esp_preview_mode"];
                if (visuals.contains("boxcolors")) {
                    auto colors = visuals["boxcolors"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::boxcolors[i] = colors[i];
                }
                if (visuals.contains("boxfillcolor")) {
                    auto colors = visuals["boxfillcolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::boxfillcolor[i] = colors[i];
                }
                if (visuals.contains("glowcolor")) {
                    auto colors = visuals["glowcolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::glowcolor[i] = colors[i];
                }
                if (visuals.contains("lockedcolor")) {
                    auto colors = visuals["lockedcolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::lockedcolor[i] = colors[i];
                }
                if (visuals.contains("oofcolor")) {
                    auto colors = visuals["oofcolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::oofcolor[i] = colors[i];
                }
                if (visuals.contains("snaplinecolor")) {
                    auto colors = visuals["snaplinecolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::snaplinecolor[i] = colors[i];
                }
                if (visuals.contains("healthbarcolor")) {
                    auto colors = visuals["healthbarcolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::healthbarcolor[i] = colors[i];
                }
                if (visuals.contains("healthbarcolor1")) {
                    auto colors = visuals["healthbarcolor1"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::healthbarcolor1[i] = colors[i];
                }
                if (visuals.contains("namecolor")) {
                    auto colors = visuals["namecolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::namecolor[i] = colors[i];
                }
                if (visuals.contains("toolespcolor")) {
                    auto colors = visuals["toolespcolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::toolespcolor[i] = colors[i];
                }
                if (visuals.contains("distancecolor")) {
                    auto colors = visuals["distancecolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::distancecolor[i] = colors[i];
                }
                if (visuals.contains("chamscolor")) {
                    auto colors = visuals["chamscolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::chamscolor[i] = colors[i];
                }
                if (visuals.contains("skeletonscolor")) {
                    auto colors = visuals["skeletonscolor"].get<std::vector<float>>();
                    for (int i = 0; i < 4 && i < colors.size(); i++) globals::visuals::skeletonscolor[i] = colors[i];
                }
                if (visuals.contains("fortniteindicator")) globals::visuals::fortniteindicator = visuals["fortniteindicator"];
                if (visuals.contains("hittracer")) globals::visuals::hittracer = visuals["hittracer"];
                if (visuals.contains("trail")) globals::visuals::trail = visuals["trail"];
                if (visuals.contains("hitbubble")) globals::visuals::hitbubble = visuals["hitbubble"];
                if (visuals.contains("targetchams")) globals::visuals::targetchams = visuals["targetchams"];
                if (visuals.contains("targetskeleton")) globals::visuals::targetskeleton = visuals["targetskeleton"];
                if (visuals.contains("localplayerchams")) globals::visuals::localplayerchams = visuals["localplayerchams"];
                if (visuals.contains("localgunchams")) globals::visuals::localgunchams = visuals["localgunchams"];
                if (visuals.contains("enemycheck")) globals::visuals::enemycheck = visuals["enemycheck"];
                if (visuals.contains("friendlycheck")) globals::visuals::friendlycheck = visuals["friendlycheck"];
                if (visuals.contains("teamcheck")) globals::visuals::teamcheck = visuals["teamcheck"];
                if (visuals.contains("rangecheck")) globals::visuals::rangecheck = visuals["rangecheck"];
                if (visuals.contains("range")) globals::visuals::range = visuals["range"];
                if (visuals.contains("visual_range")) globals::visuals::visual_range = visuals["visual_range"];
                if (visuals.contains("sonar")) globals::visuals::sonar = visuals["sonar"];
                if (visuals.contains("sonar_range")) globals::visuals::sonar_range = visuals["sonar_range"];
                if (visuals.contains("sonar_thickness")) globals::visuals::sonar_thickness = visuals["sonar_thickness"];
                if (visuals.contains("sonar_speed")) globals::visuals::sonar_speed = visuals["sonar_speed"];
                if (visuals.contains("sonarcolor") && visuals["sonarcolor"].is_array() && visuals["sonarcolor"].size() == 4) {
                    for (int i = 0; i < 4; ++i) globals::visuals::sonarcolor[i] = visuals["sonarcolor"][i].get<float>();
                }
                if (visuals.contains("sonar_detect_players")) globals::visuals::sonar_detect_players = visuals["sonar_detect_players"];
                if (visuals.contains("sonar_detect_color_out") && visuals["sonar_detect_color_out"].is_array() && visuals["sonar_detect_color_out"].size() == 4) {
                    for (int i = 0; i < 4; ++i) globals::visuals::sonar_detect_color_out[i] = visuals["sonar_detect_color_out"][i].get<float>();
                }
                if (visuals.contains("sonar_detect_color_in") && visuals["sonar_detect_color_in"].is_array() && visuals["sonar_detect_color_in"].size() == 4) {
                    for (int i = 0; i < 4; ++i) globals::visuals::sonar_detect_color_in[i] = visuals["sonar_detect_color_in"][i].get<float>();
                }
                if (visuals.contains("box_overlay_flags")) {
                    *globals::visuals::box_overlay_flags = visuals["box_overlay_flags"].get<std::vector<int>>();
                    if (globals::visuals::box_overlay_flags->size() < 3)
                        globals::visuals::box_overlay_flags->resize(3, 0);
                }
                if (visuals.contains("name_overlay_flags")) *globals::visuals::name_overlay_flags = visuals["name_overlay_flags"].get<std::vector<int>>();
                if (visuals.contains("healthbar_overlay_flags")) {
                    *globals::visuals::healthbar_overlay_flags = visuals["healthbar_overlay_flags"].get<std::vector<int>>();
                    if (globals::visuals::healthbar_overlay_flags->size() < 2) {
                        globals::visuals::healthbar_overlay_flags->resize(2, 0);
                    } else if (globals::visuals::healthbar_overlay_flags->size() > 2) {
                        globals::visuals::healthbar_overlay_flags->resize(2);
                    }
                } else {
                    globals::visuals::healthbar_overlay_flags->assign({ 0, 0 });
                }
                if (visuals.contains("tool_overlay_flags")) *globals::visuals::tool_overlay_flags = visuals["tool_overlay_flags"].get<std::vector<int>>();
                if (visuals.contains("distance_overlay_flags")) *globals::visuals::distance_overlay_flags = visuals["distance_overlay_flags"].get<std::vector<int>>();
                if (visuals.contains("skeleton_overlay_flags")) *globals::visuals::skeleton_overlay_flags = visuals["skeleton_overlay_flags"].get<std::vector<int>>();
                if (visuals.contains("chams_overlay_flags")) *globals::visuals::chams_overlay_flags = visuals["chams_overlay_flags"].get<std::vector<int>>();
                if (visuals.contains("snapline_overlay_flags")) *globals::visuals::snapline_overlay_flags = visuals["snapline_overlay_flags"].get<std::vector<int>>();
                if (visuals.contains("oof_overlay_flags")) *globals::visuals::oof_overlay_flags = visuals["oof_overlay_flags"].get<std::vector<int>>();
                if (visuals.contains("target_only_esp")) globals::visuals::target_only_esp = visuals["target_only_esp"];
                if (visuals.contains("target_only_list")) globals::visuals::target_only_list = visuals["target_only_list"].get<std::vector<std::string>>();
                if (visuals.contains("fog_changer")) globals::visuals::fog_changer = visuals["fog_changer"]; // Added fog_changer
                if (visuals.contains("fog_start")) globals::visuals::fog_start = visuals["fog_start"]; // Added fog_start
                if (visuals.contains("fog_end")) globals::visuals::fog_end = visuals["fog_end"]; // Added fog_end
                if (visuals.contains("fog_color") && visuals["fog_color"].is_array() && visuals["fog_color"].size() == 4) {
                    for (int i = 0; i < 4; ++i) globals::visuals::fog_color[i] = visuals["fog_color"][i].get<float>();
                }
                if (visuals.contains("lockedesp")) globals::visuals::lockedesp = visuals["lockedesp"]; // Added lockedesp
                if (visuals.contains("lockedespcolor") && visuals["lockedespcolor"].is_array() && visuals["lockedespcolor"].size() == 4) {
                    for (int i = 0; i < 4; ++i) globals::visuals::lockedespcolor[i] = visuals["lockedespcolor"][i].get<float>();
                }
                if (visuals.contains("healthbar")) globals::visuals::healthbar = visuals["healthbar"]; // Added healthbar
            }

                        if (config_json.contains("misc")) {
                std::cout << "[MISC] Loading misc settings..." << "\n";
                auto& misc = config_json["misc"];
                if (misc.contains("menuglowcolor") && misc["menuglowcolor"].is_array() && misc["menuglowcolor"].size() == 4) {
                    for (int i = 0; i < 4; ++i) globals::misc::menuglowcolor[i] = misc["menuglowcolor"][i].get<float>();
                }
                if (misc.contains("overlay_color") && misc["overlay_color"].is_array() && misc["overlay_color"].size() == 4) {
                    for (int i = 0; i < 4; ++i) globals::misc::overlay_color[i] = misc["overlay_color"][i].get<float>();
                }
                if (misc.contains("accent_color") && misc["accent_color"].is_array() && misc["accent_color"].size() == 4) {
                    for (int i = 0; i < 4; ++i) globals::misc::accent_color[i] = misc["accent_color"][i].get<float>();
                }
                if (misc.contains("overlay_star_color") && misc["overlay_star_color"].is_array() && misc["overlay_star_color"].size() == 4) {
                    for (int i = 0; i < 4; ++i) globals::misc::overlay_star_color[i] = misc["overlay_star_color"][i].get<float>();
                }
                if (misc.contains("health_full_color") && misc["health_full_color"].is_array() && misc["health_full_color"].size() == 4) {
                    for (int i = 0; i < 4; ++i) globals::misc::health_full_color[i] = misc["health_full_color"][i].get<float>();
                }
                if (misc.contains("health_low_color") && misc["health_low_color"].is_array() && misc["health_low_color"].size() == 4) {
                    for (int i = 0; i < 4; ++i) globals::misc::health_low_color[i] = misc["health_low_color"][i].get<float>();
                }
                if (misc.contains("show_theme_window")) globals::misc::show_theme_window = misc["show_theme_window"];
                if (misc.contains("speed")) globals::misc::speed = misc["speed"];
                if (misc.contains("speedtype")) globals::misc::speedtype = misc["speedtype"];
                if (misc.contains("speedvalue")) globals::misc::speedvalue = misc["speedvalue"];
                if (misc.contains("nojumpcooldown")) globals::misc::nojumpcooldown = misc["nojumpcooldown"];
                if (misc.contains("jumppower")) globals::misc::jumppower = misc["jumppower"];
                if (misc.contains("jumppowervalue")) globals::misc::jumppowervalue = misc["jumppowervalue"];
                if (misc.contains("flight")) globals::misc::flight = misc["flight"];
                if (misc.contains("flighttype")) globals::misc::flighttype = misc["flighttype"];
                if (misc.contains("flightvalue")) globals::misc::flightvalue = misc["flightvalue"];
                if (misc.contains("hipheight")) globals::misc::hipheight = misc["hipheight"];
                if (misc.contains("hipheightvalue")) globals::misc::hipheightvalue = misc["hipheightvalue"];
                if (misc.contains("rapidfire")) globals::misc::rapidfire = misc["rapidfire"];
                if (misc.contains("autoarmor")) globals::misc::autoarmor = misc["autoarmor"];
                if (misc.contains("autoreload")) globals::misc::autoreload = misc["autoreload"];
                if (misc.contains("autostomp")) globals::misc::autostomp = misc["autostomp"];
                if (misc.contains("antistomp")) globals::misc::antistomp = misc["antistomp"];
                if (misc.contains("bikefly")) globals::misc::bikefly = misc["bikefly"];
                if (misc.contains("spectate")) globals::misc::spectate = misc["spectate"];
                if (misc.contains("vsync")) globals::misc::vsync = misc["vsync"];
                if (misc.contains("watermark")) globals::misc::watermark = misc["watermark"];
                if (misc.contains("watermarkstuff")) *globals::misc::watermarkstuff = misc["watermarkstuff"].get<std::vector<int>>();
                if (misc.contains("targethud")) globals::misc::targethud = misc["targethud"];
                if (misc.contains("playerlist")) globals::misc::playerlist = misc["playerlist"];
                if (misc.contains("keybindsstyle")) globals::misc::keybindsstyle = misc["keybindsstyle"];
                if (misc.contains("keybinds")) globals::misc::keybinds = misc["keybinds"];
                if (misc.contains("spotify")) globals::misc::spotify = misc["spotify"];
                if (misc.contains("explorer")) globals::misc::explorer = misc["explorer"];
                if (misc.contains("colors")) globals::misc::colors = misc["colors"];
                if (misc.contains("streamproof")) globals::misc::streamproof = misc["streamproof"];
                if (misc.contains("custom_cursor")) globals::misc::custom_cursor = misc["custom_cursor"];
                if (misc.contains("rotate360")) globals::misc::rotate360 = misc["rotate360"];
                if (misc.contains("rotate360_speed")) globals::misc::rotate360_speed = misc["rotate360_speed"];
                if (misc.contains("camera_rotation_yaw")) globals::misc::camera_rotation_yaw = misc["camera_rotation_yaw"];
                if (misc.contains("menuglow")) globals::misc::menuglow = misc["menuglow"];
                if (misc.contains("overlay_stars")) globals::misc::overlay_stars = misc["overlay_stars"];

                // Load misc keybinds
                if (misc.contains("keybinds_data")) {
                    std::cout << "[MISC][KEYBIND] Loading misc keybinds..." << "\n";
                    auto& keybinds = misc["keybinds_data"];
                    if (keybinds.contains("speedkeybind")) {
                        if (keybinds["speedkeybind"].contains("key"))
                            globals::misc::speedkeybind.key = keybinds["speedkeybind"]["key"];
                        if (keybinds["speedkeybind"].contains("type"))
                            globals::misc::speedkeybind.type = static_cast<keybind::c_keybind_type>(keybinds["speedkeybind"]["type"].get<int>());
                    }
                    if (keybinds.contains("jumppowerkeybind")) {
                        if (keybinds["jumppowerkeybind"].contains("key"))
                            globals::misc::jumppowerkeybind.key = keybinds["jumppowerkeybind"]["key"];
                        if (keybinds["jumppowerkeybind"].contains("type"))
                            globals::misc::jumppowerkeybind.type = static_cast<keybind::c_keybind_type>(keybinds["jumppowerkeybind"]["type"].get<int>());
                    }
                    if (keybinds.contains("flightkeybind")) {
                        if (keybinds["flightkeybind"].contains("key"))
                            globals::misc::flightkeybind.key = keybinds["flightkeybind"]["key"];
                        if (keybinds["flightkeybind"].contains("type"))
                            globals::misc::flightkeybind.type = static_cast<keybind::c_keybind_type>(keybinds["flightkeybind"]["type"].get<int>());
                    }
                    if (keybinds.contains("stompkeybind")) {
                        if (keybinds["stompkeybind"].contains("key"))
                            globals::misc::stompkeybind.key = keybinds["stompkeybind"]["key"];
                        if (keybinds["stompkeybind"].contains("type"))
                            globals::misc::stompkeybind.type = static_cast<keybind::c_keybind_type>(keybinds["stompkeybind"]["type"].get<int>());
                    }
                    if (keybinds.contains("spectatebind")) {
                        if (keybinds["spectatebind"].contains("key"))
                            globals::misc::spectatebind.key = keybinds["spectatebind"]["key"];
                        if (keybinds["spectatebind"].contains("type"))
                            globals::misc::spectatebind.type = static_cast<keybind::c_keybind_type>(keybinds["spectatebind"]["type"].get<int>());
                    }
                    if (keybinds.contains("rotate360keybind")) {
                        if (keybinds["rotate360keybind"].contains("key"))
                            globals::misc::rotate360keybind.key = keybinds["rotate360keybind"]["key"];
                        if (keybinds["rotate360keybind"].contains("type"))
                            globals::misc::rotate360keybind.type = static_cast<keybind::c_keybind_type>(keybinds["rotate360keybind"]["type"].get<int>());
                    }
                    
                    // Load menu_hotkey
                    if (keybinds.contains("menu_hotkey")) {
                        if (keybinds["menu_hotkey"].contains("key"))
                            globals::misc::menu_hotkey.key = keybinds["menu_hotkey"]["key"];
                        if (keybinds["menu_hotkey"].contains("type"))
                            globals::misc::menu_hotkey.type = static_cast<keybind::c_keybind_type>(keybinds["menu_hotkey"]["type"].get<int>());
                    }
                    std::cout << "[MISC][KEYBIND] Misc keybinds loaded successfully" << "\n";
                }
            }

            file.close();
            current_config_name = sanitized_name;
            globals::misc::config_loaded_flag = true; // Set flag after successful config load
            ImAdd::ClearWidgetStates(); // Clear static states of ImGui widgets
            std::cout << "[CONFIG] Successfully loaded config: " << name << "\n";
            return true;
        }
        catch (const json::parse_error& e) {
            std::cout << "[CONFIG] Failed to parse config file: " << e.what() << "\n";
            file.close();
            return false;
        }
        catch (...) {
            std::cout << "[CONFIG] Failed to load config (unknown error): " << name << "\n";
            file.close();
            return false;
        }
    }

    std::cout << "[CONFIG] Failed to open config file: " << name << "\n";
    return false;
}

bool ConfigSystem::delete_config(const std::string& name) {
    if (name.empty()) return false;

    std::string sanitized_name = sanitize_filename(name);
    if (sanitized_name.empty()) return false;

    std::string filepath = config_directory + "\\" + sanitized_name + ".json";

    if (fs::exists(filepath)) {
        fs::remove(filepath);
        refresh_config_list();

        if (current_config_name == name) {
            current_config_name.clear();
        }

        std::cout << "[CONFIG] Successfully deleted config: " << name << "\n";
        return true;
    }

    std::cout << "[CONFIG] Config file not found: " << name << "\n";
    return false;
}

bool ConfigSystem::rename_config(const std::string& old_name, const std::string& new_name) {
    if (old_name.empty() || new_name.empty()) return false;

    std::string sanitized_old = sanitize_filename(old_name);
    std::string sanitized_new = sanitize_filename(new_name);
    if (sanitized_old.empty() || sanitized_new.empty()) return false;
    if (sanitized_old == sanitized_new) return false;

    std::string old_path = config_directory + "\\" + sanitized_old + ".json";
    std::string new_path = config_directory + "\\" + sanitized_new + ".json";

    if (!fs::exists(old_path) || fs::exists(new_path)) {
        return false;
    }

    try {
        fs::rename(old_path, new_path);
    }
    catch (...) {
        return false;
    }

    refresh_config_list();

    if (current_config_name == old_name || current_config_name == sanitized_old) {
        current_config_name = sanitized_new;
    }

    if (autoload_config_name == old_name || autoload_config_name == sanitized_old) {
        autoload_config_name = sanitized_new;
        save_autoload_setting(autoload_config_name);
    }

    std::cout << "[CONFIG] Renamed config from " << old_name << " to " << new_name << "\n";
    return true;
}

    void ConfigSystem::render_config_ui(float width, float height) {
        bool child_open = ImAdd::BeginChild("CONFIG SYSTEM", ImVec2(width, 0)); // Set height to 0 to auto-size and disable scrolling
        if (child_open)
        {
            ImGui::Text("Config Name:");
            ImGui::SetNextItemWidth(-1);
            
            // Get current window and draw list
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
            float item_width = ImGui::GetContentRegionAvail().x;
            float item_height = ImGui::GetFrameHeight();
            ImRect input_bb(cursor_pos, cursor_pos + ImVec2(item_width, item_height));

            // Draw a rectangle around the InputText field
            draw_list->AddRect(input_bb.Min, input_bb.Max, ImGui::GetColorU32(ImGuiCol_Border), ImGui::GetStyle().FrameRounding, 0, 1.0f);

            ImGui::InputText("##config_name", config_name_buffer, sizeof(config_name_buffer));
            ImGui::Spacing();

            float button_width = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3;

            if (ImAdd::Button("Save", ImVec2(button_width, 20))) {
                std::cout << "[CONFIG] Save button pressed" << "\n";
                if (strlen(config_name_buffer) > 0) {
                    if (save_config(std::string(config_name_buffer))) {
                        Notifications::Success("Config '" + std::string(config_name_buffer) + "' saved successfully!");
                        memset(config_name_buffer, 0, sizeof(config_name_buffer));
                    }
                    else {
                        Notifications::Error("Failed to save config '" + std::string(config_name_buffer) + "'!");
                    }
                }
                else {
                    Notifications::Warning("Please enter a config name!");
                }
            }

            ImGui::SameLine();

            if (ImAdd::Button("Rename", ImVec2(button_width, 20))) {
                if (strlen(config_name_buffer) == 0) {
                    Notifications::Warning("Select a config to rename!");
                }
                else {
                    std::string new_name = std::string(config_name_buffer);
                    // Find the config in the list to get the old name
                    for (const auto& config : config_files) {
                        if (config == current_config_name) {
                            if (rename_config(current_config_name, new_name)) {
                                Notifications::Success("Renamed to '" + new_name + "'");
                                strcpy_s(config_name_buffer, sizeof(config_name_buffer), new_name.c_str());
                            }
                            else {
                                Notifications::Error("Failed to rename config");
                            }
                            break;
                        }
                    }
                }
            }

            ImGui::SameLine();

            if (ImAdd::Button("Delete", ImVec2(button_width, 20))) {
                std::cout << "[CONFIG] Delete button pressed" << "\n";
                if (strlen(config_name_buffer) > 0) {
                    std::string config_name = std::string(config_name_buffer);
                    if (delete_config(config_name)) {
                        Notifications::Success("Config was deleted!");
                        memset(config_name_buffer, 0, sizeof(config_name_buffer));
                    }
                    else {
                        Notifications::Error("Failed To Delete!");
                    }
                }
                else {
                    Notifications::Warning("Select A Config!");
                }
            }
            ImGui::Spacing();

            if (!config_files.empty()) {
                ImGui::Text("Available Configs:");

                for (const auto& config : config_files) {
                    bool is_selected = (current_config_name == config);

                    if (ImGui::Selectable(config.c_str(), is_selected)) {
                        std::cout << "[CONFIG] Selected config: " << config << "\n";
                        strcpy_s(config_name_buffer, sizeof(config_name_buffer), config.c_str());
                        current_config_name = config;
                        Notifications::Info("Selected config: " + config, 2.0f);
                    }
                    
                    // Handle double click to load
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                        if (load_config(config)) {
                            Notifications::Success("Config '" + config + "' loaded successfully!");
                            current_config_name = config;
                        }
                        else {
                            Notifications::Error("Failed to load config '" + config + "'!");
                        }
                    }

                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }

            }
            else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No configs found");

                ImGui::Spacing();
                ImGui::TextWrapped("Create your first config by entering a name above and clicking 'Save Config'.");
            }

            ImGui::Spacing();
            // Removed the separate rename input field and button

            // Calculate height of the elements to be pushed to the bottom
            float bottom_section_height = 0.0f;
            bottom_section_height += ImGui::GetStyle().ItemSpacing.y * 2; // Spacing before Autoload section and after config list
            bottom_section_height += ImGui::GetTextLineHeight(); // "Autoload Config:" text
            bottom_section_height += ImGui::GetFrameHeight(); // Autoload Combo
            bottom_section_height += ImGui::GetStyle().ItemSpacing.y; // Spacing after Autoload Combo
            bottom_section_height += 20.0f; // Detach button height
            bottom_section_height += ImGui::GetStyle().ItemSpacing.y; // Spacing between Detach and Refresh
            bottom_section_height += 20.0f; // Refresh button height
            bottom_section_height += ImGui::GetStyle().ItemSpacing.y; // Spacing between Refresh and Themes
            bottom_section_height += 20.0f; // Themes button height

            // Calculate the remaining vertical space and add a dummy element
            float available_height = ImGui::GetContentRegionAvail().y;
            float dummy_height = available_height - bottom_section_height - 20.0f; // Extra padding for safety

            if (dummy_height > 0) {
                ImGui::Dummy(ImVec2(-1, dummy_height));
            }

            ImGui::Spacing(); // Spacing before Autoload section

            // Autoload selection
            ImGui::Text("Autoload Config:");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##autoload_combo", autoload_config_name.empty() ? "None" : autoload_config_name.c_str())) {
                bool is_none_selected = autoload_config_name.empty();
                if (ImGui::Selectable("None", is_none_selected)) {
                    save_autoload_setting("");
                }
                if (is_none_selected) {
                    ImGui::SetItemDefaultFocus();
                }

                for (const auto& config : config_files) {
                    bool is_selected = (autoload_config_name == config);
                    if (ImGui::Selectable(config.c_str(), is_selected)) {
                        save_autoload_setting(config);
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::Spacing(); // Spacing after Autoload Combo, before buttons
            
            if (ImAdd::Button("Detach", ImVec2(-1, 20))) {
                exit(0);
            }

            if (ImAdd::Button("Refresh List", ImVec2(-1, 20))) {
                std::cout << "[CONFIG] Refreshing config list..." << "\n";
                size_t old_count = config_files.size();
                refresh_config_list();
                size_t new_count = config_files.size();

                if (new_count != old_count) {
                    Notifications::Info("Config list refreshed - Found " + std::to_string(new_count) + " configs", 2.0f);
                }
                else {
                    Notifications::Info("Config list refreshed", 1.5f);
                }
            }
            
            if (ImAdd::Button("Themes", ImVec2(-1, 20))) {
                globals::misc::show_theme_window = !globals::misc::show_theme_window;
            }
        }
        ImAdd::EndChild();
    }

const std::string& ConfigSystem::get_current_config() const {
    return current_config_name;
}
