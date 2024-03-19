#pragma once

#include "Config.hpp"
#include "D3D.hpp"
#include "Hook.hpp"
#include "RBR.hpp"
#include "VR.hpp"

#include <optional>

// Forward declarations

class IPlugin;
class IRBRGame;
class Menu;
class VRInterface;

// Global variables
// A lot of the work done by the plugin is done in DirectX hooks
// There is no way to pass variables into the hooks, so the code is using a lot of globals
// None of these are thread safe and should not be treated as such

namespace g {
    // Pointer to the plugin
    extern IPlugin* openrbrvr;

    // Pointer to IRBRGame object, from the RBR plugin system
    extern IRBRGame* game;

    // Currently selected menu, if any
    extern Menu* menu;

    // Pointer to VR interface. Valid if VR runtime is up and running.
    extern VRInterface* vr;

    // openRBRVR config. Contains all local modifications
    extern Config cfg;

    // openRBRVR config. has the information that's saved on the disk.
    extern Config saved_cfg;

    // Draw border to the 2D overlay
    extern bool draw_overlay_border;

    // Pointer to hooked D3D device. Used for everything graphics related.
    extern IDirect3DDevice9* d3d_dev;

    // D3D VR interface. Used to invoke VR specific operations on the D3D device.
    extern IDirect3DVR9* d3d_vr;

    // Vector of pointers to RBR base game vertex shaders
    extern std::vector<IDirect3DVertexShader9*> base_game_shaders;

    // Mapping from car name to car textures
    extern std::unordered_map<std::string, IDirect3DTexture9*> car_textures;

    // Current VR render target, if any
    extern std::optional<RenderTarget> vr_render_target;

    // True if there's an error in the VR rendering, and the frame should not be submitted to VR headset
    extern bool vr_error;

    // Timestamp at the start of the current frame
    extern std::chrono::steady_clock::time_point frame_start;

    // Current render target for 2D content
    extern std::optional<RenderTarget> current_2d_render_target;

    // Original RBR screen render target
    extern IDirect3DSurface9* original_render_target;

    // Original RBR screen depth/stencil target
    extern IDirect3DSurface9* original_depth_stencil_target;

    // Pointer to BTB track status information. Non-zero if a BTB stage is loaded.
    extern uint8_t* btb_track_status_ptr;

    // Matrix to flip Z axis around. Because RBR uses left-handed matrices and the OpenVR runtime
    // returns right-handed projection matrices, we need to flip the Z-axis around
    constexpr M4 flip_z_matrix = {
        { 1, 0, 0, 0 },
        { 0, 1, 0, 0 },
        { 0, 0, -1, 0 },
        { 0, 0, 0, 1 },
    };

    // Hooks to DirectX and RBR functions
    namespace hooks {
        // DirectX functions
        extern Hook<decltype(&Direct3DCreate9)> create;
        extern Hook<decltype(IDirect3D9Vtbl::CreateDevice)> create_device;
        extern Hook<decltype(IDirect3DDevice9Vtbl::SetVertexShaderConstantF)> set_vertex_shader_constant_f;
        extern Hook<decltype(IDirect3DDevice9Vtbl::SetTransform)> set_transform;
        extern Hook<decltype(IDirect3DDevice9Vtbl::Present)> present;
        extern Hook<decltype(IDirect3DDevice9Vtbl::CreateVertexShader)> create_vertex_shader;
        extern Hook<decltype(IDirect3DDevice9Vtbl::SetRenderTarget)> btb_set_render_target;
        extern Hook<decltype(IDirect3DDevice9Vtbl::DrawIndexedPrimitive)> draw_indexed_primitive;
        extern Hook<decltype(IDirect3DDevice9Vtbl::DrawPrimitive)> draw_primitive;

        // RBR functions
        extern Hook<decltype(&rbr::load_texture)> load_texture;
        extern Hook<decltype(&rbr::render)> render;
        extern Hook<decltype(&rbr::render_car)> render_car;
        extern Hook<decltype(&rbr::render_particles)> render_particles;
    }
}
