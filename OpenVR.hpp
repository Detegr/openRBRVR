#pragma once

#include "VR.hpp"

class OpenVR : public VRInterface {
private:
    vr::IVRSystem* HMD;
    vr::IVRCompositor* compositor;
    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    vr::Texture_t openvrTexture[2];
    D3D9_TEXTURE_VR_DESC dxvkTexture[2];

    bool CreateVRTexture(IDirect3DDevice9* dev, RenderTarget eye, uint32_t w, uint32_t h);
    constexpr M4 GetProjectionMatrix(RenderTarget eye, float zNear, float zFar);

public:
    OpenVR(IDirect3DDevice9* dev, const Config& cfg, IDirect3DVR9** vrdev, uint32_t companionWindowWidth, uint32_t companionWindowHeight);
    virtual ~OpenVR()
    {
        ShutdownVR();
    }

    void ShutdownVR() override
    {
        vr::VR_Shutdown();
        HMD = nullptr;
        compositor = nullptr;
    }
    bool UpdateVRPoses(Quaternion* carQuat, Config::HorizonLock lockSetting) override;
    void SubmitFramesToHMD(IDirect3DDevice9* dev) override;
    FrameTimingInfo GetFrameTiming() override;
    void ResetView() override
    {
        vr::VRChaperone()->ResetZeroPose(vr::ETrackingUniverseOrigin::TrackingUniverseSeated);
    }
    VRRuntime GetRuntimeType() const override
    {
        return OPENVR;
    }
};
