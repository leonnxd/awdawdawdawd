#pragma once
#include <iostream>
#include <chrono>
#include <Windows.h>

class Logger {
public:
    static void GetTime(char(&timeBuffer)[9])
    {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        struct tm localTime;
        localtime_s(&localTime, &now_time);
        strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &localTime);
    }

    static void Log(const std::string& message) {
        char timeBuffer[9];
        GetTime(timeBuffer);

        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        WORD originalAttributes = csbi.wAttributes;

        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
        std::cout << "[" << timeBuffer << "] ";

        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        std::cout << message << "\n";

        SetConsoleTextAttribute(hConsole, originalAttributes);
    }

    static void CustomLog(int color, const std::string& start, const std::string& message);

    static void Logf(const char* format, ...) {
        char buffer[1024];

        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        Log(std::string(buffer));
    }

    static void LogfCustom(int color, const char* start, const char* format, ...) {
        char buffer[1024];

        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        CustomLog(color, start, std::string(buffer));
    }

    static void Banner() {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        WORD originalAttributes = csbi.wAttributes;

        SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        std::cout << "[ROBLOX CHEEZE]\n";

        SetConsoleTextAttribute(hConsole, originalAttributes);
    }

    static void Separator() {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        WORD originalAttributes = csbi.wAttributes;

        SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        std::cout << "================================================================================\n";

        SetConsoleTextAttribute(hConsole, originalAttributes);
    }

    static void Section(const std::string& title) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        WORD originalAttributes = csbi.wAttributes;

        SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        std::cout << "\n[== " << title << " ==============================================================]\n";

        SetConsoleTextAttribute(hConsole, originalAttributes);
    }

    static void Success(const std::string& message) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        WORD originalAttributes = csbi.wAttributes;

        SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        std::cout << "[== " << message << " COMPLETE ==]\n";

        SetConsoleTextAttribute(hConsole, originalAttributes);
    }
};
