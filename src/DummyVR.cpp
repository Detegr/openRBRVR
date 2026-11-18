#include "DummyVR.hpp"
#include "Globals.hpp"
#include "RBR.hpp"
#include "Util.hpp"

void DummyVR::init(IDirect3DDevice9* dev, IDirect3DExtensions9** vrdev, uint32_t companionWindowWidth, uint32_t companionWindowHeight)
{
    for (const auto& gfx : g::cfg.gfx) {
        RenderContext ctx = {
            .width = { companionWindowWidth, companionWindowWidth },
            .height = { companionWindowHeight, companionWindowHeight },
            .msaa = gfx.second.msaa.value_or(g::cfg.gfx["default"].msaa.value()),
            .quad_view_rendering = false,
            .multiview_rendering = false,
        };
        init_surfaces(dev, ctx, companionWindowWidth, companionWindowHeight);
        render_contexts[gfx.first] = ctx;
    }

    set_render_context("default");

    eye_pos[LeftEye] = glm::identity<glm::mat4x4>();
    eye_pos[RightEye] = glm::identity<glm::mat4x4>();
    hmd_pose[LeftEye] = glm::identity<glm::mat4x4>();
    hmd_pose[RightEye] = glm::identity<glm::mat4x4>();
}

static M4 create_default_projection_matrix(float aspect, float z_near, float z_far, bool reverse_z)
{
    // Fallback projection for 2D content, like menus
    auto ret = glm::perspectiveRH_ZO(glm::radians(60.0f), aspect, z_near, z_far);

    if (reverse_z) {
        ret[2][2] = z_near / (z_near - z_far);
        ret[2][3] = -1.0f;
        ret[3][2] = z_near;
    }

    return ret;
}

bool DummyVR::update_vr_poses()
{
    const auto aspect = static_cast<float>(companion_window_width / companion_window_height);
    projection[LeftEye] = create_default_projection_matrix(aspect, z_near, z_far, g::reverse_z);
    projection[RightEye] = projection[LeftEye];
    return true;
}

void DummyVR::projection_changed(const M4& proj)
{
    projection[LeftEye] = proj * g::flip_z_matrix;
    projection[RightEye] = projection[LeftEye];
}

void DummyVR::prepare_frames_for_hmd(IDirect3DDevice9* dev)
{
    if (current_render_context->msaa != D3DMULTISAMPLE_NONE) {
        IDirect3DSurface9 *left_eye, *right_eye;

        if (current_render_context->dx_texture[LeftEye]->GetSurfaceLevel(0, &left_eye) != D3D_OK) {
            return;
        }
        if (current_render_context->dx_texture[RightEye]->GetSurfaceLevel(0, &right_eye) != D3D_OK) {
            left_eye->Release();
            return;
        }

        dev->StretchRect(current_render_context->dx_surface[LeftEye], nullptr, left_eye, nullptr, D3DTEXF_NONE);
        dev->StretchRect(current_render_context->dx_surface[RightEye], nullptr, right_eye, nullptr, D3DTEXF_NONE);

        left_eye->Release();
        right_eye->Release();
    }
}
