#include "API.hpp"
#include "Config.hpp"
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
    dbg(std::format("Exec: {} {}", (uint64_t)ops, (uint64_t)value));

    if (ops == API_VERSION) {
        return 3;
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
        // TODO: Update debug mode
        g::cfg.debug = !g::cfg.debug;
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
