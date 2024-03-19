#pragma once

// clang-format off
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <debugapi.h>
#include <format>
#include <string>
#include <d3d9.h>

#define GLM_FORCE_SIMD_AVX2
#include <gtc/quaternion.hpp>
#include <gtc/type_ptr.hpp>
#include <mat3x3.hpp>
#include <mat4x4.hpp>

// clang-format on

inline void dbg(const std::string& str)
{
    OutputDebugString(std::format("[openRBRVR] {}\n", str).c_str());
}

using M4 = glm::mat4x4;
using M3 = glm::mat3x3;

constexpr M4 m4_from_d3d(const D3DMATRIX& m)
{
    return M4 {
        { m._11, m._12, m._13, m._14 },
        { m._21, m._22, m._23, m._24 },
        { m._31, m._32, m._33, m._34 },
        { m._41, m._42, m._43, m._44 },
    };
}

constexpr D3DMATRIX d3d_from_m4(const M4& m)
{
    D3DMATRIX ret;

    // clang-format off
    ret._11 = m[0][0]; ret._12 = m[0][1]; ret._13 = m[0][2]; ret._14 = m[0][3];
    ret._21 = m[1][0]; ret._22 = m[1][1]; ret._23 = m[1][2]; ret._24 = m[1][3];
    ret._31 = m[2][0]; ret._32 = m[2][1]; ret._33 = m[2][2]; ret._34 = m[2][3];
    ret._41 = m[3][0]; ret._42 = m[3][1]; ret._43 = m[3][2]; ret._44 = m[3][3];
    // clang-format on

    return ret;
}

constexpr M4 m4_from_shader_constant_ptr(const float* p)
{
    return M4(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
}
