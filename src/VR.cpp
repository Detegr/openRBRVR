#include "VR.hpp"
#include "Config.hpp"
#include "Globals.hpp"
#include "OpenVR.hpp"
#include "OpenXR.hpp"

#include "D3D.hpp"
#include "Util.hpp"
#include "Vertex.hpp"

#include <format>
#include <vector>

// Compliation unit global variables
namespace g {
    static IDirect3DVertexBuffer9* companion_window_vertex_buf_menu;
    static IDirect3DVertexBuffer9* companion_window_vertex_buf_3d;
    static constexpr D3DMATRIX identity_matrix = d3d_from_m4(glm::identity<glm::mat4x4>());
    static IDirect3DVertexBuffer9* quad_vertex_buf[2];
    static IDirect3DVertexBuffer9* overlay_border_quad;
}

bool VRInterface::is_using_quad_view_rendering() const
{
    return get_runtime_type() == OPENXR && g::cfg.quad_view_rendering;
}

void VRInterface::set_render_context(const std::string& name)
{
    current_render_context = &render_contexts[name];

    // Per-stage setting of multiview rendering
    if (g::cfg.multiview != g::vr->get_current_render_context()->multiview_rendering) {
        g::cfg.multiview = g::vr->get_current_render_context()->multiview_rendering;
        g::d3d_vr->EnableMultiView(g::cfg.multiview);
    }
}

static bool create_menu_screen_companion_window_buffer(IDirect3DDevice9* dev)
{
    // clang-format off
    Vertex quad[] = {
        { -1, 1, 1, 0, 0, }, // Top-left
        { 1, 1, 1, 1, 0, }, // Top-right
        { -1, -1, 1, 0, 1 }, // Bottom-left
        { 1, -1, 1, 1, 1 } // Bottom-right
    };
    // clang-format on
    if (!create_vertex_buffer(dev, quad, 4, &g::companion_window_vertex_buf_menu)) {
        dbg("Could not create vertex buffer for companion window");
        return false;
    }
    return true;
}

bool VRInterface::create_companion_window_buffer(IDirect3DDevice9* dev)
{
    const auto size = g::cfg.companion_size / 100.0f;
    const auto x = g::cfg.companion_offset.x / 100.0f;
    const auto y = g::cfg.companion_offset.y / 100.0f;
    const auto aspect = static_cast<float>(companion_window_aspect_ratio);

    // clang-format off
    Vertex quad[] = {
        { -1, 1, 1, x, y, }, // Top-left
        { 1, 1, 1, x+size, y, }, // Top-right
        { -1, -1, 1, x, y+size / aspect }, // Bottom-left
        { 1, -1, 1, x+size, y+size / aspect } // Bottom-right
    };
    // clang-format on
    if (g::companion_window_vertex_buf_3d) {
        g::companion_window_vertex_buf_3d->Release();
    }
    if (!create_vertex_buffer(dev, quad, 4, &g::companion_window_vertex_buf_3d)) {
        dbg("Could not create vertex buffer for companion window");
        return false;
    }
    return true;
}

bool create_quad(IDirect3DDevice9* dev, float size, float aspect, IDirect3DVertexBuffer9** dst)
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

    return create_vertex_buffer(dev, quad, 4, dst);
}

IDirect3DSurface9* VRInterface::prepare_vr_rendering(IDirect3DDevice9* dev, RenderTarget tgt, bool clear)
{
    if (is_using_texture_to_render(current_render_context->msaa, tgt, g::cfg.multiview)) {
        if (current_render_context->dx_texture[tgt]->GetSurfaceLevel(0, &current_render_context->dx_surface[tgt]) != D3D_OK) {
            dbg("PrepareVRRendering: Failed to get surface level");
            current_render_context->dx_surface[tgt] = nullptr;
            return nullptr;
        }
    }
    if (dev->SetRenderTarget(0, current_render_context->dx_surface[tgt]) != D3D_OK) {
        dbg("PrepareVRRendering: Failed to set render target");
        finish_vr_rendering(dev, tgt);
        return nullptr;
    }
    if (dev->SetDepthStencilSurface(current_render_context->dx_depth_stencil_surface[tgt]) != D3D_OK) {
        dbg("PrepareVRRendering: Failed to set depth stencil surface");
        finish_vr_rendering(dev, tgt);
        return nullptr;
    }
    if (clear) {
        if (dev->Clear(0, nullptr, D3DCLEAR_STENCIL | D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0, 0) != D3D_OK) {
            dbg("PrepareVRRendering: Failed to clear surface");
        }
    }
    return current_render_context->dx_surface[tgt];
}

void VRInterface::finish_vr_rendering(IDirect3DDevice9* dev, RenderTarget tgt)
{
    if (is_using_texture_to_render(current_render_context->msaa, tgt, g::cfg.multiview) && current_render_context->dx_surface[tgt]) {
        current_render_context->dx_surface[tgt]->Release();
        current_render_context->dx_surface[tgt] = nullptr;
    }
}

static bool create_render_target(IDirect3DDevice9* dev, D3DMULTISAMPLE_TYPE msaa, RenderContext& ctx, RenderTarget tgt, D3DFORMAT fmt, uint32_t w, uint32_t h, bool multiview)
{
    HANDLE null_handle = nullptr;
    HANDLE* shared_handle = (tgt == LeftEye || tgt == RightEye || tgt == FocusLeft || tgt == FocusRight) ? &ctx.dx_shared_handle[tgt] : &null_handle;
    return create_render_target(dev, msaa, &ctx.dx_surface[tgt], &ctx.dx_depth_stencil_surface[tgt], &ctx.dx_texture[tgt], shared_handle, tgt, fmt, w, h, multiview);
}

void VRInterface::init_surfaces(IDirect3DDevice9* dev, RenderContext& ctx, uint32_t res_x_2d, uint32_t res_y_2d)
{
    const auto create_vr_render_target = [&](RenderTarget tgt) {
        if (!create_render_target(dev, ctx.msaa, ctx, tgt, D3DFMT_X8B8G8R8, ctx.width[tgt], ctx.height[tgt], ctx.multiview_rendering)) {
            throw std::runtime_error(std::format("Could not create VR render target for view: {}", static_cast<int>(tgt)));
        }
    };

    create_vr_render_target(LeftEye);
    create_vr_render_target(RightEye);
    if (is_using_quad_view_rendering()) {
        create_vr_render_target(FocusLeft);
        create_vr_render_target(FocusRight);
    }

    if (!create_render_target(dev, D3DMULTISAMPLE_NONE, ctx, GameMenu, D3DFMT_X8B8G8R8, res_x_2d, res_y_2d, false))
        throw std::runtime_error("Could not create texture for menus");
    if (!create_render_target(dev, D3DMULTISAMPLE_NONE, ctx, Overlay, D3DFMT_A8B8G8R8, res_x_2d, res_y_2d, false))
        throw std::runtime_error("Could not create texture for overlay");
    if (dev->CreateTexture(res_x_2d, res_y_2d, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8B8G8R8, D3DPOOL_DEFAULT, &ctx.overlay_border, nullptr) != D3D_OK)
        throw std::runtime_error("Could not create overlay border texture");

    this->companion_window_width = res_x_2d;
    this->companion_window_height = res_y_2d;
    auto aspect_ratio = static_cast<float>(static_cast<double>(res_x_2d) / static_cast<double>(res_y_2d));
    static bool quads_created = false;
    if (!quads_created) {
        this->companion_window_aspect_ratio = aspect_ratio;

        // Create and fill a vertex buffers for the 2D planes
        // We can reuse all of these in every rendering context
        if (!create_quad(dev, 0.6f, aspect_ratio, &g::quad_vertex_buf[0]))
            throw std::runtime_error("Could not create menu quad");
        if (!create_quad(dev, 0.6f, aspect_ratio, &g::quad_vertex_buf[1]))
            throw std::runtime_error("Could not create overlay quad");
        if (!create_quad(dev, 1.0f, 1.0f, &g::overlay_border_quad))
            throw std::runtime_error("Could not create overlay border quad");
        if (!create_companion_window_buffer(dev))
            throw std::runtime_error("Could not create desktop window buffer");
        if (!create_menu_screen_companion_window_buffer(dev))
            throw std::runtime_error("Could not create menu screen desktop window buffer");

        quads_created = true;
    }

    // Render overlay border to a texture for later use
    IDirect3DSurface9* adj;
    if (ctx.overlay_border->GetSurfaceLevel(0, &adj) == D3D_OK) {
        IDirect3DSurface9* orig;
        dev->GetRenderTarget(0, &orig);
        dev->SetRenderTarget(0, adj);
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_RGBA(255, 69, 0, 50), 1.0, 0);
        constexpr auto borderSize = 0.02;
        const D3DRECT center = {
            static_cast<LONG>(borderSize / aspect_ratio * res_x_2d),
            static_cast<LONG>(borderSize * res_y_2d),
            static_cast<LONG>((1.0 - borderSize / aspect_ratio) * res_x_2d),
            static_cast<LONG>((1.0 - borderSize) * res_y_2d)
        };
        dev->Clear(1, &center, D3DCLEAR_TARGET, D3DCOLOR_RGBA(0, 0, 0, 0), 1.0, 0);
        adj->Release();
        dev->SetRenderTarget(0, orig);
        orig->Release();
    }
}

static void render_texture(
    IDirect3DDevice9* dev,
    const D3DMATRIX* proj,
    const D3DMATRIX* proj_multiview,
    const D3DMATRIX* view,
    const D3DMATRIX* world,
    IDirect3DTexture9* tex,
    IDirect3DVertexBuffer9* vbuf)
{
    IDirect3DVertexShader9* vs;
    IDirect3DPixelShader9* ps;
    D3DMATRIX origProj, origProj2, origView, origView2, origWorld;

    dev->GetVertexShader(&vs);
    dev->GetPixelShader(&ps);

    dev->SetVertexShader(nullptr);
    dev->SetPixelShader(nullptr);

    dev->GetTransform(D3DTS_PROJECTION_LEFT, &origProj);
    dev->GetTransform(D3DTS_VIEW_LEFT, &origView);
    dev->GetTransform(D3DTS_WORLD, &origWorld);

    dev->SetTransform(D3DTS_PROJECTION_LEFT, proj);
    dev->SetTransform(D3DTS_VIEW_LEFT, view);
    if (g::cfg.multiview) {
        dev->GetTransform(D3DTS_PROJECTION_RIGHT, &origProj2);
        dev->GetTransform(D3DTS_VIEW_RIGHT, &origView2);
        dev->SetTransform(D3DTS_PROJECTION_RIGHT, proj_multiview);
        dev->SetTransform(D3DTS_VIEW_RIGHT, view);
    }
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
    dev->SetTransform(D3DTS_PROJECTION_LEFT, &origProj);
    dev->SetTransform(D3DTS_VIEW_LEFT, &origView);
    if (g::cfg.multiview) {
        dev->SetTransform(D3DTS_PROJECTION_RIGHT, &origProj2);
        dev->SetTransform(D3DTS_VIEW_RIGHT, &origView2);
    }
    dev->SetTransform(D3DTS_WORLD, &origWorld);
}

void render_overlay_border(IDirect3DDevice9* dev, IDirect3DTexture9* tex)
{
    render_texture(dev, &g::identity_matrix, &g::identity_matrix, &g::identity_matrix, &g::identity_matrix, tex, g::overlay_border_quad);
}

void render_menu_quad(IDirect3DDevice9* dev, VRInterface* vr, IDirect3DTexture9* texture, RenderTarget render_target_3d, RenderTarget render_target_2d, float size, glm::vec3 translation, const std::optional<M4>& horizon_lock)
{
    const auto left = render_target_3d;

    const auto& projectionl = vr->get_projection(left);
    const auto& eyeposl = vr->get_eye_pos(left);
    const auto& posel = vr->get_pose(left);

    D3DMATRIX mvpr = { 0 };
    if (g::cfg.multiview) {
        const auto right = render_target_counterpart(left);
        const auto& projectionr = vr->get_projection(right);
        const auto& eyeposr = vr->get_eye_pos(right);
        const auto& poser = vr->get_pose(right);
        mvpr = d3d_from_m4(projectionr * glm::translate(glm::scale(eyeposr * poser * g::flip_z_matrix * horizon_lock.value_or(glm::identity<M4>()), { size, size, 1.0f }), translation));
    }

    const D3DMATRIX mvpl = d3d_from_m4(projectionl * glm::translate(glm::scale(eyeposl * posel * g::flip_z_matrix * horizon_lock.value_or(glm::identity<M4>()), { size, size, 1.0f }), translation));
    render_texture(dev, &mvpl, &mvpr, &g::identity_matrix, &g::identity_matrix, texture, g::quad_vertex_buf[render_target_2d == GameMenu ? 0 : 1]);
}

void render_companion_window_from_render_target(IDirect3DDevice9* dev, VRInterface* vr, RenderTarget tgt)
{
    render_texture(dev, &g::identity_matrix, &g::identity_matrix, &g::identity_matrix, &g::identity_matrix, vr->get_texture(tgt), (tgt == GameMenu || tgt == Overlay) ? g::companion_window_vertex_buf_menu : g::companion_window_vertex_buf_3d);
}
