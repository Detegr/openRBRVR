#pragma once

#include "VR.hpp"
#include <array>
#include <d3d9.h>
#define XR_USE_GRAPHICS_API_VULKAN
#include "openxr/openxr_platform.h"
#include "openxr/openxr_reflection.h"

class OpenXR : public VRInterface {
private:
    XrSession session;
    XrInstance instance;
    XrSystemId systemId;
    XrSwapchain swapchains[2];
    XrSpace space, viewSpace;
    XrFrameState frameState;
    std::vector<XrSwapchainImageVulkanKHR> swapchainImages[2];
    std::array<XrView, 2> views;
    std::array<XrCompositionLayerProjectionView, 2> projectionViews;
    int64_t swapchainFormat;
    XrPosef viewPose;
    bool hasProjection;
    bool resetViewRequested;

    uint32_t renderWidth[2];
    uint32_t renderHeight[2];

    std::vector<char> deviceExtensions;
    std::vector<char> instanceExtensions;

    bool perfQueryFree = true;
    IDirect3DQuery9* gpuStartQuery;
    IDirect3DQuery9* gpuEndQuery;
    IDirect3DQuery9* gpuDisjointQuery;
    IDirect3DQuery9* gpuFreqQuery;

    XrSwapchainImageVulkanKHR& AcquireSwapchainImage(RenderTarget tgt);
    std::optional<XrViewState> UpdateViews();
    void UpdatePoses();
    bool GetProjectionMatrix();
    void RecenterView();

public:
    OpenXR();
    OpenXR(const OpenXR&) = delete;
    OpenXR(const OpenXR&&) = delete;
    OpenXR& operator=(const OpenXR&) = delete;
    OpenXR& operator=(const OpenXR&&) = delete;

    void Init(IDirect3DDevice9* dev, const Config& cfg, IDirect3DVR9** vrdev, uint32_t companionWindowWidth, uint32_t companionWindowHeight);
    virtual ~OpenXR()
    {
        ShutdownVR();
    }

    void ShutdownVR() override;
    bool UpdateVRPoses(Quaternion* carQuat, Config::HorizonLock lockSetting) override;
    void PrepareFramesForHMD(IDirect3DDevice9* dev) override;
    void SubmitFramesToHMD(IDirect3DDevice9* dev) override;
    std::tuple<uint32_t, uint32_t> GetRenderResolution(RenderTarget tgt) const override
    {
        return std::make_tuple(renderWidth[0], renderHeight[0]);
    }
    void ResetView() override;
    FrameTimingInfo GetFrameTiming() override;
    VRRuntime GetRuntimeType() const override { return OPENXR; }

    const char* GetDeviceExtensions();
    const char* GetInstanceExtensions();
};
