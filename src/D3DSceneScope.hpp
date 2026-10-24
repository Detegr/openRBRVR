#pragma once

#include <d3d9.h>

// DXVK returns INVALIDCALL from BeginScene when the caller already owns an
// open scene. Draw within that scene, and only end scenes that we opened.
template <typename Device>
class D3DSceneScope {
public:
    explicit D3DSceneScope(Device* device)
        : device_(device)
        , result_(device->BeginScene())
        , owned_(SUCCEEDED(result_))
    {
    }
    ~D3DSceneScope() { finish(); }
    D3DSceneScope(const D3DSceneScope&) = delete;
    D3DSceneScope& operator=(const D3DSceneScope&) = delete;

    bool valid() const { return owned_ || result_ == D3DERR_INVALIDCALL; }
    bool owned() const { return owned_; }
    HRESULT result() const { return result_; }
    HRESULT finish()
    {
        if (!owned_)
            return D3D_OK;
        owned_ = false;
        return device_->EndScene();
    }

private:
    Device* device_;
    HRESULT result_;
    bool owned_;
};
