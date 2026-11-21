#include "API.hpp"
#include "Config.hpp"
#include "Dx.hpp"
#include "Globals.hpp"
#include "IPlugin.h"
#include "OpenXR.hpp"
#include "RBR.hpp"
#include "VR.hpp"
#include "openRBRVR.hpp"

#include <MinHook.h>

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    MH_Initialize();
    return TRUE;
}

extern "C" __declspec(dllexport) IPlugin* RBR_CreatePlugin(IRBRGame* game)
{
    if (!g::openrbrvr) {
        g::openrbrvr = new openRBRVR(game);
    }
    return g::openrbrvr;
}

extern "C" __declspec(dllexport) int64_t openRBRVR_Exec(ApiOperation ops, uint64_t value)
{
    if (ops == SET_REVERSE_Z) {
        g::reverse_z = value != 0;
    }

    if (ops & GET_VIEW_INFO) {
        // Value must be a pointer to an array of ViewInfo pointers
        // with at least 2 elements
        auto req = reinterpret_cast<ViewInfo**>(value);
        if (!req) [[unlikely]] {
            return -1;
        }

        if (!g::vr || !g::vr->get_current_render_context()) [[unlikely]] {
            return -2;
        }

        if (!g::vr_render_target || rbr::is_rendering_cube_map()) [[unlikely]] {
            return -3;
        }

        const auto target = g::vr_render_target.value();
        const auto multiview = dx::multiview_rendering_enabled();

        const auto fill = [](ViewInfo* info, RenderTarget tgt) {
            const auto [width, height] = g::vr->get_render_resolution(tgt);
            info->target = static_cast<uint32_t>(tgt);
            info->width = width;
            info->height = height;

            const auto& pose = g::vr->get_pose(tgt);
            const auto& eye_pos = g::vr->get_eye_pos(tgt);
            const auto horizon_lock = rbr::get_horizon_lock_matrix();
            const auto view = eye_pos * pose * g::flip_z_matrix * horizon_lock;

            const auto d3d_proj = d3d_from_m4(g::vr->get_projection(tgt));
            const auto d3d_pose = d3d_from_m4(pose);
            const auto d3d_eye_pos = d3d_from_m4(eye_pos);
            const auto d3d_horizon_lock = d3d_from_m4(horizon_lock);
            const auto d3d_view = d3d_from_m4(view);

            memcpy(info->projection, &d3d_proj, sizeof(D3DMATRIX));
            memcpy(info->pose, &d3d_pose, sizeof(D3DMATRIX));
            memcpy(info->eye_pos, &d3d_eye_pos, sizeof(D3DMATRIX));
            memcpy(info->horizon_lock, &d3d_horizon_lock, sizeof(D3DMATRIX));
            memcpy(info->view, &d3d_view, sizeof(D3DMATRIX));
        };

        fill(req[0], target);
        if (multiview) {
            fill(req[1], render_target_counterpart(target));
        }

        return multiview ? dx::multiview_view_count : 1;
    }

    dbg(std::format("Exec: {} {}", (uint64_t)ops, (uint64_t)value));

    if (ops == API_VERSION) {
        return 8;
    }
    if (ops & NOTIFY_VERTEX_SHADER) {
        auto shader = reinterpret_cast<IDirect3DVertexShader9*>(value);
        if (!dx::add_vertex_shader(shader)) {
            return -1;
        }
    }
    if (ops & MOVE_SEAT) {
        if (value > static_cast<uint64_t>(MOVE_SEAT_DOWN)) {
            return 1;
        }
        g::seat_movement_request = static_cast<SeatMovement>(value);
    }
    if (ops & RECENTER_VR_VIEW) {
        if (!g::vr) {
            return -1;
        }
        g::vr->reset_view();
    }
    if (ops & TOGGLE_DEBUG_INFO) {
        if (g::cfg.debug) {
            if (g::cfg.debug_mode == 1) {
                g::cfg.debug_mode = 0;
            } else {
                g::cfg.debug = false;
            }
        } else {
            g::cfg.debug = true;
            g::cfg.debug_mode = 1;
        }
        g::draw_overlay_border = (g::cfg.debug && g::cfg.debug_mode == 0);
    }
    if (ops & GET_VR_RUNTIME) {
        if (!g::vr) {
            return 0;
        }
        return static_cast<int64_t>(g::vr->get_runtime_type());
    }
    return 0;
}
