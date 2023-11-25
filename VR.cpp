#include "VR.hpp"
#include "D3D.hpp"
#include "Util.hpp"

#include <format>

IDirect3DVR9* gD3DVR = nullptr;
vr::IVRSystem* gHMD = nullptr;
vr::IVRCompositor* gCompositor = nullptr;
M4 gHMDPose;
M4 gEyePos[2];
M4 gProjection[2];

static IDirect3DTexture9* dxTexture[4];
static IDirect3DSurface9* dxSurface[4];
static IDirect3DSurface9* dxDepthStencilSurface[4];
static vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
static vr::Texture_t openvrTexture[2];
static D3D9_TEXTURE_VR_DESC dxvkTexture[2];
static IDirect3DVertexBuffer9* quadVertexBuf[2];

struct Vertex {
    float x, y, z;
    float u, v; // Texture coordinates
};

constexpr static M4 GetProjectionMatrix(RenderTarget eye)
{
    // Just guesses, seem to work okay
    constexpr auto zNear = 0.15f;
    constexpr auto zFar = 12000.0f;

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

static bool CreateTexture(IDirect3DDevice9* dev, RenderTarget tgt, D3DFORMAT fmt, uint32_t w, uint32_t h)
{
    auto ret = dev->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, fmt, D3DPOOL_DEFAULT, &dxTexture[tgt], nullptr);
    ret |= dev->CreateDepthStencilSurface(w, h, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &dxDepthStencilSurface[tgt], nullptr);
    if (FAILED(ret)) {
        Dbg("D3D initialization failed: CreateTexture");
        return false;
    }
    return true;
}

static bool CreateVRTexture(IDirect3DDevice9* dev, RenderTarget eye, uint32_t w, uint32_t h)
{
    CreateTexture(dev, eye, D3DFMT_A8R8G8B8, w, h);
    openvrTexture[eye].handle = reinterpret_cast<void*>(&dxvkTexture[eye]);
    openvrTexture[eye].eType = vr::TextureType_Vulkan;
    openvrTexture[eye].eColorSpace = vr::ColorSpace_Auto;
    return true;
}

static bool CreateQuad(IDirect3DDevice9* dev, RenderTarget tgt, float size)
{
    // clang-format off
	Vertex quad[] = {
		{ -0.6f * size, 0.3f * size, 1.0f, 0.0f, 0.0f, }, // Top-left
		{ 0.6f * size, 0.3f * size, 1.0f, 1.0f, 0.0f, }, // Top-right
		{ -0.6f * size, -0.3f * size, 1.0f, 0.0f, 1.0f }, // Bottom-left
		{ 0.6f * size, -0.3f * size, 1.0f, 1.0f, 1.0f } // Bottom-right
	};

    // We have only 2 quads, so indexing by RenderTarget directly does not work here
    auto tgtIdx = tgt == Menu ? 0 : 1;

    // clang-format on
    if (auto ret = dev->CreateVertexBuffer(sizeof(quad), 0, D3DFVF_XYZ | D3DFVF_TEX1, D3DPOOL_MANAGED, &quadVertexBuf[tgtIdx], nullptr); FAILED(ret)) {
        Dbg("D3D initialization failed: CreateVertexBuffer");
        return false;
    }
    void* bufmem;
    quadVertexBuf[tgtIdx]->Lock(0, sizeof(quad), &bufmem, 0);
    memcpy(bufmem, quad, sizeof(quad));
    quadVertexBuf[tgtIdx]->Unlock();

    return true;
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

    gProjection[LeftEye] = GetProjectionMatrix(LeftEye);
    gProjection[RightEye] = GetProjectionMatrix(RightEye);
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
    if (!CreateQuad(dev, Menu, cfg.menuSize))
        return false;
    if (!CreateQuad(dev, Overlay, cfg.overlaySize))
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

bool UpdateVRPoses()
{
    if (auto e = gCompositor->WaitGetPoses(poses, vr::k_unMaxTrackedDeviceCount, nullptr, 0); e != vr::VRCompositorError_None) {
        Dbg("Could not get VR poses");
        return false;
    }
    auto pose = &poses[vr::k_unTrackedDeviceIndex_Hmd];
    if (pose->bPoseIsValid) {
        gHMDPose = glm::inverse(M4FromSteamVRMatrix(pose->mDeviceToAbsoluteTracking));
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
    dxTexture[tgt]->GetSurfaceLevel(0, &dxSurface[tgt]);
    dev->SetRenderTarget(0, dxSurface[tgt]);
    dev->SetDepthStencilSurface(dxDepthStencilSurface[tgt]);
    if (clear) {
        dev->Clear(0, nullptr, D3DCLEAR_STENCIL | D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0, 0);
    }
    return dxSurface[tgt];
}

void FinishVRRendering(IDirect3DDevice9* dev, RenderTarget tgt)
{
    dxSurface[tgt]->Release();
}

void SubmitFramesToHMD()
{
    gD3DVR->TransferSurfaceForVR(dxSurface[LeftEye]);
    gD3DVR->TransferSurfaceForVR(dxSurface[RightEye]);
    gD3DVR->GetVRDesc(dxSurface[LeftEye], &dxvkTexture[LeftEye]);
    gD3DVR->GetVRDesc(dxSurface[RightEye], &dxvkTexture[RightEye]);
    gD3DVR->BeginVRSubmit();
    if (auto e = gCompositor->Submit(static_cast<vr::EVREye>(LeftEye), &openvrTexture[LeftEye]); e != vr::VRCompositorError_None) [[unlikely]] {
        Dbg(std::format("Compositor error: {}", VRCompositorErrorStr(e)));
    }
    if (auto e = gCompositor->Submit(static_cast<vr::EVREye>(RightEye), &openvrTexture[RightEye]); e != vr::VRCompositorError_None) [[unlikely]] {
        Dbg(std::format("Compositor error: {}", VRCompositorErrorStr(e)));
    }
    gD3DVR->EndVRSubmit();
}

void RenderMenuQuad(IDirect3DDevice9* dev, RenderTarget renderTarget3D, RenderTarget renderTarget2D)
{
    IDirect3DVertexShader9* vs;
    IDirect3DPixelShader9* ps;
    D3DMATRIX proj, view, identity, world;
    identity = D3DFromM4(glm::identity<glm::mat4x4>());

    dev->GetVertexShader(&vs);
    dev->GetPixelShader(&ps);
    dev->GetTransform(D3DTS_PROJECTION, &proj);
    dev->GetTransform(D3DTS_VIEW, &view);
    dev->GetTransform(D3DTS_WORLD, &world);

    D3DMATRIX vr;
    vr = D3DFromM4(gProjection[renderTarget3D] * gEyePos[renderTarget3D] * gHMDPose);

    dev->SetVertexShader(nullptr);
    dev->SetPixelShader(nullptr);

    dev->SetTransform(D3DTS_PROJECTION, &vr);
    dev->SetTransform(D3DTS_VIEW, &identity);
    dev->SetTransform(D3DTS_WORLD, &identity);

    dev->BeginScene();

    IDirect3DBaseTexture9* tex;
    dev->GetTexture(0, &tex);

    dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

    dev->SetTexture(0, dxTexture[renderTarget2D]);

    dev->SetStreamSource(0, quadVertexBuf[renderTarget2D == Menu ? 0 : 1], 0, sizeof(Vertex));
    dev->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);
    dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

    dev->EndScene();

    dev->SetTexture(0, tex);
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);
    dev->SetTransform(D3DTS_PROJECTION, &proj);
    dev->SetTransform(D3DTS_VIEW, &view);
    dev->SetTransform(D3DTS_WORLD, &world);
}

std::tuple<uint32_t, uint32_t> GetRenderResolution(RenderTarget tgt)
{
    D3DSURFACE_DESC desc;
    dxTexture[LeftEye]->GetLevelDesc(0, &desc);
    return std::make_tuple(desc.Width, desc.Height);
}
