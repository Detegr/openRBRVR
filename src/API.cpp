#include "API.hpp"
#include "Config.hpp"
#include "Dx.hpp"
#include "Globals.hpp"
#include "IPlugin.h"
#include "OpenXR.hpp"
#include "Util.hpp"
#include "VR.hpp"
#include "openRBRVR.hpp"

#include <MinHook.h>
#include <cstring>

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
        return 0;
    }

    if (ops == GET_VIEW_INFO) {
        // Called every rendered view by plugins, no debug print
        auto* info = reinterpret_cast<OpenRBRVRViewInfo*>(static_cast<uintptr_t>(value));
        if (!info || info->struct_size < sizeof(OpenRBRVRViewInfo) || !g::vr || !g::vr_render_target) {
            return -1;
        }

        const auto target = g::vr_render_target.value();
        if (target > FocusRight) {
            // The menu and overlay targets are not 3D views
            return -1;
        }

        const auto multiview = dx::multiview_rendering_enabled() && (target == LeftEye || target == FocusLeft);
        const auto [width, height] = g::vr->get_render_resolution(target);
        info->view_count = multiview ? 2 : 1;
        info->focus_view = (target == FocusLeft || target == FocusRight) ? 1 : 0;
        info->width = width;
        info->height = height;

        const auto first = d3d_from_m4(g::vr->get_projection(target));
        const auto second = multiview ? d3d_from_m4(g::vr->get_projection(render_target_counterpart(target))) : first;
        static_assert(sizeof(D3DMATRIX) == sizeof(float) * 16);
        std::memcpy(info->projection[0], &first, sizeof(first));
        std::memcpy(info->projection[1], &second, sizeof(second));
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
