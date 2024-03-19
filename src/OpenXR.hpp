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
    std::vector<XrSwapchainImageVulkanKHR> swapchain_images[2];
    std::array<XrView, 2> views;
    std::array<XrCompositionLayerProjectionView, 2> projection_views;
};

class OpenXR : public VRInterface {
private:
    XrSession session;
    XrInstance instance;
    XrSystemId system_id;
    XrSpace space;
    XrSpace view_space;
    XrFrameState frame_state;
    int64_t swapchain_format;
    XrPosef view_pose;
    bool has_projection;
    bool reset_view_requested;

    std::vector<char> device_extensions;
    std::vector<char> instance_extensions;

    bool perf_query_free_to_use = true;
    IDirect3DQuery9* gpu_start_query;
    IDirect3DQuery9* gpu_end_query;
    IDirect3DQuery9* gpu_disjoint_query;
    IDirect3DQuery9* gpu_freq_query;

    XrSwapchainImageVulkanKHR& acquire_swapchain_image(RenderTarget tgt);
    std::optional<XrViewState> update_views();
    void update_poses();
    bool get_projection_matrix();
    void recenter_view();
    OpenXRRenderContext* xr_context()
    {
        return reinterpret_cast<OpenXRRenderContext*>(current_render_context->ext);
    }

public:
    OpenXR();
    OpenXR(const OpenXR&) = delete;
    OpenXR(const OpenXR&&) = delete;
    OpenXR& operator=(const OpenXR&) = delete;
    OpenXR& operator=(const OpenXR&&) = delete;

    void init(IDirect3DDevice9* dev, const Config& cfg, IDirect3DVR9** vrdev, uint32_t companionWindowWidth, uint32_t companionWindowHeight);
    virtual ~OpenXR()
    {
        shutdown_vr();
    }

    void shutdown_vr() override;
    bool update_vr_poses() override;
    void prepare_frames_for_hmd(IDirect3DDevice9* dev) override;
    void submit_frames_to_hmd(IDirect3DDevice9* dev) override;
    void reset_view() override;
    FrameTimingInfo get_frame_timing() override;
    VRRuntime get_runtime_type() const override { return OPENXR; }

    const char* get_device_extensions();
    const char* get_instance_extensions();
};
