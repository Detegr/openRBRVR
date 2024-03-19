#include "OpenVR.hpp"
#include "Config.hpp"
#include "D3D.hpp"
#include "Globals.hpp"

constexpr std::string vr_compositor_error_str(vr::VRCompositorError e);

constexpr M4 OpenVR::get_projection_matrix(RenderTarget eye, float zNear, float zFar)
{
    vr::HmdMatrix44_t mat = hmd->GetProjectionMatrix(static_cast<vr::EVREye>(eye), zNear, zFar);

    return M4(
        { mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0] },
        { mat.m[0][1], mat.m[1][1], mat.m[2][1], mat.m[3][1] },
        { mat.m[0][2], mat.m[1][2], mat.m[2][2], mat.m[3][2] },
        { mat.m[0][3], mat.m[1][3], mat.m[2][3], mat.m[3][3] });
}

constexpr static M4 m4_from_steamvr_matrix(const vr::HmdMatrix34_t& m)
{
    return M4(
        { m.m[0][0], m.m[1][0], m.m[2][0], 0.0 },
        { m.m[0][1], m.m[1][1], m.m[2][1], 0.0 },
        { m.m[0][2], m.m[1][2], m.m[2][2], 0.0 },
        { m.m[0][3], m.m[1][3], m.m[2][3], 1.0f });
}

void OpenVR::init(IDirect3DDevice9* dev, const Config& cfg, IDirect3DVR9** vrdev, uint32_t companionWindowWidth, uint32_t companionWindowHeight)
{
    Direct3DCreateVR(dev, vrdev);

    // WaitGetPoses might access the Vulkan queue so we need to lock it
    g::d3d_vr->LockSubmissionQueue();
    if (auto e = compositor->WaitGetPoses(poses, vr::k_unMaxTrackedDeviceCount, nullptr, 0); e != vr::VRCompositorError_None) {
        dbg("Could not get VR poses");
    }
    g::d3d_vr->UnlockSubmissionQueue();

    uint32_t w, h;
    hmd->GetRecommendedRenderTargetSize(&w, &h);

    // This can also be used to get a (slightly different) VR display size, if need arises
    // if (vr::IVRExtendedDisplay* VRExtDisplay = vr::VRExtendedDisplay()) {
    //	int32_t x, y;
    // 	VRExtDisplay->GetWindowBounds(&x, &y, &w, &h);
    // }

    try {
        for (const auto& gfx : cfg.gfx) {
            auto supersampling = std::get<0>(gfx.second);
            auto wss = static_cast<uint32_t>(w * supersampling);
            auto hss = static_cast<uint32_t>(h * supersampling);
            RenderContext ctx = {
                .width = { wss, wss },
                .height = { hss, hss },
            };
            init_surfaces(dev, ctx, companionWindowWidth, companionWindowHeight);
            render_contexts[gfx.first] = ctx;
        }
        set_render_context("default");
    } catch (const std::runtime_error& e) {
        dbg(e.what());
        MessageBoxA(nullptr, e.what(), "VR init error", MB_OK);
        return;
    }

    dbg("VR init successful\n");
}

OpenVR::OpenVR()
    : hmd(nullptr)
    , compositor(nullptr)
{
    if (!vr::VR_IsHmdPresent()) {
        throw std::runtime_error("HMD not present, not initializing OpenVR");
    }

    vr::EVRInitError e = vr::VRInitError_None;
    hmd = vr::VR_Init(&e, vr::VRApplication_Scene);
    if (!hmd || e != vr::VRInitError_None) {
        throw std::runtime_error(std::format("VR init failed: {}", vr::VR_GetVRInitErrorAsEnglishDescription(e)));
    }

    compositor = vr::VRCompositor();
    if (!compositor) {
        vr::VR_Shutdown();
        throw std::runtime_error("Could not initialize VR compositor");
    }
    compositor->SetTrackingSpace(vr::ETrackingUniverseOrigin::TrackingUniverseSeated);

    stage_projection[LeftEye] = get_projection_matrix(LeftEye, zNearStage, zFar);
    stage_projection[RightEye] = get_projection_matrix(RightEye, zNearStage, zFar);
    cockpit_projection[LeftEye] = get_projection_matrix(LeftEye, zNearCockpit, zFar);
    cockpit_projection[RightEye] = get_projection_matrix(RightEye, zNearCockpit, zFar);
    mainmenu_projection[LeftEye] = get_projection_matrix(LeftEye, zNearMainMenu, zFar);
    mainmenu_projection[RightEye] = get_projection_matrix(RightEye, zNearMainMenu, zFar);

    eye_pos[LeftEye] = glm::inverse(m4_from_steamvr_matrix(hmd->GetEyeToHeadTransform(static_cast<vr::EVREye>(LeftEye))));
    eye_pos[RightEye] = glm::inverse(m4_from_steamvr_matrix(hmd->GetEyeToHeadTransform(static_cast<vr::EVREye>(RightEye))));
}

void OpenVR::set_render_context(const std::string& name)
{
    VRInterface::set_render_context(name);

    IDirect3DSurface9 *left_eye, *right_eye;
    if (current_render_context->dx_texture[LeftEye]->GetSurfaceLevel(0, &left_eye) != D3D_OK) {
        dbg("Failed to get left eye surface");
        return;
    }
    if (current_render_context->dx_texture[RightEye]->GetSurfaceLevel(0, &right_eye) != D3D_OK) {
        dbg("Failed to get right eye surface");
        left_eye->Release();
        return;
    }

    if (g::d3d_vr->GetVRDesc(left_eye, &dxvk_texture[LeftEye]) != D3D_OK) {
        dbg("Failed to get left eye descriptor");
        left_eye->Release();
        return;
    }
    if (g::d3d_vr->GetVRDesc(right_eye, &dxvk_texture[RightEye]) != D3D_OK) {
        dbg("Failed to get right eye descriptor");
        right_eye->Release();
        return;
    }

    openvr_texture[LeftEye].handle = reinterpret_cast<void*>(&dxvk_texture[LeftEye]);
    openvr_texture[LeftEye].eType = vr::TextureType_Vulkan;
    openvr_texture[LeftEye].eColorSpace = vr::ColorSpace_Auto;
    openvr_texture[RightEye].handle = reinterpret_cast<void*>(&dxvk_texture[RightEye]);
    openvr_texture[RightEye].eType = vr::TextureType_Vulkan;
    openvr_texture[RightEye].eColorSpace = vr::ColorSpace_Auto;
}

void OpenVR::prepare_frames_for_hmd(IDirect3DDevice9* dev)
{
    if (g::cfg.msaa != D3DMULTISAMPLE_NONE) {
        // Resolve multisampling
        IDirect3DSurface9 *left_eye, *right_eye;

        if (current_render_context->dx_texture[LeftEye]->GetSurfaceLevel(0, &left_eye) != D3D_OK) {
            dbg("Failed to get left eye surface");
            return;
        }
        if (current_render_context->dx_texture[RightEye]->GetSurfaceLevel(0, &right_eye) != D3D_OK) {
            dbg("Failed to get right eye surface");
            left_eye->Release();
            return;
        }

        dev->StretchRect(current_render_context->dx_surface[LeftEye], nullptr, left_eye, nullptr, D3DTEXF_NONE);
        dev->StretchRect(current_render_context->dx_surface[RightEye], nullptr, right_eye, nullptr, D3DTEXF_NONE);

        left_eye->Release();
        right_eye->Release();
    }
}

void OpenVR::submit_frames_to_hmd(IDirect3DDevice9* dev)
{
    g::d3d_vr->BeginVRSubmit();
    if (auto e = compositor->Submit(static_cast<vr::EVREye>(LeftEye), &openvr_texture[LeftEye]); e != vr::VRCompositorError_None) [[unlikely]] {
        dbg(std::format("Compositor error: {}", vr_compositor_error_str(e)));
    }
    if (auto e = compositor->Submit(static_cast<vr::EVREye>(RightEye), &openvr_texture[RightEye]); e != vr::VRCompositorError_None) [[unlikely]] {
        dbg(std::format("Compositor error: {}", vr_compositor_error_str(e)));
    }
    compositor->PostPresentHandoff();
    g::d3d_vr->EndVRSubmit();
}

bool OpenVR::update_vr_poses()
{
    // WaitGetPoses might access the Vulkan queue so we need to lock it
    g::d3d_vr->LockSubmissionQueue();
    if (auto e = compositor->WaitGetPoses(poses, vr::k_unMaxTrackedDeviceCount, nullptr, 0); e != vr::VRCompositorError_None) {
        dbg("Could not get VR poses");
        return false;
    }
    g::d3d_vr->UnlockSubmissionQueue();

    auto pose = &poses[vr::k_unTrackedDeviceIndex_Hmd];
    if (pose->bPoseIsValid) {
        hmd_pose[LeftEye] = glm::inverse(m4_from_steamvr_matrix(pose->mDeviceToAbsoluteTracking));
        hmd_pose[RightEye] = hmd_pose[LeftEye];
    }

    return true;
}

FrameTimingInfo OpenVR::get_frame_timing()
{
    FrameTimingInfo ret = { 0 };

    vr::Compositor_FrameTiming t {
        .m_nSize = sizeof(vr::Compositor_FrameTiming)
    };
    if (compositor->GetFrameTiming(&t)) {
        ret.gpu_pre_submit = t.m_flPreSubmitGpuMs;
        ret.gpu_post_submit = t.m_flPostSubmitGpuMs;
        ret.compositor_gpu = t.m_flCompositorRenderGpuMs;
        ret.compositor_cpu = t.m_flCompositorRenderCpuMs;
        ret.compositor_submit_frame = t.m_flSubmitFrameMs;
        ret.gpu_total = t.m_flTotalRenderGpuMs;
        ret.frame_interval = t.m_flClientFrameIntervalMs;
        ret.cpu_present_call = t.m_flPresentCallCpuMs;
        ret.cpu_wait_for_present = t.m_flWaitForPresentCpuMs;
        ret.reprojection_flags = t.m_nReprojectionFlags;
        ret.mispresented_frames = t.m_nNumMisPresented;
        ret.dropped_frames = t.m_nNumDroppedFrames;
    }
    return ret;
}

static constexpr std::string vr_compositor_error_str(vr::VRCompositorError e)
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
