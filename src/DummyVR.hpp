#pragma once

#include "VR.hpp"

class DummyVR : public VRInterface {
private:
    GpuTimer gpu_timer;

public:
    DummyVR() { }
    virtual ~DummyVR()
    {
        shutdown_vr();
    }

    void init(IDirect3DDevice9* dev, IDirect3DExtensions9** vrdev, uint32_t companionWindowWidth, uint32_t companionWindowHeight);
    void shutdown_vr() override
    {
        gpu_timer.release();
    }

    void prepare_frames_for_hmd(IDirect3DDevice9* dev) override;

    bool update_vr_poses() override;
    void projection_changed(const M4& proj) override;

    void submit_frames_to_hmd(IDirect3DDevice9* dev) override
    {
        gpu_timer.end();
    }

    FrameTimingInfo get_frame_timing() override
    {
        FrameTimingInfo ret = { 0 };
        ret.gpu_total = gpu_timer.get_elapsed();
        return ret;
    }

    void reset_view() override { }
    VRRuntime get_runtime_type() const override { return DUMMY; }
};
