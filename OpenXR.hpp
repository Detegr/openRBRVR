#pragma once

#include "VR.hpp"
#include <array>
#define XR_USE_GRAPHICS_API_VULKAN
#include "openxr/openxr_platform.h"
#include "openxr/openxr_reflection.h"

class OpenXR : public VRInterface {
private:
    XrSession session;
    XrInstance instance;
    XrSystemId systemId;
    XrSwapchain swapchains[2];
    XrSpace space;
    std::vector<XrSwapchainImageVulkanKHR> swapchainImages[2];
    std::array<XrView, 2> views;
    std::array<XrCompositionLayerProjectionView, 2> projectionViews;
    bool hasProjection; // TODO
    int64_t swapchainFormat;

    std::vector<char> deviceExtensions;
    std::vector<char> instanceExtensions;

    XrSwapchainImageVulkanKHR& AcquireSwapchainImage(RenderTarget tgt);
    std::optional<XrViewState> UpdateViews(const XrFrameState& frameState);
    void UpdatePoses(const XrFrameState& frameState);
    bool GetProjectionMatrix(const XrFrameState& frameState);

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
    void SubmitFramesToHMD(IDirect3DDevice9* dev) override;
    std::tuple<uint32_t, uint32_t> GetRenderResolution(RenderTarget tgt) const override { return std::make_tuple(0, 0); }
    FrameTimingInfo GetFrameTiming() const override
    {
        return FrameTimingInfo {};
    }
    void ResetView() const override
    {
    }
    VRRuntime GetRuntimeType() const override { return OPENXR; }

    const char* GetDeviceExtensions();
    const char* GetInstanceExtensions();
};
