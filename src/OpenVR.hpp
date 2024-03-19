#pragma once

#include "Config.hpp"
#include "VR.hpp"

class OpenVR : public VRInterface {
private:
    vr::IVRSystem* hmd;
    vr::IVRCompositor* compositor;
    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    vr::Texture_t openvr_texture[2];
    D3D9_TEXTURE_VR_DESC dxvk_texture[2];

    constexpr M4 get_projection_matrix(RenderTarget eye, float zNear, float zFar);

public:
    OpenVR();
    virtual ~OpenVR()
    {
        shutdown_vr();
    }

    void init(IDirect3DDevice9* dev, const Config& cfg, IDirect3DVR9** vrdev, uint32_t companionWindowWidth, uint32_t companionWindowHeight);

    void shutdown_vr() override
    {
        vr::VR_Shutdown();
        hmd = nullptr;
        compositor = nullptr;
    }
    void prepare_frames_for_hmd(IDirect3DDevice9* dev) override;
    bool update_vr_poses() override;
    void submit_frames_to_hmd(IDirect3DDevice9* dev) override;
    FrameTimingInfo get_frame_timing() override;
    void reset_view() override
    {
        vr::VRChaperone()->ResetZeroPose(vr::ETrackingUniverseOrigin::TrackingUniverseSeated);
    }
    VRRuntime get_runtime_type() const override
    {
        return OPENVR;
    }
    virtual void set_render_context(const std::string& name) override;
};
