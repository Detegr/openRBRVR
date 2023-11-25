#pragma once

#include "IPlugin.h"
#include "VR.hpp"
#include "openRBRVR.hpp"

static openRBRVR* gPlugin;

extern "C" __declspec(dllexport) IPlugin* RBR_CreatePlugin(IRBRGame* game)
{
    if (!gPlugin) {
        gPlugin = new openRBRVR(game);
    }
    return gPlugin;
}

enum ApiOperations : uint64_t {
    RESET_VR_VIEW = 1,
};

extern "C" __declspec(dllexport) int32_t openRBRVR_Exec(ApiOperations ops)
{
    if (ops & RESET_VR_VIEW) {
        vr::VRChaperone()->ResetZeroPose(vr::ETrackingUniverseOrigin::TrackingUniverseSeated);
    }
    return 0;
}