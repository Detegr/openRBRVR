#include "RenderTarget.hpp"
#include "Globals.hpp"

static bool is_aa_enabled_for_render_target(D3DMULTISAMPLE_TYPE msaa, RenderTarget t)
{
    if (g::vr && g::vr->is_using_quad_view_rendering()) {
        if (t < 2)
            return g::cfg.peripheral_msaa > 0;
        else
            return t < 4 && msaa > 0;
    } else {
        return msaa > 0 && t < 2;
    }
}

bool is_using_texture_to_render(D3DMULTISAMPLE_TYPE msaa, RenderTarget t, bool multiview)
{
    if (multiview && (t == LeftEye || t == RightEye || t == FocusLeft || t == FocusRight)) {
        // We don't use texture to render as we need to render into a layered image
        // and copy out the result anyway.
        return false;
    }
    if (g::vr && (g::vr->get_runtime_type() == OPENXR) && (t == LeftEye || t == RightEye)) {
        // Rendering directly to the texture causes flashing when using OpenXR in D3D11 mode.
        // It is likely that there's something wrong in the Vulkan/D3D11 synchronization, but
        // going through the render target seems to fix most or all of the issues.
        // This demands a bit more performance but the difference is pretty minimal.
        return false;
    }
    return !is_aa_enabled_for_render_target(msaa, t);
}

static D3DMULTISAMPLE_TYPE get_msaa_for_render_target(RenderTarget t, D3DMULTISAMPLE_TYPE msaa)
{
    // Peripheral MSAA for peripheral VR views if quad view rendering is used
    if (g::vr && g::vr->is_using_quad_view_rendering() && t < 2) {
        return g::cfg.peripheral_msaa;
    }

    // Render context's MSAA for VR views
    if (t < 4)
        return msaa;

    return D3DMULTISAMPLE_NONE;
}

bool create_render_target(
    IDirect3DDevice9* dev,
    D3DMULTISAMPLE_TYPE msaa_in,
    IDirect3DSurface9** msaa_surface,
    IDirect3DSurface9** depth_stencil_surface,
    IDirect3DTexture9** target_texture,
    HANDLE* shared_handle,
    RenderTarget tgt,
    D3DFORMAT fmt,
    uint32_t w,
    uint32_t h,
    bool multiview)
{
    HRESULT ret = 0;

    const auto msaa = get_msaa_for_render_target(tgt, msaa_in);

    // If anti-aliasing is enabled, we need to first render into an anti-aliased render target.
    // If not, we can render directly to a texture that has D3DUSAGE_RENDERTARGET set.
    if (!is_using_texture_to_render(msaa, tgt, multiview)) {
        if (multiview) {
            ret |= g::d3d_vr->CreateMultiViewRenderTarget(w, h, fmt, msaa, 0, false, msaa_surface, nullptr, 2);
        } else {
            g::d3d_dev->CreateRenderTarget(w, h, fmt, msaa, 0, false, msaa_surface, nullptr);
        }
    }
    ret |= g::d3d_dev->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, fmt, D3DPOOL_DEFAULT, target_texture, *shared_handle == nullptr ? nullptr : shared_handle);
    if (ret != D3D_OK || *shared_handle == INVALID_HANDLE_VALUE) {
        dbg("D3D initialization failed: CreateRenderTarget");
        return false;
    }
    static D3DFORMAT depth_stencil_format;
    if (depth_stencil_format == D3DFMT_UNKNOWN) {
        // CheckDepthStencilMatch started OK for format that did not actually work when creating the surface
        // I have no clue why, but for now we'll just iterate through the formats one by one and use the best one
        // that we were able to make the surface out of.

        D3DFORMAT wantedFormats[] = {
            D3DFMT_D32F_LOCKABLE,
            D3DFMT_D24S8,
            D3DFMT_D24X8,
            D3DFMT_D16,
        };

        int i;
        for (i = 0; i < 4; i++) {
            HRESULT create_depth_result = -1;
            if (multiview) {
                create_depth_result = g::d3d_vr->CreateMultiViewDepthStencilSurface(w, h, wantedFormats[i], msaa, 0, true, depth_stencil_surface, nullptr, 2);
            } else {
                create_depth_result = g::d3d_dev->CreateDepthStencilSurface(w, h, wantedFormats[i], msaa, 0, true, depth_stencil_surface, nullptr);
            }
            if (SUCCEEDED(create_depth_result)) {
                break;
            }
        }
        depth_stencil_format = wantedFormats[i];
        dbg(std::format("Using {} as depthstencil format", (int)depth_stencil_format));
    } else {
        if (multiview) {
            ret |= g::d3d_vr->CreateMultiViewDepthStencilSurface(w, h, depth_stencil_format, msaa, 0, true, depth_stencil_surface, nullptr, 2);
        } else {
            ret |= g::d3d_dev->CreateDepthStencilSurface(w, h, depth_stencil_format, msaa, 0, true, depth_stencil_surface, nullptr);
        }
        if (FAILED(ret)) {
            dbg("D3D initialization failed: CreateRenderTarget");
            return false;
        }
    }
    return true;
}
