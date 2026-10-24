#pragma once

#include "Util.hpp"
#include <fstream>

inline void hand_log(const std::string& message)
{
    dbg(message);
    // One small log per launch, readable without a debugger. Callers log state
    // transitions/errors or throttle periodic status, never joint coordinates.
    static std::ofstream file("Plugins/openRBRVR-hands.log", std::ios::trunc);
    if (file) {
        SYSTEMTIME now;
        GetLocalTime(&now);
        file << std::format("{:02}:{:02}:{:02}.{:03} {}\n", now.wHour, now.wMinute, now.wSecond, now.wMilliseconds, message);
        file.flush();
    }
}
