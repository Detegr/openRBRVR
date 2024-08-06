#include "Globals.hpp"

// Default initialization for global variables

namespace g {
    IPlugin* openrbrvr;
    IRBRGame* game;
    VRInterface* vr;
    Config cfg;
    Config saved_cfg;
    bool draw_overlay_border;
    IDirect3DDevice9* d3d_dev;
    ID3D11Device5* d3d11_dev = nullptr;
    ID3D11DeviceContext4* d3d11_ctx = nullptr;
    IDirect3DVR9* d3d_vr;
    std::vector<IDirect3DVertexShader9*> base_game_shaders;
    std::unordered_map<std::string, IDirect3DTexture9*> car_textures;
    std::optional<RenderTarget> vr_render_target;
    bool vr_error;
    std::chrono::steady_clock::time_point frame_start;
    std::optional<RenderTarget> current_2d_render_target;
    IDirect3DSurface9* original_render_target;
    IDirect3DSurface9* original_depth_stencil_target;
    uint8_t* btb_track_status_ptr;
    SeatMovement seat_movement_request;
    bool seat_position_loaded;

    namespace hooks {
        // DirectX functions
        Hook<decltype(&Direct3DCreate9)> create;
        Hook<decltype(IDirect3D9Vtbl::CreateDevice)> create_device;
        Hook<decltype(IDirect3DDevice9Vtbl::SetVertexShaderConstantF)> set_vertex_shader_constant_f;
        Hook<decltype(IDirect3DDevice9Vtbl::SetTransform)> set_transform;
        Hook<decltype(IDirect3DDevice9Vtbl::Present)> present;
        Hook<decltype(IDirect3DDevice9Vtbl::CreateVertexShader)> create_vertex_shader;
        Hook<decltype(IDirect3DDevice9Vtbl::SetRenderTarget)> btb_set_render_target;
        Hook<decltype(IDirect3DDevice9Vtbl::DrawIndexedPrimitive)> draw_indexed_primitive;
        Hook<decltype(IDirect3DDevice9Vtbl::DrawPrimitive)> draw_primitive;

        // RBR functions
        Hook<decltype(&rbr::load_texture)> load_texture;
        Hook<decltype(&rbr::render)> render;
        Hook<decltype(&rbr::render_car)> render_car;
        Hook<decltype(&rbr::render_particles)> render_particles;
        Hook<decltype(&rbr::render_particles_2)> render_particles_2;
        Hook<decltype(&rbr::render_particles_3)> render_particles_3;
        Hook<decltype(&rbr::render_particles_4)> render_particles_4;
        Hook<decltype(&rbr::render_windscreen_effects)> render_windscreen_effects;
        Hook<decltype(&rbr::set_camera_target)> set_camera_target;
    }
}