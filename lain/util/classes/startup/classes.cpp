#include "../classes.h"
#include <Windows.h>
#include "../../../util/driver/driver.h"
#include <vector>
#include "../../../util/offsets.h"
#include "../../protection/hider.h"
#include "../../protection/keyauth/skStr.h"
#include <thread>
#include "../../../drawing/overlay/overlay.h"
#include <chrono>
#include "../../globals.h"
#include "../../../features/hook.h"
#include "../../console/console.h"

Logger logger;

roblox::instance ReadVisualEngine() {
    return read<roblox::instance>(base_address + offsets::VisualEnginePointer);
}

roblox::instance ReadDatamodel() {
    uintptr_t padding = read<uintptr_t>(ReadVisualEngine().address + offsets::VisualEngineToDataModel1);
    return read<roblox::instance>(padding + offsets::VisualEngineToDataModel2);
}

bool engine::startup() {
    system("cls");
    logger.Banner();

    logger.Section("CORE INITIALIZATION");
    logger.CustomLog(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "ENGINE", "Starting Core Systems...");

    logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "DRIVER", "Initializing memory driver interface...");
    mem::initialize();

    logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "SCANNER", "Scanning for Roblox process...");
    std::cout << "GRABBING ROBLOX PID" << std::endl;
    std::cout << "CLOSING HANDLE" << std::endl;
    mem::grabroblox_h();
    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "SUCCESS", "Memory interface established");

    logger.Section("INSTANCE RESOLUTION");
    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, "RESOLVER", "Reading core Roblox instances...");

    roblox::instance visualengine = ReadVisualEngine();
    roblox::instance datamodel = ReadDatamodel();
    roblox::instance workspace = datamodel.read_workspace();
    roblox::instance players = datamodel.read_service("Players");
    roblox::instance localplayer = players.local_player();
    roblox::instance lighting = datamodel.read_service("Lighting");
    roblox::instance camera = workspace.read_service("Camera");


    logger.Section("ADDRESS VALIDATION");

    if (is_valid_address(visualengine.address)) {
        globals::instances::visualengine = visualengine;
        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "VALID", "VisualEngine    -> 0x%p", visualengine.address);
        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "VALID", "Base Address   -> 0x%p", base_address);
    }
    else {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "ERROR", "VisualEngine Address -> 0x0");
    }

    if (is_valid_address(datamodel.address)) {
        globals::instances::datamodel = datamodel;
        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "VALID", "DataModel      -> 0x%p", datamodel.address);
        std::string gameId = globals::instances::datamodel.get_game_id();
        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "VALID", "Game ID        -> %s", gameId.c_str());
    }
    else {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "ERROR", "DataModel Address -> 0x0");
    }

    if (is_valid_address(workspace.address)) {
        globals::instances::workspace = workspace;
        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "VALID", "Workspace      -> 0x%p", workspace.address);
    }
    else {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "ERROR", "Workspace Address -> 0x0");
    }

    if (is_valid_address(players.address)) {
        globals::instances::players = players;
        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "VALID", "Players        -> 0x%p", players.address);
    }
    else {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "ERROR", "Players Address -> 0x0");
    }

    if (is_valid_address(localplayer.address)) {
        globals::instances::localplayer = localplayer;
        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "VALID", "LocalPlayer    -> 0x%p", localplayer.address);
    }
    else {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "ERROR", "LocalPlayer Address -> 0x0");
    }

    if (is_valid_address(lighting.address)) {
        globals::instances::lighting = lighting;
        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "VALID", "Lighting       -> 0x%p", lighting.address);
    }
    else {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "ERROR", "Lighting Address -> 0x0");
    }

    if (is_valid_address(camera.address)) {
        globals::instances::camera.address = camera.address;
        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "VALID", "Camera         -> 0x%p", camera.address);
    }
    else {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "ERROR", "Camera Address -> 0x0");
    }
    
std::thread([] {
    while (true) {
        uint64_t place = read<uint64_t>(globals::instances::datamodel.address + offsets::PlaceId);
        globals::place_id = place;

        roblox::instance playerGui = globals::instances::datamodel
            .findfirstchild("Players")
            .findfirstchild(globals::instances::localplayer.get_name())
            .findfirstchild("PlayerGui");

        if (place == 9825515356ull) {
            globals::game = "hood customs";
            globals::instances::aim = playerGui
                .findfirstchild("Main Screen")
                .findfirstchild("Aim");
        }
        else if (place == 91222270589102) {
            globals::game = "zee hood";
            globals::instances::aim = playerGui
                .findfirstchild("MainScreenGui")
                .findfirstchild("Aim");
        } 
        else if (place == 138995385694035) {
            globals::game = "hood customs ffa";
            globals::instances::aim = playerGui
                .findfirstchild("Main Screen")
                .findfirstchild("Aim");
        }
        else if (place == 90724401598574) {
            globals::game = "da track ffa";
            globals::instances::aim = playerGui
                .findfirstchild("MainUI")
                .findfirstchild("Aim");
        }
        else if (place == 75159825516372) {
            globals::game = "da track";
            globals::instances::aim = playerGui
                .findfirstchild("Main")
                .findfirstchild("Aim");        
        }
        else if (place == 99148804220244) {
            globals::game = "model hood";
            globals::instances::aim = playerGui
                .findfirstchild("MainScreenGui")
                .findfirstchild("Aim");
        }
        else if (place == 80567999110374) {
            globals::game = "hood custom anarchy";
            globals::instances::aim = playerGui
                .findfirstchild("Main Screen")
                .findfirstchild("Aim");
        }
        else if (place == 89562160011863) {
            globals::game = "der hood";
            globals::instances::aim = playerGui
                .findfirstchild("MainScreenGui")
                .findfirstchild("Aim");
        }
        else {
            globals::game = "universal";
            globals::instances::aim = playerGui
                .findfirstchild("MainScreenGui")
                .findfirstchild("Aim");
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}).detach();

    logger.Section("THREAD INITIALIZATION");

    logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "OVERLAY", "Launching overlay interface...");
    std::thread overlay(overlay::load_interface);
    overlay.detach();
    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "SUCCESS", "Overlay thread launched");

    logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "CACHE", "Initializing player cache system...");
    player_cache playercache;
    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "SUCCESS", "Player cache initialized");

    logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "MONITOR", "Starting teleport detection thread...");
    std::thread PlayerCacheThread([&playercache]() {
        while (true)
        {
            if (globals::unattach) break;

            try {
                roblox::instance datamodel = ReadDatamodel();
                if (globals::instances::datamodel.address != datamodel.address) {
                    system("cls");
                    logger.Banner();
                    logger.Section("TELEPORT RECOVERY");
                    logger.CustomLog(FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY, "TELEPORT", "Game teleport detected! Emergency rescan initiated...");
                    globals::handlingtp = true;
                    
                    // Clear all cached targets to prevent auto-targeting in new game
                    globals::instances::cachedtarget = {};
                    globals::instances::cachedlasttarget = {};
                    globals::instances::cachedplayers.clear();
                    
                    // Set flag to prevent auto-targeting for a period after rescanning
                    globals::prevent_auto_targeting = true;

                    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, "RECOVERY", "Reinitializing driver connection...");
                    // Reinitialize driver if needed
                    mem::initialize();
                    mem::grabroblox_h();

                    // Reduced delay for faster recovery
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, "RECOVERY", "Resolving new instance addresses...");

                    // Attempt to read instances with retry logic
                    roblox::instance visualengine, datamodel, workspace, players, localplayer, lighting, camera;
                    int retry_count = 0;
                    const int max_retries = 3;
                    
                    do {
                        visualengine = ReadVisualEngine();
                        datamodel = ReadDatamodel();
                        
                        if (is_valid_address(datamodel.address)) {
                            workspace = datamodel.read_workspace();
                            players = datamodel.read_service("Players");
                            localplayer = players.local_player();
                            lighting = datamodel.read_service("Lighting");
                            camera = workspace.read_service("Camera");
                        }
                        
                        retry_count++;
                        if (retry_count < max_retries && (!is_valid_address(datamodel.address) || !is_valid_address(workspace.address))) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(200));
                        }
                    } while (retry_count < max_retries && (!is_valid_address(datamodel.address) || !is_valid_address(workspace.address)));
                    
                    playercache.Refresh();
                    globals::instances::localplayer.rescan_silent_mouse_service(); // Call the new rescan function
                    
                    // Clear any remaining cached data and reset combat state
                    globals::bools::player_status.clear();

                    logger.Section("RECOVERY VALIDATION");

                    if (is_valid_address(datamodel.address)) {
                        globals::instances::datamodel = datamodel;
                        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "RECOVERED", "DataModel      -> 0x%p", datamodel.address);
                    }
                    else {
                        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "FAILED", "DataModel recovery failed");
                    }

                    if (is_valid_address(workspace.address)) {
                        globals::instances::workspace = workspace;
                        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "RECOVERED", "Workspace      -> 0x%p", workspace.address);
                    }
                    else {
                        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "FAILED", "Workspace recovery failed");
                    }

                    if (is_valid_address(players.address)) {
                        globals::instances::players = players;
                        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "RECOVERED", "Players        -> 0x%p", players.address);
                    }
                    else {
                        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "FAILED", "Players recovery failed");
                    }

                    if (is_valid_address(localplayer.address)) {
                        globals::instances::localplayer = localplayer;
                        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "RECOVERED", "LocalPlayer    -> 0x%p", localplayer.address);
                    }
                    else {
                        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "FAILED", "LocalPlayer recovery failed");
                    }

                    if (is_valid_address(lighting.address)) {
                        globals::instances::lighting = lighting;
                        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "RECOVERED", "Lighting       -> 0x%p", lighting.address);
                    }
                    else {
                        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "FAILED", "Lighting recovery failed");
                    }

                    if (is_valid_address(camera.address)) {
                        globals::instances::camera.address = camera.address;
                        logger.LogfCustom(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "RECOVERED", "Camera         -> 0x%p", camera.address);
                    }
                    else {
                        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "FAILED", "Camera recovery failed");
                    }

                    logger.Success("TELEPORT RECOVERY COMPLETED SUCCESSFULLY");
                    
                    // Add a delay before re-enabling combat features to prevent auto-targeting
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                    globals::handlingtp = false;
                    
                    // Start a timer to reset the prevent_auto_targeting flag after 5 seconds
                    std::thread([]() {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                        globals::prevent_auto_targeting = false;
                    }).detach();
                }
                else {
                    playercache.UpdateCache();
                }
            }
            catch (...) {
                logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "ERROR", "Exception in monitor thread");
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        });
    PlayerCacheThread.detach();
    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "SUCCESS", "Monitor thread launched");

    logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "HOOKS", "Launching exploitation hooks...");
    try {
        hooking::launchThreads();

        logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "WAIT", "Waiting for all threads to initialize...");
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));

        logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "SUCCESS", "All hook threads launched and initialized");
    }
    catch (...) {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "ERROR", "Failed to launch hook threads");
    }


    logger.CustomLog(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "RUNTIME", "Engine running... use in-app control to terminate");
    logger.Separator();
    try {
        while (true) {
            if (globals::unattach) {
                logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, "SHUTDOWN", "Unattach signal received");
                break;
            }
            // Do not read std::cin for ENTER; console may be detached or buffered. Prevent accidental shutdown.
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    catch (...) {
        logger.CustomLog(FOREGROUND_RED | FOREGROUND_INTENSITY, "ERROR", "Exception in main loop");
    }

    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, "SHUTDOWN", "Initiating cleanup sequence...");
    globals::unattach = true;

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    logger.CustomLog(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "SHUTDOWN", "Engine terminated successfully");

    return true;
}

float roblox::instance::read_fogstart() {
    return read<float>(this->address + offsets::FogStart);
}

float roblox::instance::read_fogend() {
    return read<float>(this->address + offsets::FogEnd);
}

math::Vector3 roblox::instance::read_fogcolor() {
    return read<math::Vector3>(this->address + offsets::FogColor);
}
