#pragma once

#include "Config.hpp"
#include "VR.hpp"
#include <array>
#include <d3d9.h>
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr_platform.h>
#include <openxr_reflection.h>
#include <optional>

struct OpenXRRenderContext {
    XrSwapchain swapchains[2];
    std::vector<XrSwapchainImageVulkanKHR> swapchainImages[2];
    std::array<XrView, 2> views;
    std::array<XrCompositionLayerProjectionView, 2> projectionViews;
};

class OpenXR : public VRInterface {
private:
    XrSession session;
    XrInstance instance;
    XrSystemId systemId;
    XrSpace space;
    XrSpace viewSpace;
    XrFrameState frameState;
    int64_t swapchainFormat;
    XrPosef viewPose;
    bool hasProjection;
    bool resetViewRequested;

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
    OpenXRRenderContext* XrContext()
    {
        return reinterpret_cast<OpenXRRenderContext*>(currentRenderContext->ext);
    }

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
    bool UpdateVRPoses() override;
    void PrepareFramesForHMD(IDirect3DDevice9* dev) override;
    void SubmitFramesToHMD(IDirect3DDevice9* dev) override;
    void ResetView() override;
    FrameTimingInfo GetFrameTiming() override;
    VRRuntime GetRuntimeType() const override { return OPENXR; }

    const char* GetDeviceExtensions();
    const char* GetInstanceExtensions();
};
