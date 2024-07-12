#pragma once

#include "Util.hpp"

enum RenderTarget : size_t {
    LeftEye = 0,
    RightEye = 1,
    FocusLeft = 2,
    FocusRight = 3,
    GameMenu = 4,
    Overlay = 5,
};

bool create_render_target(
    IDirect3DDevice9* dev,
    D3DMULTISAMPLE_TYPE msaa,
    IDirect3DSurface9** msaa_surface,
    IDirect3DSurface9** depth_stencil_surface,
    IDirect3DTexture9** target_texture,
    HANDLE* shared_handle,
    RenderTarget tgt,
    D3DFORMAT fmt,
    uint32_t w,
    uint32_t h);

bool is_using_texture_to_render(D3DMULTISAMPLE_TYPE msaa, RenderTarget t);
