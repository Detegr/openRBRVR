#pragma once

#define GLM_FORCE_SIMD_AVX2

#include "Config.hpp"
#include "dxvk/d3d9_vr.h"
#include "glm/gtc/type_ptr.hpp"
#include "glm/mat4x4.hpp"
#include "openvr/openvr.h"
#include <d3d9.h>

enum RenderTarget : size_t {
    LeftEye = 0,
    RightEye = 1,
    Menu = 2,
    Overlay = 3,
};

using M4 = glm::mat4x4;

extern IDirect3DVR9* gD3DVR;
extern vr::IVRSystem* gHMD;
extern vr::IVRCompositor* gCompositor;
extern D3DVIEWPORT9 gVRViewPort;

extern M4 gHMDPose;
extern M4 gEyePos[2];
extern M4 gProjection[2];

bool InitVR(IDirect3DDevice9* dev, const Config& cfg, IDirect3DVR9** vrdev, uint32_t ww, uint32_t wh);
void ShutdownVR();
bool UpdateVRPoses();
IDirect3DSurface9* PrepareVRRendering(IDirect3DDevice9* dev, RenderTarget tgt, bool clear = true);
void FinishVRRendering(IDirect3DDevice9* dev, RenderTarget tgt);
void SubmitFramesToHMD();
void RenderMenuQuad(IDirect3DDevice9* dev, RenderTarget renderTarget3D, RenderTarget renderTarget2D);
std::tuple<uint32_t, uint32_t> GetRenderResolution(RenderTarget tgt);
void RenderCompanionWindowFromRenderTarget(IDirect3DDevice9* dev, RenderTarget tgt);

constexpr M4 M4FromD3D(const D3DMATRIX& m)
{
    return M4 {
        { m._11, m._12, m._13, m._14 },
        { m._21, m._22, m._23, m._24 },
        { m._31, m._32, m._33, m._34 },
        { m._41, m._42, m._43, m._44 },
    };
}

constexpr D3DMATRIX D3DFromM4(const M4& m)
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

constexpr M4 M4FromShaderConstantPtr(const float* p)
{
    return M4(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
}
