#include "console.h"

void Logger::CustomLog(int color, const std::string& start, const std::string& message) {
    char timeBuffer[9];
    GetTime(timeBuffer);

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    WORD originalAttributes = csbi.wAttributes;

    SetConsoleTextAttribute(hConsole, color | FOREGROUND_INTENSITY);
    std::cout << "[" << start << "] ";

    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    std::cout << message << "\n";

    SetConsoleTextAttribute(hConsole, originalAttributes);
}
