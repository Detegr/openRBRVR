#pragma once

#include "IPlugin.h"
#include "VR.hpp"
#include "openRBRVR.hpp"

extern Config gCfg;
static openRBRVR* gPlugin;

extern "C" __declspec(dllexport) IPlugin* RBR_CreatePlugin(IRBRGame* game)
{
    if (!gPlugin) {
        gPlugin = new openRBRVR(game);
    }
    return gPlugin;
}

enum ApiOperations : uint64_t {
    API_VERSION = 0,
    RECENTER_VR_VIEW = 0b1,
    TOGGLE_DEBUG_INFO = 0b10,
};

extern "C" __declspec(dllexport) int64_t openRBRVR_Exec(ApiOperations ops, uint64_t value)
{
    if (ops == API_VERSION) {
        return 1;
    }
    if (ops & RECENTER_VR_VIEW) {
        auto chaperone = vr::VRChaperone();
        if (chaperone) {
            chaperone->ResetZeroPose(vr::ETrackingUniverseOrigin::TrackingUniverseSeated);
        }
    }
    if (ops & TOGGLE_DEBUG_INFO) {
        gCfg.debug = !gCfg.debug;
    }
    return 0;
}
