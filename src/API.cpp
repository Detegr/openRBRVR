#include "API.hpp"
#include "Config.hpp"
#include "Dx.hpp"
#include "Globals.hpp"
#include "IPlugin.h"
#include "OpenXR.hpp"
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
        auto info = reinterpret_cast<ViewInfo*>(value);
        if (!info || info->struct_size < sizeof(ViewInfo) || !g::vr || !g::vr->get_current_render_context()) [[unlikely]] {
            return -1;
        }

        auto tgt = static_cast<RenderTarget>(info->requested_target);
        if (tgt > FocusRight) [[unlikely]] {
            return -2;
        }

        if (!g::vr->is_using_quad_view_rendering() && tgt > RightEye) {
            return -3;
        }

        const auto [width, height] = g::vr->get_render_resolution(tgt);
        info->width = width;
        info->height = height;

        const auto proj = d3d_from_m4(g::vr->get_projection(tgt));
        const auto pose = d3d_from_m4(g::vr->get_pose(tgt));
        const auto eye_pos = d3d_from_m4(g::vr->get_eye_pos(tgt));

        memcpy(info->projection, &proj, sizeof(D3DMATRIX));
        memcpy(info->pose, &pose, sizeof(D3DMATRIX));
        memcpy(info->eye_pos, &eye_pos, sizeof(D3DMATRIX));

        return 0;
    }

    dbg(std::format("Exec: {} {}", (uint64_t)ops, (uint64_t)value));

    if (ops == API_VERSION) {
        return 6;
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
