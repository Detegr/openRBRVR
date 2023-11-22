#include "VR.hpp"
#include "OpenVR.hpp"
#include "OpenXR.hpp"

#include "D3D.hpp"
#include "Util.hpp"
#include "Vertex.hpp"

#include <format>
#include <vector>

extern Config gCfg;

IDirect3DVR9* gD3DVR = nullptr;

static IDirect3DVertexBuffer9* companionWindowVertexBuf;
static constexpr D3DMATRIX identityMatrix = D3DFromM4(glm::identity<glm::mat4x4>());
static IDirect3DVertexBuffer9* quadVertexBuf[2];

constexpr static bool IsAAEnabledForRenderTarget(RenderTarget t)
{
    return gCfg.msaa > 0 && t < 2;
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

bool VRInterface::CreateRenderTarget(IDirect3DDevice9* dev, RenderTarget tgt, D3DFORMAT fmt, uint32_t w, uint32_t h)
{
    HRESULT ret = 0;
    // If anti-aliasing is enabled, we need to first render into an anti-aliased render target.
    // If not, we can render directly to a texture that has D3DUSAGE_RENDERTARGET set.
    if (IsAAEnabledForRenderTarget(tgt)) {
        ret |= dev->CreateRenderTarget(w, h, fmt, gCfg.msaa, 0, false, &dxSurface[tgt], nullptr);
    }
    ret |= dev->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, fmt, D3DPOOL_DEFAULT, &dxTexture[tgt], nullptr);
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
    ret |= dev->CreateDepthStencilSurface(w, h, depthStencilFormat, msaa, 0, TRUE, &dxDepthStencilSurface[tgt], nullptr);
    if (FAILED(ret)) {
        Dbg("D3D initialization failed: CreateRenderTarget");
        return false;
    }
    return true;
}

bool CreateQuad(IDirect3DDevice9* dev, RenderTarget tgt, float aspect)
{
    constexpr auto w = 0.6f;
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

    // We have only 2 quads, so indexing by RenderTarget directly does not work here
    auto tgtIdx = tgt == Menu ? 0 : 1;

    return CreateVertexBuffer(dev, quad, 4, &quadVertexBuf[tgtIdx]);
}

IDirect3DSurface9* VRInterface::PrepareVRRendering(IDirect3DDevice9* dev, RenderTarget tgt, bool clear)
{
    if (!IsAAEnabledForRenderTarget(tgt)) {
        if (dxTexture[tgt]->GetSurfaceLevel(0, &dxSurface[tgt]) != D3D_OK) {
            Dbg("PrepareVRRendering: Failed to get surface level");
            dxSurface[tgt] = nullptr;
            return nullptr;
        }
    }
    if (dev->SetRenderTarget(0, dxSurface[tgt]) != D3D_OK) {
        Dbg("PrepareVRRendering: Failed to set render target");
        FinishVRRendering(dev, tgt);
        return nullptr;
    }
    if (dev->SetDepthStencilSurface(dxDepthStencilSurface[tgt]) != D3D_OK) {
        Dbg("PrepareVRRendering: Failed to set depth stencil surface");
        FinishVRRendering(dev, tgt);
        return nullptr;
    }
    if (clear) {
        if (dev->Clear(0, nullptr, D3DCLEAR_STENCIL | D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0, 0) != D3D_OK) {
            Dbg("PrepareVRRendering: Failed to clear surface");
        }
    }
    return dxSurface[tgt];
}

void VRInterface::FinishVRRendering(IDirect3DDevice9* dev, RenderTarget tgt)
{
    if (!IsAAEnabledForRenderTarget(tgt) && dxSurface[tgt]) {
        dxSurface[tgt]->Release();
        dxSurface[tgt] = nullptr;
    }
}

void VRInterface::InitSurfaces(IDirect3DDevice9* dev, uint32_t resx, uint32_t resy, uint32_t resx2d, uint32_t resy2d)
{
    if (!CreateRenderTarget(dev, LeftEye, D3DFMT_X8B8G8R8, resx, resy))
        throw std::runtime_error("Could not create texture for left eye");
    if (!CreateRenderTarget(dev, RightEye, D3DFMT_X8B8G8R8, resx, resy))
        throw std::runtime_error("Could not create texture for right eye");
    if (!CreateRenderTarget(dev, Menu, D3DFMT_X8B8G8R8, resx2d, resy2d))
        throw std::runtime_error("Could not create texture for menus");
    if (!CreateRenderTarget(dev, Overlay, D3DFMT_A8B8G8R8, resx2d, resy2d))
        throw std::runtime_error("Could not create texture for overlay");

    auto aspectRatio = static_cast<float>(static_cast<double>(resx2d) / static_cast<double>(resy2d));

    // Create and fill a vertex buffers for the 2D planes
    if (!CreateQuad(dev, Menu, aspectRatio))
        throw std::runtime_error("Could not create menu quad");
    if (!CreateQuad(dev, Overlay, aspectRatio))
        throw std::runtime_error("Could not create overlay quad");

    if (!CreateCompanionWindowBuffer(dev))
        throw std::runtime_error("Could not create desktop window buffer");
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

void RenderMenuQuad(IDirect3DDevice9* dev, VRInterface* vr, RenderTarget renderTarget3D, RenderTarget renderTarget2D, Projection projType, float size, glm::vec3 translation, const std::optional<M4>& horizonLock)
{
    const auto& projection = vr->GetProjection(renderTarget3D, projType);
    const auto& eyepos = vr->GetEyePos(renderTarget3D);
    const auto& pose = vr->GetPose();
    const auto& texture = vr->GetTexture(renderTarget2D);

    const D3DMATRIX mvp = D3DFromM4(projection * glm::translate(glm::scale(eyepos * pose * gFlipZMatrix * horizonLock.value_or(glm::identity<M4>()), { size, size, 1.0f }), translation));
    RenderTexture(dev, &mvp, &identityMatrix, &identityMatrix, texture, quadVertexBuf[renderTarget2D == Menu ? 0 : 1]);
}

void RenderCompanionWindowFromRenderTarget(IDirect3DDevice9* dev, VRInterface* vr, RenderTarget tgt)
{
    RenderTexture(dev, &identityMatrix, &identityMatrix, &identityMatrix, vr->GetTexture(tgt), companionWindowVertexBuf);
}
