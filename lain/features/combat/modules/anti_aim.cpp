#include "../combat.h"
#include "../../../util/console/console.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>
#include <thread>
#include <algorithm>
#include <random>
#include "../../hook.h"
#include <iostream>
#include <windows.h>
#include "../../../util/classes/math/math.h"

#pragma comment(lib, "winmm.lib")

void hooks::anti_aim() {
    HANDLE currentThread = GetCurrentThread();
    SetThreadPriority(currentThread, THREAD_PRIORITY_TIME_CRITICAL);
    SetThreadAffinityMask(currentThread, 1);

    static LARGE_INTEGER frequency;
    static LARGE_INTEGER lastTime;
    static bool timeInitialized = false;

    if (!timeInitialized) {
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&lastTime);
        timeBeginPeriod(1);
        timeInitialized = true;
    }

    const double targetFrameTime = 1.0 / 425.0;
    bool toggle = false;

    while (true) {
        roblox::instance playerservice = roblox::instance().read_service("Players");
        roblox::instance localplayer = playerservice.local_player();

        if (localplayer.is_valid()) {
            roblox::instance character = localplayer.findfirstchild("Character");  // change the logic for this if u wanna
            if (character.is_valid()) {
                roblox::instance hrp = character.findfirstchild("HumanoidRootPart");
                if (hrp.is_valid()) {
                    math::Vector3 currentpos = hrp.get_pos();

                    if (globals::combat::antiaim) {
                        float jitter = toggle ? 2.2f : -2.2f;
                        toggle = !toggle;

                        math::Vector3 newpos = {
                            currentpos.x + jitter,
                            currentpos.y + static_cast<float>((rand() % 3) - 1),
                            currentpos.z + jitter
                        };

                        hrp.write_position(newpos);
                    }
                    else if (globals::combat::underground_antiaim) {
                        float jitter = toggle ? -5.0f : 5.0f;
                        toggle = !toggle;

                        math::Vector3 newpos = {
                            currentpos.x + jitter,
                            currentpos.y - 50.0f + static_cast<float>((rand() % 3) - 1),  // underground
                            currentpos.z + jitter
                        };

                        hrp.write_position(newpos);
                    }
                }
            }
        }
    }

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
        SwitchToThread();
    }
