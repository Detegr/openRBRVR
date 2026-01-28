#include "DummyVR.hpp"
#include "Dx.hpp"
#include "Globals.hpp"
#include "RBR.hpp"
#include "Util.hpp"

void DummyVR::init(IDirect3DDevice9* dev, IDirect3DVR9** vrdev, uint32_t companionWindowWidth, uint32_t companionWindowHeight)
{
    for (const auto& gfx : g::cfg.gfx) {
        RenderContext ctx = {
            .width = { companionWindowWidth, companionWindowWidth },
            .height = { companionWindowHeight, companionWindowHeight },
            .msaa = gfx.second.msaa.value_or(g::cfg.gfx["default"].msaa.value()),
            .quad_view_rendering = false, 
            .multiview_rendering = true, //true, // Set to true so we only render once
        };
        init_surfaces(dev, ctx, companionWindowWidth, companionWindowHeight);
        render_contexts[gfx.first] = ctx;
    }

    set_render_context("default");
    g::cfg.multiview = true;

    eye_pos[LeftEye] = glm::identity<glm::mat4x4>();
    hmd_pose[LeftEye] = glm::identity<glm::mat4x4>();
    eye_pos[RightEye] = glm::identity<glm::mat4x4>();
    hmd_pose[RightEye] = glm::identity<glm::mat4x4>();
}

bool DummyVR::update_vr_poses()
{
    projection[LeftEye] = get_projection_matrix(LeftEye, rbr::get_z_near(), z_far, rbr::should_use_reverse_z_buffer());
    projection[RightEye] = get_projection_matrix(LeftEye, rbr::get_z_near(), z_far, rbr::should_use_reverse_z_buffer());
    return true;
}

constexpr M4 DummyVR::get_projection_matrix(RenderTarget eye, float z_near, float z_far, bool reverse_z)
{
    const auto w = static_cast<float>(companion_window_width);
    const auto h = static_cast<float>(companion_window_height);

    auto ret = glm::perspectiveFovLH_ZO(rbr::get_fov(), w, h, 0.001f, z_far);

    if (reverse_z) [[likely]] {
        ret[2][2] = z_near / (z_near - z_far);
        ret[2][3] = -1.0f;
        ret[3][2] = z_near;
    }

    return ret;
}

void DummyVR::prepare_frames_for_hmd(IDirect3DDevice9* dev)
{
    const auto msaa = current_render_context->msaa != D3DMULTISAMPLE_NONE;
    if (dx::multiview_rendering_enabled() || msaa) {
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
        if (dx::multiview_rendering_enabled()) {
            IDirect3DSurface9* eyes[2] = { left_eye, right_eye };
            g::d3d_vr->CopySurfaceLayers(current_render_context->dx_surface[LeftEye], eyes, 2);
        } else {
            // Resolve multisampling
            dev->StretchRect(current_render_context->dx_surface[LeftEye], nullptr, left_eye, nullptr, D3DTEXF_NONE);
            dev->StretchRect(current_render_context->dx_surface[RightEye], nullptr, right_eye, nullptr, D3DTEXF_NONE);
        }
    }
}
