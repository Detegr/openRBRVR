#pragma once

#include "VR.hpp"

#include <cstdint>
#include <d3d9.h>

namespace rbr {
    enum GameMode : uint32_t {
        NotSet = 0,
        Driving = 1,
        Pause = 2,
        MainMenu = 3,
        Blackout = 4,
        Loading = 5,
        Exiting = 6,
        Quit = 7,
        Replay = 8,
        End = 9,
        PreStage = 10,
        Starting = 12,
    };

    uintptr_t get_address(uintptr_t target);
    uintptr_t get_hedgehog_address(uintptr_t target);
    uintptr_t get_render_function_addr();
    std::array<uintptr_t, 4> get_render_particles_function_addrs();
    GameMode get_game_mode();
    uint32_t get_current_stage_id();

    bool is_on_btb_stage();
    bool is_loading_btb_stage();
    bool is_rendering_3d();
    bool is_using_cockpit_camera();
    bool is_car_texture(IDirect3DBaseTexture9* tex);
    bool is_rendering_wet_windscreen();

    void change_camera(void* p, uint32_t cameraType);
    const M4& get_horizon_lock_matrix();

    bool should_use_reverse_z_buffer();

    // Hookable functions
    void __fastcall render(void* p);
    uint32_t __stdcall load_texture(void* p, const char* texName, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f, uint32_t g, uint32_t h, uint32_t i, uint32_t j, uint32_t k, IDirect3DTexture9** ppTexture);
    void __stdcall render_car(void* a, void* b);
    void __fastcall render_particles(void* This);
    void __fastcall render_particles_2(void* This);
    void __fastcall render_particles_3(void* This);
    void __fastcall render_particles_4(void* This);
    void __fastcall render_windscreen_effects(void* a);
    void* set_camera_target(void* a, float* cam_pos, float* cam_target);
}

namespace rbr_rx {
    constexpr uintptr_t DEVICE_VTABLE_OFFSET = 0x4f56c;
    constexpr uintptr_t TRACK_STATUS_OFFSET = 0x608d0;
    bool is_loaded();
}

namespace rbrhud {
    bool is_loaded();
}
