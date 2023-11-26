#pragma once

// clang-format off
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <debugapi.h>
#include <format>
#include <string>
// clang-format on

inline void Dbg(const std::string& str)
{
    OutputDebugString(std::format("[openRBRVR] {}\n", str).c_str());
}
