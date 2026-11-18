#include "VR.hpp"
#include "Config.hpp"
#include "Dx.hpp"
#include "Globals.hpp"

#include "Util.hpp"
#include "Vertex.hpp"

#include <format>

// Compliation unit global variables
namespace g {
    static IDirect3DVertexBuffer9* companion_window_vertex_buf_menu;
    static IDirect3DVertexBuffer9* companion_window_vertex_buf_3d;
    static constexpr D3DMATRIX identity_matrix = d3d_from_m4(glm::identity<glm::mat4x4>());
    static IDirect3DVertexBuffer9* quad_vertex_buf[2];
    static IDirect3DVertexBuffer9* overlay_border_quad;
    static IDirect3DVertexBuffer9* companion_window_vertex_buf_focus;
}

void GpuTimer::init(IDirect3DDevice9* dev)
{
    if (dev->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &start_query) != D3D_OK) {
        throw std::runtime_error("VR initialization failed: CreateQuery");
    }
    if (dev->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &end_query) != D3D_OK) {
        throw std::runtime_error("VR initialization failed: CreateQuery");
    }
    if (dev->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT, &disjoint_query) != D3D_OK) {
        throw std::runtime_error("VR initialization failed: CreateQuery");
    }
    if (dev->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, &freq_query) != D3D_OK) {
        throw std::runtime_error("VR initialization failed: CreateQuery");
    }
}

void GpuTimer::release()
{
    for (auto query : { &start_query, &end_query, &disjoint_query, &freq_query }) {
        if (*query) {
            (*query)->Release();
            *query = nullptr;
        }
    }
}

void GpuTimer::begin()
{
    if (g::cfg.debug && queries_free_to_use && start_query) [[unlikely]] {
        disjoint_query->Issue(D3DISSUE_BEGIN);
        start_query->Issue(D3DISSUE_END);
    }
}

void GpuTimer::end()
{
    if (g::cfg.debug && queries_free_to_use && end_query) [[unlikely]] {
        end_query->Issue(D3DISSUE_END);
        freq_query->Issue(D3DISSUE_END);
        disjoint_query->Issue(D3DISSUE_END);
    }
}

float GpuTimer::get_elapsed()
{
    if (!disjoint_query) {
        return 0.0f;
    }

    BOOL disjoint;
    const auto hr = disjoint_query->GetData(&disjoint, sizeof(disjoint), 0);
    if (hr == S_FALSE) {
        // Queries still in flight, don't reissue them yet
        queries_free_to_use = false;
        return 0.0f;
    }

    // The queries have completed (or failed), so they can be reissued next frame.
    queries_free_to_use = true;

    if (hr != D3D_OK || disjoint) {
        return 0.0f;
    }

    uint64_t gpu_start, gpu_end, freq;
    if (start_query->GetData(&gpu_start, sizeof(gpu_start), 0) == D3D_OK
        && end_query->GetData(&gpu_end, sizeof(gpu_end), 0) == D3D_OK
        && freq_query->GetData(&freq, sizeof(freq), 0) == D3D_OK
        && freq > 0 && gpu_end > gpu_start) {
        return float(gpu_end - gpu_start) / float(freq);
    }
    return 0.0f;
}

bool VRInterface::is_using_quad_view_rendering() const
{
    return get_runtime_type() == OPENXR && g::cfg.quad_view_rendering;
}

void VRInterface::set_render_context(const std::string& name)
{
    current_render_context = &render_contexts[name];
    current_render_context_name = name;

    // Per-stage setting of multiview rendering
    if (dx::multiview_rendering_enabled() != g::vr->get_current_render_context()->multiview_rendering) {
        g::cfg.multiview = g::vr->get_current_render_context()->multiview_rendering;
        // g::d3d_vr->EnableMultiView(dx::multiview_rendering_enabled());
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

bool create_quad(IDirect3DDevice9* dev, float size, float aspect, float z, IDirect3DVertexBuffer9** dst)
{
    auto w = size;
    const auto h = w / aspect;
    constexpr auto left = 0.0f;
    constexpr auto rght = 1.0f;
    constexpr auto top = 0.0f;
    constexpr auto btm = 1.0f;
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
    if (is_using_texture_to_render(current_render_context->msaa, tgt, dx::multiview_rendering_enabled())) {
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
    if (is_using_texture_to_render(current_render_context->msaa, tgt, dx::multiview_rendering_enabled()) && current_render_context->dx_surface[tgt]) {
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
        if (!create_quad(dev, 1.25f, aspect_ratio, 2.4f, &g::quad_vertex_buf[0]))
            throw std::runtime_error("Could not create menu quad");
        if (!create_quad(dev, 0.6f, aspect_ratio, 1.0f, &g::quad_vertex_buf[1]))
            throw std::runtime_error("Could not create overlay quad");
        if (!create_quad(dev, 1.0f, 1.0f, 1.0f, &g::overlay_border_quad))
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
    IDirect3DVertexBuffer9* vbuf,
    bool depth_test = rbr::get_game_mode() == rbr::GameMode::MainMenu)
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
    if (dx::multiview_rendering_enabled()) {
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

    dev->SetRenderState(D3DRS_ZENABLE, depth_test);

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
    if (dx::multiview_rendering_enabled()) {
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
    if (dx::multiview_rendering_enabled()) {
        const auto right = render_target_counterpart(left);
        const auto& projectionr = vr->get_projection(right);
        const auto& eyeposr = vr->get_eye_pos(right);
        const auto& poser = vr->get_pose(right);
        mvpr = d3d_from_m4(projectionr * glm::translate(glm::scale(eyeposr * poser * g::flip_z_matrix * horizon_lock.value_or(glm::identity<M4>()), { size, size, 1.0f }), translation));
    }

    const D3DMATRIX mvpl = d3d_from_m4(projectionl * glm::translate(glm::scale(eyeposl * posel * g::flip_z_matrix * horizon_lock.value_or(glm::identity<M4>()), { size, size, 1.0f }), translation));
    render_texture(dev, &mvpl, &mvpr, &g::identity_matrix, &g::identity_matrix, texture, g::quad_vertex_buf[render_target_2d == GameMenu ? 0 : 1]);
}

void render_2d_fullscreen(IDirect3DDevice9* dev, IDirect3DTexture9* texture)
{
    render_texture(dev, &g::identity_matrix, &g::identity_matrix, &g::identity_matrix, &g::identity_matrix, texture, g::companion_window_vertex_buf_menu, false);
}

// Maps a point of the focus view in normalized device coordinates to the
// companion window normalized device coordinates through the peripheral view.
static glm::vec2 focus_point_to_companion_window(VRInterface* vr, RenderTarget peripheral, RenderTarget focus, glm::vec2 focus_ndc)
{
    const auto& focus_proj = vr->get_projection(focus);
    const auto& peripheral_proj = vr->get_projection(peripheral);

    // Ray through the NDC point in the focus view space (right handed, looking towards -z)
    const glm::vec3 focus_dir = {
        (focus_ndc.x + focus_proj[2][0]) / focus_proj[0][0],
        (focus_ndc.y + focus_proj[2][1]) / focus_proj[1][1],
        -1.0f,
    };

    const auto focus_view = glm::mat3(vr->get_eye_pos(focus) * vr->get_pose(focus));
    const auto peripheral_view = glm::mat3(vr->get_eye_pos(peripheral) * vr->get_pose(peripheral));
    const auto dir = peripheral_view * glm::inverse(focus_view) * focus_dir;

    const auto x = (peripheral_proj[0][0] * dir.x + peripheral_proj[2][0] * dir.z) / -dir.z;
    const auto y = (peripheral_proj[1][1] * dir.y + peripheral_proj[2][1] * dir.z) / -dir.z;
    const auto u = (x + 1.0f) / 2.0f;
    const auto v = (1.0f - y) / 2.0f;

    // Apply the same crop that companion_window_vertex_buf_3d does to the peripheral view
    const auto size = g::cfg.companion_size / 100.0f;
    const auto offset_x = g::cfg.companion_offset.x / 100.0f;
    const auto offset_y = g::cfg.companion_offset.y / 100.0f;
    const auto aspect = static_cast<float>(vr->companion_window_aspect_ratio);

    return {
        (u - offset_x) / size * 2.0f - 1.0f,
        1.0f - (v - offset_y) / (size / aspect) * 2.0f,
    };
}

static void render_companion_window_focus_view(IDirect3DDevice9* dev, VRInterface* vr, RenderTarget peripheral)
{
    const auto focus = static_cast<RenderTarget>(peripheral + 2);

    const auto tl = focus_point_to_companion_window(vr, peripheral, focus, { -1.0f, 1.0f });
    const auto tr = focus_point_to_companion_window(vr, peripheral, focus, { 1.0f, 1.0f });
    const auto bl = focus_point_to_companion_window(vr, peripheral, focus, { -1.0f, -1.0f });
    const auto br = focus_point_to_companion_window(vr, peripheral, focus, { 1.0f, -1.0f });

    // clang-format off
    Vertex quad[] = {
        { tl.x, tl.y, 1, 0, 0, }, // Top-left
        { tr.x, tr.y, 1, 1, 0, }, // Top-right
        { bl.x, bl.y, 1, 0, 1 }, // Bottom-left
        { br.x, br.y, 1, 1, 1 } // Bottom-right
    };
    // clang-format on

    if (!g::companion_window_vertex_buf_focus) {
        if (!create_vertex_buffer(dev, quad, 4, &g::companion_window_vertex_buf_focus)) {
            dbg("Could not create vertex buffer for companion window focus view");
            return;
        }
    } else {
        void* bufmem;
        if (FAILED(g::companion_window_vertex_buf_focus->Lock(0, sizeof(quad), &bufmem, 0))) {
            return;
        }
        memcpy(bufmem, quad, sizeof(quad));
        g::companion_window_vertex_buf_focus->Unlock();
    }

    render_texture(dev, &g::identity_matrix, &g::identity_matrix, &g::identity_matrix, &g::identity_matrix, vr->get_texture(focus), g::companion_window_vertex_buf_focus, false);
}

void render_companion_window_from_render_target(IDirect3DDevice9* dev, VRInterface* vr, RenderTarget tgt)
{
    // The dummy runtime renders the eye views in the companion window resolution, so they are shown as they are.
    const auto show_whole_texture = tgt == GameMenu || tgt == Overlay || vr->get_runtime_type() == DUMMY;
    render_texture(dev, &g::identity_matrix, &g::identity_matrix, &g::identity_matrix, &g::identity_matrix, vr->get_texture(tgt), show_whole_texture ? g::companion_window_vertex_buf_menu : g::companion_window_vertex_buf_3d);

    if ((tgt == LeftEye || tgt == RightEye) && vr->is_using_quad_view_rendering()) {
        render_companion_window_focus_view(dev, vr, tgt);
    }
}
