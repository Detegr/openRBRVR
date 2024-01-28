#include "VR.hpp"
#include "Config.hpp"
#include "OpenVR.hpp"
#include "OpenXR.hpp"

#include "D3D.hpp"
#include "Util.hpp"
#include "Vertex.hpp"

#include <format>
#include <vector>

extern Config gCfg;

IDirect3DVR9* gD3DVR = nullptr;

static IDirect3DVertexBuffer9 *companionWindowVertexBufMenu, *companionWindowVertexBuf3D;
static constexpr D3DMATRIX identityMatrix = D3DFromM4(glm::identity<glm::mat4x4>());
static IDirect3DVertexBuffer9* quadVertexBuf[2];
static IDirect3DVertexBuffer9* overlayBorderQuad;

constexpr static bool IsAAEnabledForRenderTarget(RenderTarget t)
{
    return gCfg.msaa > 0 && t < 2;
}

bool VRInterface::IsUsingTextureToRender(RenderTarget t)
{
    return !(IsAAEnabledForRenderTarget(t) || (GetRuntimeType() == OPENXR && t < 2));
}

static bool CreateMenuScreenCompanionWindowBuffer(IDirect3DDevice9* dev)
{
    // clang-format off
    Vertex quad[] = {
        { -1, 1, 1, 0, 0, }, // Top-left
        { 1, 1, 1, 1, 0, }, // Top-right
        { -1, -1, 1, 0, 1 }, // Bottom-left
        { 1, -1, 1, 1, 1 } // Bottom-right
    };
    // clang-format on
    if (!CreateVertexBuffer(dev, quad, 4, &companionWindowVertexBufMenu)) {
        Dbg("Could not create vertex buffer for companion window");
        return false;
    }
    return true;
}

bool VRInterface::CreateCompanionWindowBuffer(IDirect3DDevice9* dev)
{
    const auto size = static_cast<float>(gCfg.companionSize);
    const auto x = static_cast<float>(gCfg.companionOffset.x);
    const auto y = static_cast<float>(gCfg.companionOffset.y);
    const auto aspect = static_cast<float>(aspectRatio);

    // clang-format off
    Vertex quad[] = {
        { -1, 1, 1, x, y, }, // Top-left
        { 1, 1, 1, x+size, y, }, // Top-right
        { -1, -1, 1, x, y+size / aspect }, // Bottom-left
        { 1, -1, 1, x+size, y+size / aspect } // Bottom-right
    };
    // clang-format on
    if (companionWindowVertexBuf3D) {
        companionWindowVertexBuf3D->Release();
    }
    if (!CreateVertexBuffer(dev, quad, 4, &companionWindowVertexBuf3D)) {
        Dbg("Could not create vertex buffer for companion window");
        return false;
    }
    return true;
}

bool VRInterface::CreateRenderTarget(IDirect3DDevice9* dev, RenderContext& ctx, RenderTarget tgt, D3DFORMAT fmt, uint32_t w, uint32_t h)
{
    HRESULT ret = 0;

    // If anti-aliasing is enabled, we need to first render into an anti-aliased render target.
    // If not, we can render directly to a texture that has D3DUSAGE_RENDERTARGET set.
    if (!IsUsingTextureToRender(tgt)) {
        ret |= dev->CreateRenderTarget(w, h, fmt, gCfg.msaa, 0, false, &ctx.dxSurface[tgt], nullptr);
    }
    ret |= dev->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, fmt, D3DPOOL_DEFAULT, &ctx.dxTexture[tgt], nullptr);
    if (ret != D3D_OK) {
        Dbg("D3D initialization failed: CreateRenderTarget");
        return false;
    }
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
    auto msaa = IsAAEnabledForRenderTarget(tgt) ? gCfg.msaa : D3DMULTISAMPLE_NONE;
    ret |= dev->CreateDepthStencilSurface(w, h, depthStencilFormat, msaa, 0, TRUE, &ctx.dxDepthStencilSurface[tgt], nullptr);
    if (FAILED(ret)) {
        Dbg("D3D initialization failed: CreateRenderTarget");
        return false;
    }
    return true;
}

bool CreateQuad(IDirect3DDevice9* dev, float size, float aspect, IDirect3DVertexBuffer9** dst)
{
    auto w = size;
    const auto h = w / aspect;
    constexpr auto left = 0.0f;
    constexpr auto rght = 1.0f;
    constexpr auto top = 0.0f;
    constexpr auto btm = 1.0f;
    constexpr auto z = 1.0f;
    // clang-format off
    Vertex quad[] = {
        { -w,  h, z, left, top, },
        {  w,  h, z, rght, top, },
        { -w, -h, z, left, btm  },
        {  w, -h, z, rght, btm  }
    };
    // clang-format on

    return CreateVertexBuffer(dev, quad, 4, dst);
}

IDirect3DSurface9* VRInterface::PrepareVRRendering(IDirect3DDevice9* dev, RenderTarget tgt, bool clear)
{
    if (IsUsingTextureToRender(tgt)) {
        if (currentRenderContext->dxTexture[tgt]->GetSurfaceLevel(0, &currentRenderContext->dxSurface[tgt]) != D3D_OK) {
            Dbg("PrepareVRRendering: Failed to get surface level");
            currentRenderContext->dxSurface[tgt] = nullptr;
            return nullptr;
        }
    }
    if (dev->SetRenderTarget(0, currentRenderContext->dxSurface[tgt]) != D3D_OK) {
        Dbg("PrepareVRRendering: Failed to set render target");
        FinishVRRendering(dev, tgt);
        return nullptr;
    }
    if (dev->SetDepthStencilSurface(currentRenderContext->dxDepthStencilSurface[tgt]) != D3D_OK) {
        Dbg("PrepareVRRendering: Failed to set depth stencil surface");
        FinishVRRendering(dev, tgt);
        return nullptr;
    }
    if (clear) {
        if (dev->Clear(0, nullptr, D3DCLEAR_STENCIL | D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0, 0) != D3D_OK) {
            Dbg("PrepareVRRendering: Failed to clear surface");
        }
    }
    return currentRenderContext->dxSurface[tgt];
}

void VRInterface::FinishVRRendering(IDirect3DDevice9* dev, RenderTarget tgt)
{
    if (IsUsingTextureToRender(tgt) && currentRenderContext->dxSurface[tgt]) {
        currentRenderContext->dxSurface[tgt]->Release();
        currentRenderContext->dxSurface[tgt] = nullptr;
    }
}

void VRInterface::InitSurfaces(IDirect3DDevice9* dev, RenderContext& ctx, uint32_t resx2d, uint32_t resy2d)
{
    if (!CreateRenderTarget(dev, ctx, LeftEye, D3DFMT_X8B8G8R8, ctx.width[0], ctx.height[0]))
        throw std::runtime_error("Could not create texture for left eye");
    if (!CreateRenderTarget(dev, ctx, RightEye, D3DFMT_X8B8G8R8, ctx.width[1], ctx.height[1]))
        throw std::runtime_error("Could not create texture for right eye");
    if (!CreateRenderTarget(dev, ctx, Menu, D3DFMT_X8B8G8R8, resx2d, resy2d))
        throw std::runtime_error("Could not create texture for menus");
    if (!CreateRenderTarget(dev, ctx, Overlay, D3DFMT_A8B8G8R8, resx2d, resy2d))
        throw std::runtime_error("Could not create texture for overlay");
    if (dev->CreateTexture(resx2d, resy2d, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8B8G8R8, D3DPOOL_DEFAULT, &ctx.overlayBorder, nullptr) != D3D_OK)
        throw std::runtime_error("Could not create overlay border texture");

    auto aspectRatio = static_cast<float>(static_cast<double>(resx2d) / static_cast<double>(resy2d));
    static bool quadsCreated = false;
    if (!quadsCreated) {
        this->aspectRatio = aspectRatio;

        // Create and fill a vertex buffers for the 2D planes
        // We can reuse all of these in every rendering context
        if (!CreateQuad(dev, 0.6f, aspectRatio, &quadVertexBuf[0]))
            throw std::runtime_error("Could not create menu quad");
        if (!CreateQuad(dev, 0.6f, aspectRatio, &quadVertexBuf[1]))
            throw std::runtime_error("Could not create overlay quad");
        if (!CreateQuad(dev, 1.0f, 1.0f, &overlayBorderQuad))
            throw std::runtime_error("Could not create overlay border quad");
        if (!CreateCompanionWindowBuffer(dev))
            throw std::runtime_error("Could not create desktop window buffer");
        if (!CreateMenuScreenCompanionWindowBuffer(dev))
            throw std::runtime_error("Could not create menu screen desktop window buffer");

        quadsCreated = true;
    }

    // Render overlay border to a texture for later use
    IDirect3DSurface9* adj;
    if (ctx.overlayBorder->GetSurfaceLevel(0, &adj) == D3D_OK) {
        IDirect3DSurface9* orig;
        dev->GetRenderTarget(0, &orig);
        dev->SetRenderTarget(0, adj);
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_RGBA(255, 69, 0, 50), 1.0, 0);
        constexpr auto borderSize = 0.02;
        const D3DRECT center = {
            static_cast<LONG>(borderSize / aspectRatio * resx2d),
            static_cast<LONG>(borderSize * resy2d),
            static_cast<LONG>((1.0 - borderSize / aspectRatio) * resx2d),
            static_cast<LONG>((1.0 - borderSize) * resy2d)
        };
        dev->Clear(1, &center, D3DCLEAR_TARGET, D3DCOLOR_RGBA(0, 0, 0, 0), 1.0, 0);
        adj->Release();
        dev->SetRenderTarget(0, orig);
        orig->Release();
    }
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

    dev->SetRenderState(D3DRS_ZENABLE, false);

    dev->SetTexture(0, tex);

    dev->SetStreamSource(0, vbuf, 0, sizeof(Vertex));
    dev->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);
    dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

    dev->EndScene();

    dev->SetRenderState(D3DRS_ZENABLE, true);
    dev->SetTexture(0, origTex);
    dev->SetVertexShader(vs);
    dev->SetPixelShader(ps);
    dev->SetTransform(D3DTS_PROJECTION, &origProj);
    dev->SetTransform(D3DTS_VIEW, &origView);
    dev->SetTransform(D3DTS_WORLD, &origWorld);
}

void RenderOverlayBorder(IDirect3DDevice9* dev, IDirect3DTexture9* tex)
{
    RenderTexture(dev, &identityMatrix, &identityMatrix, &identityMatrix, tex, overlayBorderQuad);
}

void RenderMenuQuad(IDirect3DDevice9* dev, VRInterface* vr, IDirect3DTexture9* texture, RenderTarget renderTarget3D, RenderTarget renderTarget2D, Projection projType, float size, glm::vec3 translation, const std::optional<M4>& horizonLock)
{
    const auto& projection = vr->GetProjection(renderTarget3D, projType);
    const auto& eyepos = vr->GetEyePos(renderTarget3D);
    const auto& pose = vr->GetPose(renderTarget3D);

    const D3DMATRIX mvp = D3DFromM4(projection * glm::translate(glm::scale(eyepos * pose * gFlipZMatrix * horizonLock.value_or(glm::identity<M4>()), { size, size, 1.0f }), translation));
    RenderTexture(dev, &mvp, &identityMatrix, &identityMatrix, texture, quadVertexBuf[renderTarget2D == Menu ? 0 : 1]);
}

void RenderCompanionWindowFromRenderTarget(IDirect3DDevice9* dev, VRInterface* vr, RenderTarget tgt)
{
    RenderTexture(dev, &identityMatrix, &identityMatrix, &identityMatrix, vr->GetTexture(tgt), tgt == Menu ? companionWindowVertexBufMenu : companionWindowVertexBuf3D);
}

M4 GetHorizonLockMatrix(Quaternion* carQuat, HorizonLock lockSetting)
{
    if (carQuat) {
        // If car quaternion is given, calculate matrix for locking the horizon
        glm::quat q(carQuat->w, carQuat->x, carQuat->y, carQuat->z);
        glm::vec3 ang = glm::eulerAngles(q);
        auto pitch = (lockSetting & HorizonLock::LOCK_PITCH) ? glm::pitch(q) : 0.0f;
        auto roll = (lockSetting & HorizonLock::LOCK_ROLL) ? glm::yaw(q) : 0.0f; // somehow in glm the axis is yaw
        glm::quat cancelCarRotation = glm::normalize(glm::quat(glm::vec3(pitch, 0.0f, roll))) * static_cast<float>(gCfg.horizonLockMultiplier);
        return glm::mat4_cast(cancelCarRotation);
    } else {
        return glm::identity<M4>();
    }
}
