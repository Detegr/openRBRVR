#pragma once

#include <debugapi.h>
#include <format>
#include <string>

inline void Dbg(const std::string& str)
{
    OutputDebugString(std::format("[openRBRVR] {}\n", str).c_str());
}
