#pragma once

#include "RenderTarget.hpp"
#include "Util.hpp"

#include <d3d9_vr.h>

#include <d3d9.h>
#include <openvr.h>
#include <openxr.h>
#include <optional>
#include <unordered_map>

// We pass multiview view/projection matrices in D3DTS_WORLDMATRIX indices in
// the openRBRVR modified DXVK version, see dxvk-openRBRVR d3d9_device.cpp
constexpr auto D3DTS_VIEW_LEFT = D3DTS_VIEW;
constexpr auto D3DTS_VIEW_RIGHT = D3DTS_WORLDMATRIX(10);
constexpr auto D3DTS_PROJECTION_LEFT = D3DTS_PROJECTION;
constexpr auto D3DTS_PROJECTION_RIGHT = D3DTS_WORLDMATRIX(11);

enum HorizonLock : uint8_t {
    LOCK_NONE = 0x0,        //0000 0000
    LOCK_ROLL = 0x1,        //0000 0001
    LOCK_PITCH = 0x2,       //0000 0010
    LOWPASS_NONE = 0x4,     //0000 0100
    LOWPASS_ROLL = 0x8,     //0000 1000
    LOWPASS_PITCH = 0x10,   //0001 0000 
};

enum VRRuntime {
    OPENVR = 1,
    OPENXR = 2,
};

enum class Projection {
    Stage,
    Cockpit,
    MainMenu,
};

enum class CompanionMode {
    Off,
    VREye,
    Static,
};

inline const std::string companion_mode_str(CompanionMode m)
{
    switch (m) {
        case CompanionMode::Off: return "off";
        case CompanionMode::VREye: return "vreye";
        case CompanionMode::Static: return "static";
    }
    std::unreachable();
}

inline const std::string companion_mode_str_pretty(CompanionMode m)
{
    switch (m) {
        case CompanionMode::Off: return "Off";
        case CompanionMode::VREye: return "VR view";
        case CompanionMode::Static: return "Bonnet camera";
    }
    std::unreachable();
}

inline CompanionMode companion_mode_from_str(const std::string& s)
{
    if (s == "off")
        return CompanionMode::Off;
    if (s == "vreye")
        return CompanionMode::VREye;
    if (s == "static")
        return CompanionMode::Static;
    return CompanionMode::VREye;
}

struct FrameTimingInfo {
    float gpu_pre_submit;
    float gpu_post_submit;
    float compositor_gpu;
    float compositor_cpu;
    float compositor_submit_frame;
    float gpu_total;
    float frame_interval;
    float wait_for_present;
    float cpu_present_call;
    float cpu_wait_for_present;
    uint32_t reprojection_flags;
    uint32_t mispresented_frames;
    uint32_t dropped_frames;
};

struct RenderContext {
    // One for each possible view (2 for stereo, 4 for quad views)
    uint32_t width[4];
    uint32_t height[4];
    HANDLE dx_shared_handle[4] = { 0 };

    // One for each render target
    IDirect3DTexture9* dx_texture[6] = { 0 };
    IDirect3DSurface9* dx_surface[6] = { 0 };
    IDirect3DSurface9* dx_depth_stencil_surface[6] = { 0 };

    IDirect3DTexture9* overlay_border;
    D3DMULTISAMPLE_TYPE msaa;

    bool quad_view_rendering;
    bool multiview_rendering;

    void* ext;
};

class VRInterface {
protected:
    std::unordered_map<std::string, RenderContext> render_contexts;
    RenderContext* current_render_context;

    M4 hmd_pose[4];
    M4 eye_pos[4];
    M4 projection[4];

    void init_surfaces(IDirect3DDevice9* dev, RenderContext& ctx, uint32_t res_x_2d, uint32_t res_y_2d);

    static constexpr float z_near = 0.01f;
    static constexpr float z_far = 10000.0f;

public:
    virtual ~VRInterface()
    {
    }

    double companion_window_width;
    double companion_window_height;
    double companion_window_aspect_ratio;

    virtual void shutdown_vr() = 0;
    virtual bool update_vr_poses() = 0;
    virtual IDirect3DSurface9* prepare_vr_rendering(IDirect3DDevice9* dev, RenderTarget tgt, bool clear = true);
    virtual void finish_vr_rendering(IDirect3DDevice9* dev, RenderTarget tgt);
    virtual void prepare_frames_for_hmd(IDirect3DDevice9* dev) = 0;
    virtual void submit_frames_to_hmd(IDirect3DDevice9* dev) = 0;
    bool is_using_quad_view_rendering() const;
    std::tuple<uint32_t, uint32_t> get_render_resolution(RenderTarget tgt) const
    {
        return std::make_tuple(current_render_context->width[tgt], current_render_context->height[tgt]);
    }
    virtual FrameTimingInfo get_frame_timing() = 0;

    const M4& get_projection(RenderTarget tgt) const
    {
        return projection[tgt];
    }
    const M4& get_eye_pos(RenderTarget tgt) const { return eye_pos[tgt]; }
    const M4& get_pose(RenderTarget tgt) const { return hmd_pose[tgt]; }
    IDirect3DTexture9* get_texture(RenderTarget tgt) const { return current_render_context->dx_texture[tgt]; }
    RenderContext* get_current_render_context() const { return current_render_context; }
    bool create_companion_window_buffer(IDirect3DDevice9* dev);

    virtual void reset_view() = 0;
    virtual VRRuntime get_runtime_type() const = 0;
    virtual void set_render_context(const std::string& name);
};

bool create_quad(IDirect3DDevice9* dev, float size, float aspect, IDirect3DVertexBuffer9** dst);
void render_overlay_border(IDirect3DDevice9* dev, IDirect3DTexture9* tex);
void render_menu_quad(IDirect3DDevice9* dev, VRInterface* vr, IDirect3DTexture9* texture, RenderTarget renderTarget3D, RenderTarget render_target_2d, float size, glm::vec3 translation, const std::optional<M4>& horizon_lock);
void render_companion_window_from_render_target(IDirect3DDevice9* dev, VRInterface* vr, RenderTarget tgt);
