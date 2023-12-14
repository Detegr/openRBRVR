#include "VR.hpp"
#include "D3D.hpp"
#include "Util.hpp"
#include "Vertex.hpp"

#include <format>

IDirect3DVR9* gD3DVR = nullptr;
vr::IVRSystem* gHMD = nullptr;
vr::IVRCompositor* gCompositor = nullptr;
M4 gHMDPose;
M4 gEyePos[2];
M4 gProjection[2];
M4 gCockpitProjection[2];
M4 gMainMenu3dProjection[2];

static IDirect3DTexture9* dxTexture[4];
static IDirect3DSurface9* dxSurface[4];
static IDirect3DSurface9* dxDepthStencilSurface[4];
static vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
static vr::Texture_t openvrTexture[2];
static D3D9_TEXTURE_VR_DESC dxvkTexture[2];
static IDirect3DVertexBuffer9* quadVertexBuf[2];
static IDirect3DVertexBuffer9* companionWindowVertexBuf;
static constexpr D3DMATRIX identityMatrix = D3DFromM4(glm::identity<glm::mat4x4>());

constexpr static M4 GetProjectionMatrix(RenderTarget eye, float zNear, float zFar)
{
    vr::HmdMatrix44_t mat = gHMD->GetProjectionMatrix(static_cast<vr::EVREye>(eye), zNear, zFar);

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

bool CreateCompanionWindowBuffer(IDirect3DDevice9* dev)
{
    // clang-format off
    Vertex quad[] = {
        { -1, 1, 1, 0, 0, }, // Top-left
        { 1, 1, 1, 1, 0, }, // Top-right
        { -1, -1, 1, 0, 1 }, // Bottom-left
        { 1, -1, 1, 1, 1 } // Bottom-right
    };
    // clang-format on
    if (!CreateVertexBuffer(dev, quad, 4, &companionWindowVertexBuf)) {
        Dbg("Could not create vertex buffer for companion window");
        return false;
    }
    return true;
}

static bool CreateTexture(IDirect3DDevice9* dev, RenderTarget tgt, D3DFORMAT fmt, uint32_t w, uint32_t h)
{
    auto ret = dev->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, fmt, D3DPOOL_DEFAULT, &dxTexture[tgt], nullptr);
    static D3DFORMAT depthStencilFormat;
    if (depthStencilFormat == D3DFMT_UNKNOWN) {
        D3DFORMAT wantedFormats[] = {
            D3DFMT_D32F_LOCKABLE,
            D3DFMT_D24S8,
            D3DFMT_D24X8,
            D3DFMT_D16,
        };
        IDirect3D9* d3d;
        if (dev->GetDirect3D(&d3d) != D3D_OK) {
            Dbg("Could not get Direct3D adapter");
            depthStencilFormat = D3DFMT_D16;
        } else {
            for (const auto& format : wantedFormats) {
                if (d3d->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, fmt, fmt, format) == D3D_OK) {
                    depthStencilFormat = format;
                    Dbg(std::format("Using {} as depthstencil format", (int)format));
                    break;
                }
            }
            if (depthStencilFormat == D3DFMT_UNKNOWN) {
                Dbg("No depth stencil format found?? Using D3DFMT_D16");
            }
        }
    }
    ret |= dev->CreateDepthStencilSurface(w, h, depthStencilFormat, D3DMULTISAMPLE_NONE, 0, TRUE, &dxDepthStencilSurface[tgt], nullptr);
    if (FAILED(ret)) {
        Dbg("D3D initialization failed: CreateTexture");
        return false;
    }
    return true;
}

static bool CreateVRTexture(IDirect3DDevice9* dev, RenderTarget eye, uint32_t w, uint32_t h)
{
    CreateTexture(dev, eye, D3DFMT_X8R8G8B8, w, h);
    openvrTexture[eye].handle = reinterpret_cast<void*>(&dxvkTexture[eye]);
    openvrTexture[eye].eType = vr::TextureType_Vulkan;
    openvrTexture[eye].eColorSpace = vr::ColorSpace_Auto;
    return true;
}

static bool CreateQuad(IDirect3DDevice9* dev, RenderTarget tgt)
{
    // clang-format off
    Vertex quad[] = {
        { -0.6f, 0.35f, 1.0f, 0.0f, 0.0f, }, // Top-left
        { 0.6f, 0.35f, 1.0f, 1.0f, 0.0f, }, // Top-right
        { -0.6f, -0.35f, 1.0f, 0.0f, 1.0f }, // Bottom-left
        { 0.6f, -0.35f, 1.0f, 1.0f, 1.0f } // Bottom-right
    };

    // We have only 2 quads, so indexing by RenderTarget directly does not work here
    auto tgtIdx = tgt == Menu ? 0 : 1;

    // clang-format on
    return CreateVertexBuffer(dev, quad, 4, &quadVertexBuf[tgtIdx]);
}

bool InitVR(IDirect3DDevice9* dev, const Config& cfg, IDirect3DVR9** vrdev, uint32_t companionWindowWidth, uint32_t companionWindowHeight)
{
    if (!vr::VR_IsHmdPresent()) {
        Dbg("HMD not present, not initializing VR");
        return false;
    }

    vr::EVRInitError e = vr::VRInitError_None;
    gHMD = vr::VR_Init(&e, vr::VRApplication_Scene);

    if (e != vr::VRInitError_None) {
        Dbg(std::format("VR init failed: {}", vr::VR_GetVRInitErrorAsEnglishDescription(e)));
        return false;
    }
    if (!gHMD) {
        Dbg("VR init failed");
        return false;
    }

    gCompositor = vr::VRCompositor();
    if (!gCompositor) {
        Dbg("Could not initialize VR compositor");
        vr::VR_Shutdown();
        return false;
    }
    gCompositor->SetTrackingSpace(vr::ETrackingUniverseOrigin::TrackingUniverseSeated);

    constexpr auto zFar = 10000.0f;
    constexpr auto zNearStage = 1.0f;
    constexpr auto zNearCockpit = 0.01f;
    constexpr auto zNearMainMenu = 0.1f;
    gProjection[LeftEye] = GetProjectionMatrix(LeftEye, zNearStage, zFar);
    gProjection[RightEye] = GetProjectionMatrix(RightEye, zNearStage, zFar);
    gCockpitProjection[LeftEye] = GetProjectionMatrix(LeftEye, zNearCockpit, zFar);
    gCockpitProjection[RightEye] = GetProjectionMatrix(RightEye, zNearCockpit, zFar);
    gMainMenu3dProjection[LeftEye] = GetProjectionMatrix(LeftEye, zNearMainMenu, zFar);
    gMainMenu3dProjection[RightEye] = GetProjectionMatrix(RightEye, zNearMainMenu, zFar);

    gEyePos[LeftEye] = glm::inverse(M4FromSteamVRMatrix(gHMD->GetEyeToHeadTransform(static_cast<vr::EVREye>(LeftEye))));
    gEyePos[RightEye] = glm::inverse(M4FromSteamVRMatrix(gHMD->GetEyeToHeadTransform(static_cast<vr::EVREye>(RightEye))));

    if (auto e = gCompositor->WaitGetPoses(poses, vr::k_unMaxTrackedDeviceCount, nullptr, 0); e != vr::VRCompositorError_None) {
        Dbg("Could not get VR poses");
        vr::VR_Shutdown();
        return false;
    }

    uint32_t w, h;
    gHMD->GetRecommendedRenderTargetSize(&w, &h);

    auto wss = static_cast<uint32_t>(w * cfg.superSampling);
    auto hss = static_cast<uint32_t>(h * cfg.superSampling);

    // This can also be used to get a (slightly different) VR display size, if need arises
    // if (vr::IVRExtendedDisplay* VRExtDisplay = vr::VRExtendedDisplay()) {
    //	int32_t x, y;
    // 	VRExtDisplay->GetWindowBounds(&x, &y, &w, &h);
    // }

    if (!CreateVRTexture(dev, LeftEye, wss, hss))
        return false;
    if (!CreateVRTexture(dev, RightEye, wss, hss))
        return false;
    if (!CreateTexture(dev, Menu, D3DFMT_X8B8G8R8, companionWindowWidth, companionWindowHeight))
        return false;
    if (!CreateTexture(dev, Overlay, D3DFMT_A8B8G8R8, companionWindowWidth, companionWindowHeight))
        return false;

    // Create and fill a vertex buffers for the 2D planes
    if (!CreateQuad(dev, Menu))
        return false;
    if (!CreateQuad(dev, Overlay))
        return false;

    if (!CreateCompanionWindowBuffer(dev))
        return false;

    Direct3DCreateVR(dev, vrdev);

    Dbg("VR init successful\n");

    return true;
}

void ShutdownVR()
{
    if (gHMD) {
        vr::VR_Shutdown();
        gHMD = nullptr;
        gCompositor = nullptr;
        gD3DVR = nullptr;
    }
}

bool UpdateVRPoses(Quaternion* carQuat, Config::HorizonLock lockSetting, M4* horizonLock)
{
    if (auto e = gCompositor->WaitGetPoses(poses, vr::k_unMaxTrackedDeviceCount, nullptr, 0); e != vr::VRCompositorError_None) {
        Dbg("Could not get VR poses");
        return false;
    }
    auto pose = &poses[vr::k_unTrackedDeviceIndex_Hmd];
    if (pose->bPoseIsValid) {
        gHMDPose = glm::inverse(M4FromSteamVRMatrix(pose->mDeviceToAbsoluteTracking));
    }
    if (carQuat) {
        // If car quaternion is given, calculate matrix for locking the horizon
        glm::quat q(carQuat->w, carQuat->x, carQuat->y, carQuat->z);
        glm::vec3 ang = glm::eulerAngles(q);
        auto pitch = (lockSetting & Config::HorizonLock::LOCK_PITCH) ? glm::pitch(q) : 0.0f;
        auto roll = (lockSetting & Config::HorizonLock::LOCK_ROLL) ? glm::yaw(q) : 0.0f; // somehow in glm the axis is yaw
        glm::quat cancelCarRotation = glm::quat(glm::vec3(pitch, 0.0f, roll));
        *horizonLock = glm::mat4_cast(cancelCarRotation);
    }
    return true;
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

IDirect3DSurface9* PrepareVRRendering(IDirect3DDevice9* dev, RenderTarget tgt, bool clear)
{
    if (dxTexture[tgt]->GetSurfaceLevel(0, &dxSurface[tgt]) != D3D_OK) {
        Dbg("PrepareVRRendering: Failed to get surface level");
        dxSurface[tgt] = nullptr;
        return nullptr;
    }
    if (dev->SetRenderTarget(0, dxSurface[tgt]) != D3D_OK) {
        Dbg("PrepareVRRendering: Failed to set render target");
        dxSurface[tgt]->Release();
        return nullptr;
    }
    if (dev->SetDepthStencilSurface(dxDepthStencilSurface[tgt]) != D3D_OK) {
        Dbg("PrepareVRRendering: Failed to set depth stencil surface");
        dxSurface[tgt]->Release();
        return nullptr;
    }
    if (clear) {
        if (dev->Clear(0, nullptr, D3DCLEAR_STENCIL | D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0, 0) != D3D_OK) {
            Dbg("PrepareVRRendering: Failed to clear surface");
        }
    }
    return dxSurface[tgt];
}

void FinishVRRendering(IDirect3DDevice9* dev, RenderTarget tgt)
{
    if (dxSurface[tgt]) [[likely]] {
        dxSurface[tgt]->Release();
        dxSurface[tgt] = nullptr;
    }
}

void SubmitFramesToHMD(IDirect3DDevice9* dev)
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
    if (gD3DVR->BeginVRSubmit() != D3D_OK) {
        Dbg("BeginVRSubmit failed");
        goto release;
    }
    if (auto e = gCompositor->Submit(static_cast<vr::EVREye>(LeftEye), &openvrTexture[LeftEye]); e != vr::VRCompositorError_None) [[unlikely]] {
        Dbg(std::format("Compositor error: {}", VRCompositorErrorStr(e)));
    }
    if (auto e = gCompositor->Submit(static_cast<vr::EVREye>(RightEye), &openvrTexture[RightEye]); e != vr::VRCompositorError_None) [[unlikely]] {
        Dbg(std::format("Compositor error: {}", VRCompositorErrorStr(e)));
    }
    if (gD3DVR->EndVRSubmit() != D3D_OK) {
        Dbg("EndVRSubmit failed");
    }

release:
    leftEye->Release();
    rightEye->Release();
}

static void RenderTexture(
    IDirect3DDevice9* dev,
    const D3DMATRIX* proj,
    const D3DMATRIX* view,
    const D3DMATRIX* world,
    IDirect3DTexture9* tex,
    IDirect3DVertexBuffer9* vbuf)
{
    IDirect3DVertexShader9* vs;
    IDirect3DPixelShader9* ps;
    D3DMATRIX origProj, origView, origWorld;

    dev->GetVertexShader(&vs);
    dev->GetPixelShader(&ps);

    dev->SetVertexShader(nullptr);
    dev->SetPixelShader(nullptr);

    dev->GetTransform(D3DTS_PROJECTION, &origProj);
    dev->GetTransform(D3DTS_VIEW, &origView);
    dev->GetTransform(D3DTS_WORLD, &origWorld);

    dev->SetTransform(D3DTS_PROJECTION, proj);
    dev->SetTransform(D3DTS_VIEW, view);
    dev->SetTransform(D3DTS_WORLD, world);

    dev->BeginScene();

    IDirect3DBaseTexture9* origTex;
    dev->GetTexture(0, &origTex);

    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

    dev->SetTexture(0, tex);

    dev->SetStreamSource(0, vbuf, 0, sizeof(Vertex));
    dev->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);
    dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

    dev->EndScene();

    dev->SetTexture(0, origTex);
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);
    dev->SetTransform(D3DTS_PROJECTION, &origProj);
    dev->SetTransform(D3DTS_VIEW, &origView);
    dev->SetTransform(D3DTS_WORLD, &origWorld);
}

void RenderMenuQuad(IDirect3DDevice9* dev, RenderTarget renderTarget3D, RenderTarget renderTarget2D, float size, const M4& projection, const glm::vec3& translation, const std::optional<M4>& horizonLock)
{ // TODO: Arguments to this function are a bit silly already
    const D3DMATRIX vr = D3DFromM4(projection * glm::translate(glm::scale(gEyePos[renderTarget3D] * gHMDPose * gFlipZMatrix * horizonLock.value_or(glm::identity<M4>()), { size, size, 1.0f }), translation));
    RenderTexture(dev, &vr, &identityMatrix, &identityMatrix, dxTexture[renderTarget2D], quadVertexBuf[renderTarget2D == Menu ? 0 : 1]);
}

void RenderCompanionWindowFromRenderTarget(IDirect3DDevice9* dev, RenderTarget tgt)
{
    RenderTexture(dev, &identityMatrix, &identityMatrix, &identityMatrix, dxTexture[tgt], companionWindowVertexBuf);
}

std::tuple<uint32_t, uint32_t> GetRenderResolution(RenderTarget tgt)
{
    D3DSURFACE_DESC desc;
    dxTexture[LeftEye]->GetLevelDesc(0, &desc);
    return std::make_tuple(desc.Width, desc.Height);
}

FrameTimingInfo GetFrameTiming()
{
    FrameTimingInfo ret = { 0 };

    vr::Compositor_FrameTiming t {
        .m_nSize = sizeof(vr::Compositor_FrameTiming)
    };
    if (gCompositor->GetFrameTiming(&t)) {
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
