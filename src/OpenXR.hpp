#pragma once

#include <d3d11.h>
#include <d3d11_4.h>

#define XR_USE_GRAPHICS_API_D3D11
#define XR_USE_PLATFORM_WIN32
#include <openxr_platform.h>
#include <openxr_reflection.h>
#include <optional>

#include "Config.hpp"
#include "VR.hpp"
#include <array>
#include <d3d9.h>

struct OpenXRRenderContext {
    std::vector<XrSwapchain> swapchains;
    std::vector<ID3D11Texture2D*> shared_textures;
    std::vector<std::vector<XrSwapchainImageD3D11KHR>> swapchain_images;
    std::vector<XrView> views;
    std::vector<XrCompositionLayerProjectionView> projection_views;

    OpenXRRenderContext(size_t view_count)
        : swapchains(view_count)
        , shared_textures(view_count)
        , swapchain_images(view_count)
        , views(view_count)
        , projection_views(view_count)
    {
    }
};

namespace Side {
    const int LEFT = 0;
    const int RIGHT = 1;
    const int COUNT = 2;
}

struct InputState {
    XrActionSet action_set { XR_NULL_HANDLE };
    XrAction pose_action { XR_NULL_HANDLE };
    std::array<XrPath, Side::COUNT> hand_subaction_path;
    std::array<XrSpace, Side::COUNT> hand_space;
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
    XrViewConfigurationType primary_view_config_type;
    InputState input_state; // For sending poses to OpenXR-MotionCompensation https://github.com/BuzzteeBear/OpenXR-MotionCompensation
    bool reset_view_requested;
    bool has_views;

    PFN_xrConvertWin32PerformanceCounterToTimeKHR xr_convert_win32_performance_counter_to_time;

    struct {
        uint64_t value;
        ID3D11Fence* fence;
        HANDLE shared_handle;
    } cross_api_fence;

    std::vector<char> device_extensions;
    std::vector<char> instance_extensions;

    bool perf_query_free_to_use = true;
    IDirect3DQuery9* gpu_start_query;
    IDirect3DQuery9* gpu_end_query;
    IDirect3DQuery9* gpu_disjoint_query;
    IDirect3DQuery9* gpu_freq_query;

    XrSwapchainImageD3D11KHR& acquire_swapchain_image(RenderTarget tgt);
    std::optional<XrViewState> update_views();
    void update_poses();
    bool get_projection_matrix();
    void recenter_view();
    void synchronize_graphics_apis(bool wait_for_cpu = false);
    OpenXRRenderContext* xr_context()
    {
        return reinterpret_cast<OpenXRRenderContext*>(current_render_context->ext);
    }

    std::optional<std::string> init_motion_compensation_support();
    bool set_interaction_profile_bindings();
    std::vector<XrInteractionProfileSuggestedBinding> get_supported_interaction_profiles(const std::array<XrActionSuggestedBinding, 2>& bindings);
    void update_hand_poses();

public:
    OpenXR();
    OpenXR(const OpenXR&) = delete;
    OpenXR(const OpenXR&&) = delete;
    OpenXR& operator=(const OpenXR&) = delete;
    OpenXR& operator=(const OpenXR&&) = delete;

    bool native_quad_views;

    void init(IDirect3DDevice9* dev, IDirect3DVR9** vrdev, uint32_t companionWindowWidth, uint32_t companionWindowHeight, std::optional<XrPosef> old_view_pose = std::nullopt);
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

    constexpr XrInstance get_instance() const { return instance; }
    constexpr XrSystemId get_system_id() const { return system_id; }
    XrPosef get_view_pose() const { return view_pose; }

    template <typename T>
    static T get_extension(XrInstance instance, const std::string& fnName)
    {
        T fn = nullptr;
        if (auto err = xrGetInstanceProcAddr(instance, fnName.c_str(), reinterpret_cast<PFN_xrVoidFunction*>(&fn)); err != XR_SUCCESS) {
            throw std::runtime_error(std::format("Failed to initialize OpenXR: xrGetInstanceProcAddr(\"{}\") {}", fnName, XrResultToString(instance, err)));
        }
        if (!fn) {
            throw std::runtime_error(std::format("Failed to initialize OpenXR: Could not get extension function {}", fnName));
        }
        return fn;
    }

    static std::string XrResultToString(const XrInstance& instance, XrResult res)
    {
        char buf[XR_MAX_RESULT_STRING_SIZE] = { 0 };

        if (auto err = xrResultToString(instance, res, buf); err != XR_SUCCESS) {
            dbg("Could not convert XrResult to string");
            return "";
        }

        return buf;
    }
};
