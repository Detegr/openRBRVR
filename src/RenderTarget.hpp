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

constexpr size_t shared_render_target_count = 2;

constexpr bool is_shared_render_target(RenderTarget t)
{
    return t >= GameMenu;
}

constexpr size_t shared_render_target_index(RenderTarget t)
{
    return static_cast<size_t>(t) - static_cast<size_t>(GameMenu);
}

constexpr RenderTarget render_target_counterpart(RenderTarget t)
{
    if (t == LeftEye)
        return RightEye;
    if (t == RightEye)
        return LeftEye;
    if (t == FocusLeft)
        return FocusRight;
    if (t == FocusRight)
        return FocusLeft;

    throw std::runtime_error("invalid use of render_target_counterpart");
}

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
    uint32_t h,
    bool multiview);

bool is_using_texture_to_render(D3DMULTISAMPLE_TYPE msaa, RenderTarget t, bool multiview);
