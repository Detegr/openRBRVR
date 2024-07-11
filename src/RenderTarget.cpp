#include "RenderTarget.hpp"
#include "Globals.hpp"

constexpr static bool is_aa_enabled_for_render_target(D3DMULTISAMPLE_TYPE msaa, RenderTarget t)
{
    return msaa > 0 && t < 2;
}

bool is_using_texture_to_render(D3DMULTISAMPLE_TYPE msaa, RenderTarget t)
{
    return !is_aa_enabled_for_render_target(msaa, t);
}

bool create_render_target(
    IDirect3DDevice9* dev,
    D3DMULTISAMPLE_TYPE msaa,
    IDirect3DSurface9** msaa_surface,
    IDirect3DSurface9** depth_stencil_surface,
    IDirect3DTexture9** target_texture,
    HANDLE* shared_handle,
    RenderTarget tgt,
    D3DFORMAT fmt,
    uint32_t w,
    uint32_t h)
{
    HRESULT ret = 0;

    // If anti-aliasing is enabled, we need to first render into an anti-aliased render target.
    // If not, we can render directly to a texture that has D3DUSAGE_RENDERTARGET set.
    if (!is_using_texture_to_render(msaa, tgt)) {
        ret |= dev->CreateRenderTarget(w, h, fmt, msaa, 0, false, msaa_surface, nullptr);
    }
    ret |= dev->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, fmt, D3DPOOL_DEFAULT, target_texture, *shared_handle == nullptr ? nullptr : shared_handle);
    if (ret != D3D_OK || *shared_handle == INVALID_HANDLE_VALUE) {
        dbg("D3D initialization failed: CreateRenderTarget");
        return false;
    }
    static D3DFORMAT depth_stencil_format;
    if (depth_stencil_format == D3DFMT_UNKNOWN) {
        D3DFORMAT wantedFormats[] = {
            D3DFMT_D32F_LOCKABLE,
            D3DFMT_D24S8,
            D3DFMT_D24X8,
            D3DFMT_D16,
        };
        IDirect3D9* d3d;
        if (dev->GetDirect3D(&d3d) != D3D_OK) {
            dbg("Could not get Direct3D adapter");
            depth_stencil_format = D3DFMT_D16;
        } else {
            for (const auto& format : wantedFormats) {
                if (d3d->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, fmt, fmt, format) == D3D_OK) {
                    depth_stencil_format = format;
                    dbg(std::format("Using {} as depthstencil format", (int)format));
                    break;
                }
            }
            if (depth_stencil_format == D3DFMT_UNKNOWN) {
                dbg("No depth stencil format found?? Using D3DFMT_D16");
            }
        }
    }

    ret |= dev->CreateDepthStencilSurface(w, h, depth_stencil_format, msaa, 0, TRUE, depth_stencil_surface, nullptr);
    if (FAILED(ret)) {
        dbg("D3D initialization failed: CreateRenderTarget");
        return false;
    }
    return true;
}
