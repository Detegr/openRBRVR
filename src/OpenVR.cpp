#include "OpenVR.hpp"
#include "Config.hpp"
#include "D3D.hpp"

extern Config gCfg;

constexpr std::string VRCompositorErrorStr(vr::VRCompositorError e);

constexpr M4 OpenVR::GetProjectionMatrix(RenderTarget eye, float zNear, float zFar)
{
    vr::HmdMatrix44_t mat = HMD->GetProjectionMatrix(static_cast<vr::EVREye>(eye), zNear, zFar);

    return M4(
        { mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0] },
        { mat.m[0][1], mat.m[1][1], mat.m[2][1], mat.m[3][1] },
        { mat.m[0][2], mat.m[1][2], mat.m[2][2], mat.m[3][2] },
        { mat.m[0][3], mat.m[1][3], mat.m[2][3], mat.m[3][3] });
}

constexpr static M4 M4FromSteamVRMatrix(const vr::HmdMatrix34_t& m)
{
    return M4(
        { m.m[0][0], m.m[1][0], m.m[2][0], 0.0 },
        { m.m[0][1], m.m[1][1], m.m[2][1], 0.0 },
        { m.m[0][2], m.m[1][2], m.m[2][2], 0.0 },
        { m.m[0][3], m.m[1][3], m.m[2][3], 1.0f });
}

void OpenVR::Init(IDirect3DDevice9* dev, const Config& cfg, IDirect3DVR9** vrdev, uint32_t companionWindowWidth, uint32_t companionWindowHeight)
{
    Direct3DCreateVR(dev, vrdev);

    // WaitGetPoses might access the Vulkan queue so we need to lock it
    gD3DVR->LockSubmissionQueue();
    if (auto e = compositor->WaitGetPoses(poses, vr::k_unMaxTrackedDeviceCount, nullptr, 0); e != vr::VRCompositorError_None) {
        Dbg("Could not get VR poses");
    }
    gD3DVR->UnlockSubmissionQueue();

    uint32_t w, h;
    HMD->GetRecommendedRenderTargetSize(&w, &h);

    // This can also be used to get a (slightly different) VR display size, if need arises
    // if (vr::IVRExtendedDisplay* VRExtDisplay = vr::VRExtendedDisplay()) {
    //	int32_t x, y;
    // 	VRExtDisplay->GetWindowBounds(&x, &y, &w, &h);
    // }

    try {
        for (const auto& gfx : cfg.gfx) {
            auto superSampling = std::get<0>(gfx.second);
            auto wss = static_cast<uint32_t>(w * superSampling);
            auto hss = static_cast<uint32_t>(h * superSampling);
            RenderContext ctx = {
                .width = { wss, wss },
                .height = { hss, hss },
            };
            InitSurfaces(dev, ctx, companionWindowWidth, companionWindowHeight);
            renderContexts[gfx.first] = ctx;
        }
        SetRenderContext("default");
    } catch (const std::runtime_error& e) {
        Dbg(e.what());
        MessageBoxA(nullptr, e.what(), "VR init error", MB_OK);
        return;
    }

    Dbg("VR init successful\n");
}

OpenVR::OpenVR()
    : HMD(nullptr)
    , compositor(nullptr)
{
    if (!vr::VR_IsHmdPresent()) {
        throw std::runtime_error("HMD not present, not initializing OpenVR");
    }

    vr::EVRInitError e = vr::VRInitError_None;
    HMD = vr::VR_Init(&e, vr::VRApplication_Scene);
    if (!HMD || e != vr::VRInitError_None) {
        throw std::runtime_error(std::format("VR init failed: {}", vr::VR_GetVRInitErrorAsEnglishDescription(e)));
    }

    compositor = vr::VRCompositor();
    if (!compositor) {
        vr::VR_Shutdown();
        throw std::runtime_error("Could not initialize VR compositor");
    }
    compositor->SetTrackingSpace(vr::ETrackingUniverseOrigin::TrackingUniverseSeated);

    stageProjection[LeftEye] = GetProjectionMatrix(LeftEye, zNearStage, zFar);
    stageProjection[RightEye] = GetProjectionMatrix(RightEye, zNearStage, zFar);
    cockpitProjection[LeftEye] = GetProjectionMatrix(LeftEye, zNearCockpit, zFar);
    cockpitProjection[RightEye] = GetProjectionMatrix(RightEye, zNearCockpit, zFar);
    mainMenuProjection[LeftEye] = GetProjectionMatrix(LeftEye, zNearMainMenu, zFar);
    mainMenuProjection[RightEye] = GetProjectionMatrix(RightEye, zNearMainMenu, zFar);

    eyePos[LeftEye] = glm::inverse(M4FromSteamVRMatrix(HMD->GetEyeToHeadTransform(static_cast<vr::EVREye>(LeftEye))));
    eyePos[RightEye] = glm::inverse(M4FromSteamVRMatrix(HMD->GetEyeToHeadTransform(static_cast<vr::EVREye>(RightEye))));
}

void OpenVR::SetRenderContext(const std::string& name)
{
    VRInterface::SetRenderContext(name);

    IDirect3DSurface9 *leftEye, *rightEye;
    if (currentRenderContext->dxTexture[LeftEye]->GetSurfaceLevel(0, &leftEye) != D3D_OK) {
        Dbg("Failed to get left eye surface");
        return;
    }
    if (currentRenderContext->dxTexture[RightEye]->GetSurfaceLevel(0, &rightEye) != D3D_OK) {
        Dbg("Failed to get right eye surface");
        leftEye->Release();
        return;
    }

    if (gD3DVR->GetVRDesc(leftEye, &dxvkTexture[LeftEye]) != D3D_OK) {
        Dbg("Failed to get left eye descriptor");
        leftEye->Release();
        return;
    }
    if (gD3DVR->GetVRDesc(rightEye, &dxvkTexture[RightEye]) != D3D_OK) {
        Dbg("Failed to get right eye descriptor");
        rightEye->Release();
        return;
    }

    openvrTexture[LeftEye].handle = reinterpret_cast<void*>(&dxvkTexture[LeftEye]);
    openvrTexture[LeftEye].eType = vr::TextureType_Vulkan;
    openvrTexture[LeftEye].eColorSpace = vr::ColorSpace_Auto;
    openvrTexture[RightEye].handle = reinterpret_cast<void*>(&dxvkTexture[RightEye]);
    openvrTexture[RightEye].eType = vr::TextureType_Vulkan;
    openvrTexture[RightEye].eColorSpace = vr::ColorSpace_Auto;
}

void OpenVR::PrepareFramesForHMD(IDirect3DDevice9* dev)
{
    if (gCfg.msaa != D3DMULTISAMPLE_NONE) {
        // Resolve multisampling
        IDirect3DSurface9 *leftEye, *rightEye;

        if (currentRenderContext->dxTexture[LeftEye]->GetSurfaceLevel(0, &leftEye) != D3D_OK) {
            Dbg("Failed to get left eye surface");
            return;
        }
        if (currentRenderContext->dxTexture[RightEye]->GetSurfaceLevel(0, &rightEye) != D3D_OK) {
            Dbg("Failed to get right eye surface");
            leftEye->Release();
            return;
        }

        dev->StretchRect(currentRenderContext->dxSurface[LeftEye], nullptr, leftEye, nullptr, D3DTEXF_NONE);
        dev->StretchRect(currentRenderContext->dxSurface[RightEye], nullptr, rightEye, nullptr, D3DTEXF_NONE);

        leftEye->Release();
        rightEye->Release();
    }
}

void OpenVR::SubmitFramesToHMD(IDirect3DDevice9* dev)
{
    gD3DVR->BeginVRSubmit();
    if (auto e = compositor->Submit(static_cast<vr::EVREye>(LeftEye), &openvrTexture[LeftEye]); e != vr::VRCompositorError_None) [[unlikely]] {
        Dbg(std::format("Compositor error: {}", VRCompositorErrorStr(e)));
    }
    if (auto e = compositor->Submit(static_cast<vr::EVREye>(RightEye), &openvrTexture[RightEye]); e != vr::VRCompositorError_None) [[unlikely]] {
        Dbg(std::format("Compositor error: {}", VRCompositorErrorStr(e)));
    }
    compositor->PostPresentHandoff();
    gD3DVR->EndVRSubmit();
}

bool OpenVR::UpdateVRPoses(Quaternion* carQuat, HorizonLock lockSetting)
{
    // WaitGetPoses might access the Vulkan queue so we need to lock it
    gD3DVR->LockSubmissionQueue();
    if (auto e = compositor->WaitGetPoses(poses, vr::k_unMaxTrackedDeviceCount, nullptr, 0); e != vr::VRCompositorError_None) {
        Dbg("Could not get VR poses");
        return false;
    }
    gD3DVR->UnlockSubmissionQueue();

    auto pose = &poses[vr::k_unTrackedDeviceIndex_Hmd];
    if (pose->bPoseIsValid) {
        HMDPose[LeftEye] = glm::inverse(M4FromSteamVRMatrix(pose->mDeviceToAbsoluteTracking));
        HMDPose[RightEye] = HMDPose[LeftEye];
    }

    horizonLock = GetHorizonLockMatrix(carQuat, lockSetting);

    return true;
}

FrameTimingInfo OpenVR::GetFrameTiming()
{
    FrameTimingInfo ret = { 0 };

    vr::Compositor_FrameTiming t {
        .m_nSize = sizeof(vr::Compositor_FrameTiming)
    };
    if (compositor->GetFrameTiming(&t)) {
        ret.gpuPreSubmit = t.m_flPreSubmitGpuMs;
        ret.gpuPostSubmit = t.m_flPostSubmitGpuMs;
        ret.compositorGpu = t.m_flCompositorRenderGpuMs;
        ret.compositorCpu = t.m_flCompositorRenderCpuMs;
        ret.compositorSubmitFrame = t.m_flSubmitFrameMs;
        ret.gpuTotal = t.m_flTotalRenderGpuMs;
        ret.frameInterval = t.m_flClientFrameIntervalMs;
        ret.cpuPresentCall = t.m_flPresentCallCpuMs;
        ret.cpuWaitForPresent = t.m_flWaitForPresentCpuMs;
        ret.reprojectionFlags = t.m_nReprojectionFlags;
        ret.mispresentedFrames = t.m_nNumMisPresented;
        ret.droppedFrames = t.m_nNumDroppedFrames;
    }
    return ret;
}

static constexpr std::string VRCompositorErrorStr(vr::VRCompositorError e)
{
    switch (e) {
        case vr::VRCompositorError::VRCompositorError_AlreadySet:
            return "AlreadySet";
        case vr::VRCompositorError::VRCompositorError_AlreadySubmitted:
            return "AlreadySubmitted";
        case vr::VRCompositorError::VRCompositorError_DoNotHaveFocus:
            return "DoNotHaveFocus";
        case vr::VRCompositorError::VRCompositorError_IncompatibleVersion:
            return "IncompatibleVersion";
        case vr::VRCompositorError::VRCompositorError_IndexOutOfRange:
            return "IndexOutOfRange";
        case vr::VRCompositorError::VRCompositorError_InvalidBounds:
            return "InvalidBounds";
        case vr::VRCompositorError::VRCompositorError_InvalidTexture:
            return "InvalidTexture";
        case vr::VRCompositorError::VRCompositorError_IsNotSceneApplication:
            return "IsNotSceneApplication";
        case vr::VRCompositorError::VRCompositorError_None:
            return "None";
        case vr::VRCompositorError::VRCompositorError_RequestFailed:
            return "RequestFailed";
        case vr::VRCompositorError::VRCompositorError_SharedTexturesNotSupported:
            return "SharedTexturesNotSupported";
        case vr::VRCompositorError::VRCompositorError_TextureIsOnWrongDevice:
            return "TextureIsOnWrongDevice";
        case vr::VRCompositorError::VRCompositorError_TextureUsesUnsupportedFormat:
            return "TextureUsesUnsupportedFormat";
        default:
            return "";
    }
}
