#pragma once

#include "API.hpp"
#include "Config.hpp"
#include "D3D.hpp"
#include "Hook.hpp"
#include "RBR.hpp"
#include "VR.hpp"

#include <d3d11.h>
#include <d3d11_4.h>
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

    // Pointer to hooked D3D9 device. Used for everything graphics related except OpenXR.
    extern IDirect3DDevice9* d3d_dev;

    // Pointer to D3D11 device used with OpenXR
    extern ID3D11Device5* d3d11_dev;

    // Pointer to D3D11 device context used with OpenXR
    extern ID3D11DeviceContext4* d3d11_ctx;

    // D3D VR interface. Used to invoke VR specific operations on the D3D device.
    extern IDirect3DVR9* d3d_vr;

    // The register after which we can supply extra data to the vertex shaders
    extern uint32_t base_shader_data_end_register;

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

    // If a seat movement API call is made, the requested direction is stored here for later use
    extern SeatMovement seat_movement_request;

    // Is a saved seat position loaded from disk for the current car
    extern bool seat_position_loaded;

    // String containing the current DXVK version in use
    extern std::string dxvk_version;

    // Hooks to DirectX and RBR functions
    namespace hooks {
        // DirectX functions
        extern Hook<decltype(&Direct3DCreate9)> create;
        extern Hook<decltype(IDirect3D9Vtbl::CreateDevice)> create_device;
        extern Hook<decltype(IDirect3DDevice9Vtbl::SetVertexShaderConstantF)> set_vertex_shader_constant_f;
        extern Hook<decltype(IDirect3DDevice9Vtbl::SetTransform)> set_transform;
        extern Hook<decltype(IDirect3DDevice9Vtbl::Present)> present;
        extern Hook<decltype(IDirect3DDevice9Vtbl::CreateVertexShader)> create_vertex_shader;
        extern Hook<decltype(IDirect3DDevice9Vtbl::GetVertexShader)> get_vertex_shader;
        extern Hook<decltype(IDirect3DDevice9Vtbl::SetVertexShader)> set_vertex_shader;
        extern Hook<decltype(IDirect3DDevice9Vtbl::SetRenderTarget)> btb_set_render_target;
        extern Hook<decltype(IDirect3DDevice9Vtbl::DrawIndexedPrimitive)> draw_indexed_primitive;
        extern Hook<decltype(IDirect3DDevice9Vtbl::DrawPrimitive)> draw_primitive;
        extern Hook<decltype(IDirect3DDevice9Vtbl::SetRenderState)> set_render_state;
        extern Hook<decltype(IDirect3DDevice9Vtbl::Clear)> clear;
        extern Hook<decltype(IDirect3DDevice9Vtbl::EndStateBlock)> end_state_block;
        extern Hook<decltype(IDirect3DStateBlock9Vtbl::Apply)> apply_state_block;

        // RBR functions
        extern Hook<decltype(&rbr::load_texture)> load_texture;
        extern Hook<decltype(&rbr::render)> render;
        extern Hook<decltype(&rbr::render_car)> render_car;
        extern Hook<decltype(&rbr::render_particles)> render_particles;
        extern Hook<decltype(&rbr::render_particles_2)> render_particles_2;
        extern Hook<decltype(&rbr::render_particles_3)> render_particles_3;
        extern Hook<decltype(&rbr::render_particles_4)> render_particles_4;
        extern Hook<decltype(&rbr::render_windscreen_effects)> render_windscreen_effects;
        extern Hook<decltype(&rbr::set_camera_target)> set_camera_target;
    }
}
