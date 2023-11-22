#include "OpenVR.hpp"
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

OpenVR::OpenVR(IDirect3DDevice9* dev, const Config& cfg, IDirect3DVR9** vrdev, uint32_t companionWindowWidth, uint32_t companionWindowHeight)
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

    if (auto e = compositor->WaitGetPoses(poses, vr::k_unMaxTrackedDeviceCount, nullptr, 0); e != vr::VRCompositorError_None) {
        Dbg("Could not get VR poses");
    }

    uint32_t w, h;
    HMD->GetRecommendedRenderTargetSize(&w, &h);

    auto wss = static_cast<uint32_t>(w * cfg.superSampling);
    auto hss = static_cast<uint32_t>(h * cfg.superSampling);

    // This can also be used to get a (slightly different) VR display size, if need arises
    // if (vr::IVRExtendedDisplay* VRExtDisplay = vr::VRExtendedDisplay()) {
    //	int32_t x, y;
    // 	VRExtDisplay->GetWindowBounds(&x, &y, &w, &h);
    // }

    try {
        InitSurfaces(dev, std::make_tuple(wss, hss), std::make_tuple(wss, hss), companionWindowWidth, companionWindowHeight);

        openvrTexture[LeftEye].handle = reinterpret_cast<void*>(&dxvkTexture[LeftEye]);
        openvrTexture[LeftEye].eType = vr::TextureType_Vulkan;
        openvrTexture[LeftEye].eColorSpace = vr::ColorSpace_Auto;
        openvrTexture[RightEye].handle = reinterpret_cast<void*>(&dxvkTexture[RightEye]);
        openvrTexture[RightEye].eType = vr::TextureType_Vulkan;
        openvrTexture[RightEye].eColorSpace = vr::ColorSpace_Auto;
    } catch (const std::runtime_error& e) {
        Dbg(e.what());
    }

    Direct3DCreateVR(dev, vrdev);
    Dbg("VR init successful\n");
}

void OpenVR::SubmitFramesToHMD(IDirect3DDevice9* dev)
{
    IDirect3DSurface9 *leftEye, *rightEye;

    if (dxTexture[LeftEye]->GetSurfaceLevel(0, &leftEye) != D3D_OK) {
        Dbg("Failed to get left eye surface");
        return;
    }
    if (dxTexture[RightEye]->GetSurfaceLevel(0, &rightEye) != D3D_OK) {
        Dbg("Failed to get right eye surface");
        leftEye->Release();
        return;
    }

    if (gCfg.msaa != D3DMULTISAMPLE_NONE) {
        // Resolve multisampling
        dev->StretchRect(dxSurface[LeftEye], nullptr, leftEye, nullptr, D3DTEXF_NONE);
        dev->StretchRect(dxSurface[RightEye], nullptr, rightEye, nullptr, D3DTEXF_NONE);
    }

    if (gD3DVR->BeginVRSubmit() != D3D_OK) {
        Dbg("BeginVRSubmit failed");
        return;
    }

    if (gD3DVR->TransferSurfaceForVR(leftEye) != D3D_OK) {
        Dbg("Failed to transfer left eye surface");
        goto release;
    }
    if (gD3DVR->TransferSurfaceForVR(rightEye) != D3D_OK) {
        Dbg("Failed to transfer right eye surface");
        goto release;
    }
    if (gD3DVR->GetVRDesc(leftEye, &dxvkTexture[LeftEye]) != D3D_OK) {
        Dbg("Failed to get left eye descriptor");
        goto release;
    }
    if (gD3DVR->GetVRDesc(rightEye, &dxvkTexture[RightEye]) != D3D_OK) {
        Dbg("Failed to get left eye descriptor");
        goto release;
    }
    if (auto e = compositor->Submit(static_cast<vr::EVREye>(LeftEye), &openvrTexture[LeftEye]); e != vr::VRCompositorError_None) [[unlikely]] {
        Dbg(std::format("Compositor error: {}", VRCompositorErrorStr(e)));
    }
    if (auto e = compositor->Submit(static_cast<vr::EVREye>(RightEye), &openvrTexture[RightEye]); e != vr::VRCompositorError_None) [[unlikely]] {
        Dbg(std::format("Compositor error: {}", VRCompositorErrorStr(e)));
    }
    if (gD3DVR->EndVRSubmit() != D3D_OK) {
        Dbg("EndVRSubmit failed");
    }

release:
    leftEye->Release();
    rightEye->Release();
}

bool OpenVR::UpdateVRPoses(Quaternion* carQuat, Config::HorizonLock lockSetting)
{
    if (auto e = compositor->WaitGetPoses(poses, vr::k_unMaxTrackedDeviceCount, nullptr, 0); e != vr::VRCompositorError_None) {
        Dbg("Could not get VR poses");
        return false;
    }
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
