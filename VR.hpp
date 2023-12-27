#pragma once

#define GLM_FORCE_SIMD_AVX2

#include "Config.hpp"
#include "Quaternion.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/mat3x3.hpp"
#include "glm/mat4x4.hpp"
#include <d3d9_vr.h>
#define XR_USE_GRAPHICS_API_VULKAN
#include "openvr/openvr.h"
#include "openxr/openxr.h"
#include <d3d9.h>

enum RenderTarget : size_t {
    LeftEye = 0,
    RightEye = 1,
    Menu = 2,
    Overlay = 3,
};

enum class Projection {
    Stage,
    Cockpit,
    MainMenu,
};

using M4 = glm::mat4x4;

extern IDirect3DVR9* gD3DVR;

struct FrameTimingInfo {
    float gpuPreSubmit;
    float gpuPostSubmit;
    float compositorGpu;
    float compositorCpu;
    float compositorSubmitFrame;
    float gpuTotal;
    float frameInterval;
    float waitForPresent;
    float cpuPresentCall;
    float cpuWaitForPresent;
    uint32_t reprojectionFlags;
    uint32_t mispresentedFrames;
    uint32_t droppedFrames;
};

enum VRRuntime {
    OPENVR,
    OPENXR
};

class VRInterface {
protected:
    IDirect3DTexture9* dxTexture[4];
    IDirect3DSurface9* dxSurface[4];
    IDirect3DSurface9* dxDepthStencilSurface[4];

    M4 HMDPose;
    M4 eyePos[2];
    M4 cockpitProjection[2];
    M4 stageProjection[2];
    M4 mainMenuProjection[2];
    M4 horizonLock;

    bool CreateRenderTarget(IDirect3DDevice9* dev, RenderTarget tgt, D3DFORMAT fmt, uint32_t w, uint32_t h);
    void InitSurfaces(IDirect3DDevice9* dev, uint32_t resx, uint32_t resy, uint32_t resx2d, uint32_t resy2d);

    static constexpr float zFar = 10000.0f;
    static constexpr float zNearStage = 0.35f;
    static constexpr float zNearCockpit = 0.01f;
    static constexpr float zNearMainMenu = 0.1f;

public:
    virtual void ShutdownVR() = 0;
    virtual bool UpdateVRPoses(Quaternion* carQuat, Config::HorizonLock lockSetting) = 0;
    virtual IDirect3DSurface9* PrepareVRRendering(IDirect3DDevice9* dev, RenderTarget tgt, bool clear = true);
    virtual void FinishVRRendering(IDirect3DDevice9* dev, RenderTarget tgt);
    virtual void SubmitFramesToHMD(IDirect3DDevice9* dev) = 0;
    virtual std::tuple<uint32_t, uint32_t> GetRenderResolution(RenderTarget tgt) const
    {
        D3DSURFACE_DESC desc;
        dxTexture[LeftEye]->GetLevelDesc(0, &desc);
        return std::make_tuple(desc.Width, desc.Height);
    }
    virtual FrameTimingInfo GetFrameTiming() const = 0;

    const M4& GetProjection(RenderTarget tgt, Projection p) const
    {
        switch (p) {
            case Projection::Stage:
                return stageProjection[tgt];
            case Projection::Cockpit:
                return cockpitProjection[tgt];
            case Projection::MainMenu:
                return mainMenuProjection[tgt];
            default:
                std::unreachable();
        }
    }
    const M4& GetEyePos(RenderTarget tgt) const { return eyePos[tgt]; }
    const M4& GetPose() const { return HMDPose; }
    const M4& GetHorizonLock() const { return horizonLock; }
    IDirect3DTexture9* GetTexture(RenderTarget tgt) const { return dxTexture[tgt]; }

    virtual void ResetView() const = 0;
    virtual VRRuntime GetRuntimeType() const = 0;
};

bool CreateQuad(IDirect3DDevice9* dev, RenderTarget tgt, float aspect);
bool CreateCompanionWindowBuffer(IDirect3DDevice9* dev);
void RenderMenuQuad(IDirect3DDevice9* dev, VRInterface* vr, RenderTarget renderTarget3D, RenderTarget renderTarget2D, Projection projType, float size, glm::vec3 translation, const std::optional<M4>& horizonLock);
void RenderCompanionWindowFromRenderTarget(IDirect3DDevice9* dev, VRInterface* vr, RenderTarget tgt);

constexpr M4 gFlipZMatrix = {
    { 1, 0, 0, 0 },
    { 0, 1, 0, 0 },
    { 0, 0, -1, 0 },
    { 0, 0, 0, 1 },
};

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
