#pragma once

#include "Config.hpp"
#include "VR.hpp"

class DummyVR : public VRInterface {
private:
    constexpr M4 get_projection_matrix(RenderTarget eye, float z_near, float z_far, bool reverse_z);

public:
    DummyVR() {}
    virtual ~DummyVR()
    {
    }

    void init(IDirect3DDevice9* dev, IDirect3DVR9** vrdev, uint32_t companionWindowWidth, uint32_t companionWindowHeight)
    {
    }

    void prepare_frames_for_hmd(IDirect3DDevice9* dev) override
    {
    }

    bool update_vr_poses() override
    {
    }

    void submit_frames_to_hmd(IDirect3DDevice9* dev) override
    {
    }

    FrameTimingInfo get_frame_timing() override
    {
        FrameTimingInfo dummy = {0};
        return dummy;
    }

    void reset_view() override { }
    VRRuntime get_runtime_type() const override { return DUMMY; }
    void set_render_context(const std::string& name) override { }
};
