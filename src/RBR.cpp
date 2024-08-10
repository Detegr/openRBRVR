#include "RBR.hpp"
#include "Dx.hpp"
#include "Globals.hpp"
#include "IPlugin.h"
#include "Util.hpp"
#include "VR.hpp"

#include "OpenXR.hpp"
#include <ranges>

// Compilation unit global variables
namespace g {
    static uint32_t* camera_type_ptr;
    static uint32_t* car_id_ptr;
    static uint32_t* stage_id_ptr;
    static M3* car_rotation_ptr;

    static rbr::GameMode game_mode;
    static rbr::GameMode previous_game_mode;
    static bool previously_on_btb_stage;
    static uint32_t current_stage_id;
    static M4 horizon_lock_matrix = glm::identity<M4>();
    static glm::vec3 seat_translation;
    static bool is_driving;
    static bool is_rendering_3d;
    static bool is_rendering_car;
    static bool is_rendering_particles;
    static bool is_rendering_wet_windscreen;
}

namespace rbr {
    static uintptr_t get_base_address()
    {
        // If ASLR is enabled, the base address is randomized
        static uintptr_t addr;
        addr = reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));
        if (!addr) {
            dbg("Could not retrieve RBR base address, this may be bad.");
        }
        return addr;
    }

    uintptr_t get_address(uintptr_t target)
    {
        constexpr uintptr_t RBR_ABSOLUTE_LOAD_ADDR = 0x400000;
        return get_base_address() + target - RBR_ABSOLUTE_LOAD_ADDR;
    }

    static uintptr_t RENDER_FUNCTION_ADDR = get_address(0x47E1E0);
    static uintptr_t RENDER_PARTICLES_FUNCTION_ADDR = get_address(0x5eff60);
    static uintptr_t RENDER_PARTICLES_FUNCTION_ADDR_2 = get_address(0x5efed0);
    static uintptr_t RENDER_PARTICLES_FUNCTION_ADDR_3 = get_address(0x5effd0);
    static uintptr_t RENDER_PARTICLES_FUNCTION_ADDR_4 = get_address(0x5eca60);
    static uintptr_t CAR_INFO_ADDR = get_address(0x165FC68);
    static uintptr_t* GAME_MODE_EXT_2_PTR = reinterpret_cast<uintptr_t*>(get_address(0x7EA678));
    static auto CAR_ROTATION_OFFSET = 0x16C;
    constexpr auto SEAT_MOVE_MAGIC = 0x0DDB411;

    using ChangeCameraFn = void(__thiscall*)(void* p, int cameraType, uint32_t a);
    using PrepareCameraFn = void(__thiscall*)(void* This, uint32_t a);
    static ChangeCameraFn change_camera_fn = reinterpret_cast<ChangeCameraFn>(get_address(0x487680));
    static PrepareCameraFn apply_camera_position = reinterpret_cast<PrepareCameraFn>(get_address(0x4825B0));
    static PrepareCameraFn apply_camera_fov = reinterpret_cast<PrepareCameraFn>(get_address(0x4BF690));

    uintptr_t get_render_function_addr()
    {
        return RENDER_FUNCTION_ADDR;
    }

    std::array<uintptr_t, 4> get_render_particles_function_addrs()
    {
        return { RENDER_PARTICLES_FUNCTION_ADDR, RENDER_PARTICLES_FUNCTION_ADDR_2, RENDER_PARTICLES_FUNCTION_ADDR_3, RENDER_PARTICLES_FUNCTION_ADDR_4 };
    }

    GameMode get_game_mode()
    {
        return g::game_mode;
    }

    uint32_t get_current_stage_id()
    {
        return g::current_stage_id;
    }

    bool is_on_btb_stage()
    {
        return g::btb_track_status_ptr && *g::btb_track_status_ptr == 1;
    }

    bool is_loading_btb_stage()
    {
        return is_on_btb_stage() && g::game_mode == GameMode::Loading;
    }

    bool is_rendering_3d()
    {
        return g::is_rendering_3d;
    }

    bool is_using_cockpit_camera()
    {
        if (!g::camera_type_ptr) {
            return false;
        }

        const auto camera = *g::camera_type_ptr;
        return (camera >= 3) && (camera <= 6);
    }

    bool is_using_internal_camera()
    {
        if (!g::camera_type_ptr) {
            return false;
        }
        return *g::camera_type_ptr == 5;
    }

    bool is_car_texture(IDirect3DBaseTexture9* tex)
    {
        for (const auto& entry : g::car_textures) {
            if (std::get<1>(entry) == tex) {
                return true;
            }
        }
        return false;
    }

    bool is_rendering_car()
    {
        return g::is_rendering_car;
    }

    bool is_rendering_particles()
    {
        return g::is_rendering_particles;
    }

    bool is_rendering_wet_windscreen()
    {
        return g::is_rendering_wet_windscreen;
    }

    void change_camera(void* p, uint32_t camera_type)
    {
        const auto camera_data = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(CAR_INFO_ADDR) + 0x758);
        const auto camera_info = reinterpret_cast<void*>(*reinterpret_cast<uintptr_t*>(camera_data + 0x10));
        const auto camera_fov_this = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(p) + 0xcf4);

        change_camera_fn(camera_info, camera_type, 1);
        apply_camera_position(p, 0);
        apply_camera_fov(camera_fov_this, 0);
    }

    const M4& get_horizon_lock_matrix()
    {
        return g::horizon_lock_matrix;
    }

    bool should_use_reverse_z_buffer()
    {
        return g::game_mode == GameMode::Driving || g::game_mode == GameMode::Replay;
    }

    void update_horizon_lock_matrix()
    {
        auto horizon_lock_game_mode = is_using_cockpit_camera() && (g::game_mode == Driving || g::game_mode == Replay);
        if (horizon_lock_game_mode && (g::cfg.lock_to_horizon != HorizonLock::LOCK_NONE)) {
            // If car quaternion is given, calculate matrix for locking the horizon
            auto q = glm::quat_cast(*g::car_rotation_ptr);
            const auto multiplier = static_cast<float>(g::cfg.horizon_lock_multiplier);
            auto pitch = (g::cfg.lock_to_horizon & HorizonLock::LOCK_PITCH) ? glm::pitch(q) * multiplier : 0.0f;
            auto roll = (g::cfg.lock_to_horizon & HorizonLock::LOCK_ROLL) ? glm::yaw(q) * multiplier : 0.0f; // somehow in glm the axis is yaw

            auto yaw = 0.0f;
            if (g::cfg.horizon_lock_flip) {
                constexpr auto flip_speed = 0.02f;
                static float yaw_flip_progress = 0.0f;
                static int frames_upside_down = 0;

                // TODO: This still works awfully bad in some situations. A better solution is needed.
                // Flip the yaw if the car goes upside down (by pitch)
                // To prevent jerky movement, make sure the car has been upside down for a while before adjusting the view
                // Also smooth out the rotation of the camera with linear interpolation
                if (pitch > glm::radians(90.0f) || pitch < glm::radians(-90.0f)) {
                    frames_upside_down += 1;
                    if (frames_upside_down > 50) {
                        yaw_flip_progress = std::min(yaw_flip_progress + flip_speed, 1.0f);
                    }
                } else {
                    yaw_flip_progress = std::max(yaw_flip_progress - flip_speed, 0.0f);
                    frames_upside_down = 0;
                }
                yaw = std::lerp(0.0f, glm::radians(180.0f), yaw_flip_progress);
            }

            glm::quat cancel_car_rotation = glm::normalize(glm::quat(glm::vec3(pitch, yaw, roll)));
            g::horizon_lock_matrix = glm::mat4_cast(cancel_car_rotation);
        } else {
            g::horizon_lock_matrix = glm::identity<M4>();
        }
    }

    static void force_camera_change(void* p, uint32_t magic)
    {
        auto cam = *g::camera_type_ptr;
        *g::camera_type_ptr = magic;
        change_camera(p, cam);
    }

    static bool update_seat_translation(SeatMovement movement)
    {
        constexpr auto amount = 0.001f;

        switch (movement) {
            case SeatMovement::MOVE_SEAT_STOP: break;
            case SeatMovement::MOVE_SEAT_FORWARD: {
                g::seat_translation.z -= amount;
                return true;
            }
            case SeatMovement::MOVE_SEAT_RIGHT: {
                g::seat_translation.x -= amount;
                return true;
            }
            case SeatMovement::MOVE_SEAT_BACKWARD: {
                g::seat_translation.z += amount;
                return true;
            }
            case SeatMovement::MOVE_SEAT_LEFT: {
                g::seat_translation.x += amount;
                return true;
            }
            case SeatMovement::MOVE_SEAT_UP: {
                g::seat_translation.y += amount;
                return true;
            }
            case SeatMovement::MOVE_SEAT_DOWN: {
                g::seat_translation.y -= amount;
                return true;
            }
        }
        return false;
    }

    static void update_render_context()
    {
        // Use per-stage render context
        bool found = false;
        for (const auto& v : g::cfg.gfx) {
            const std::vector<int>& stages = v.second.stage_ids;
            if (std::find(stages.cbegin(), stages.cend(), g::current_stage_id) != stages.cend()) {
                g::vr->set_render_context(v.first);
                found = true;
                break;
            }
        }
        if (!found) {
            g::vr->set_render_context("default");
        }
    }

    static bool init_or_update_game_data(uintptr_t ptr)
    {
        if (!g::hooks::render_car.call || !g::hooks::load_texture.call) [[unlikely]] {
            auto handle = reinterpret_cast<uintptr_t>(GetModuleHandle("HedgeHog3D.dll"));
            if (!handle) {
                dbg("Could not get handle for HedgeHog3D");
            } else {
                if (!g::hooks::load_texture.call) {
                    g::hooks::load_texture = Hook(*reinterpret_cast<decltype(load_texture)*>(handle + 0xAEC15), load_texture);
                }
                if (!g::hooks::render_car.call) {
                    g::hooks::render_car = Hook(*reinterpret_cast<decltype(render_car)*>(handle + 0x7BC60), render_car);
                }
            }
        }

        auto game_mode = *reinterpret_cast<GameMode*>(ptr + 0x728);
        if (game_mode != g::game_mode) [[unlikely]] {
            g::previous_game_mode = g::game_mode;
            g::game_mode = game_mode;
        }

        if (g::previous_game_mode != g::game_mode && g::game_mode == GameMode::PreStage) {
            // Make sure we reload the seat position whenever the stage is restarted
            g::seat_position_loaded = false;
        }

        g::is_driving = g::game_mode == GameMode::Driving;
        g::is_rendering_3d = g::is_driving
            || (g::cfg.render_mainmenu_3d && g::game_mode == GameMode::MainMenu)
            || (g::cfg.render_pausemenu_3d && g::game_mode == GameMode::Pause && g::previous_game_mode != GameMode::Replay)
            || (g::cfg.render_pausemenu_3d && g::game_mode == GameMode::Pause && (g::previous_game_mode == GameMode::Replay && g::cfg.render_replays_3d))
            || (g::cfg.render_prestage_3d && g::game_mode == GameMode::PreStage)
            || (g::cfg.render_replays_3d && g::game_mode == GameMode::Replay);

        if (!g::car_rotation_ptr) [[unlikely]] {
            g::car_rotation_ptr = reinterpret_cast<M3*>((ptr + CAR_ROTATION_OFFSET));
        } else {
            update_horizon_lock_matrix();
        }

        // Cache the initial value of quad view rendering
        static bool quad_view_rendering_in_use = g::vr && g::vr->is_using_quad_view_rendering();

        if (quad_view_rendering_in_use) {
            bool restart_session = false;

            if (!g::previously_on_btb_stage && is_on_btb_stage() && g::game_mode == PreStage) {
                // BTB rendering does not play along well with quad view rendering so revert back to stereo rendering for BTB stages
                g::previously_on_btb_stage = true;
                g::cfg.quad_view_rendering = false;
                restart_session = true;
            } else if (g::previously_on_btb_stage && !is_on_btb_stage() && g::game_mode == MainMenu) {
                // Turn quad view rendering back on if we were using it previously
                g::previously_on_btb_stage = false;
                g::cfg.quad_view_rendering = true;
                restart_session = true;
            } else if (!g::previously_on_btb_stage && !is_on_btb_stage()) {
                bool wanted_quad_view_mode = g::vr->get_current_render_context()->quad_view_rendering;
                restart_session = g::cfg.quad_view_rendering != wanted_quad_view_mode;
                g::cfg.quad_view_rendering = wanted_quad_view_mode;
            }

            if (restart_session) {
                dbg("Restarting OpenXR session");

                const auto w = static_cast<uint32_t>(g::vr->companion_window_width);
                const auto h = static_cast<uint32_t>(g::vr->companion_window_height);

                delete g::vr;
                g::vr = new OpenXR();
                reinterpret_cast<OpenXR*>(g::vr)->init(g::d3d_dev, &g::d3d_vr, w, h);

                // Reload render context in case it was not the default
                update_render_context();
            }
        }

        if (g::game_mode == GameMode::MainMenu && !g::car_textures.empty()) [[unlikely]] {
            // Clear saved car textures if we're in the menu
            // Not sure if this is needed, but better be safe than sorry,
            // the car textures will be reloaded when loading the stage.
            g::car_textures.clear();
        }

        if (!g::camera_type_ptr) [[unlikely]] {
            uintptr_t cameraData = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(CAR_INFO_ADDR) + 0x758);
            uintptr_t cameraInfo = *reinterpret_cast<uintptr_t*>(cameraData + 0x10);
            g::camera_type_ptr = reinterpret_cast<uint32_t*>(cameraInfo);
        }

        if (!g::stage_id_ptr) [[unlikely]] {
            auto game_mode_ext_2 = *reinterpret_cast<uintptr_t*>(*GAME_MODE_EXT_2_PTR + 0x70);
            g::car_id_ptr = reinterpret_cast<uint32_t*>(game_mode_ext_2 + 0x1C);
            g::stage_id_ptr = reinterpret_cast<uint32_t*>(game_mode_ext_2 + 0x20);
        } else if (g::vr && g::current_stage_id != *g::stage_id_ptr) {
            // Stage has changed
            g::current_stage_id = *g::stage_id_ptr;
            g::seat_position_loaded = false;
            update_render_context();
        }

        if (g::game_mode == GameMode::Driving) [[likely]] {
            if (is_using_internal_camera()) {
                if (!g::seat_position_loaded) {
                    // Load saved seat translation matrix for the current car
                    auto [translation, is_openrbrvr_translation] = g::cfg.load_seat_translation(*g::car_id_ptr);
                    g::seat_translation = translation;

                    if (is_openrbrvr_translation) {
                        force_camera_change(reinterpret_cast<void*>(ptr), SEAT_MOVE_MAGIC);
                    }
                    g::seat_position_loaded = true;
                }

                static int seat_still_frames = 0;
                static bool seat_saved = true;

                if (update_seat_translation(g::seat_movement_request)) {
                    seat_saved = false;
                    seat_still_frames = 0;
                    force_camera_change(reinterpret_cast<void*>(ptr), SEAT_MOVE_MAGIC);
                } else {
                    seat_still_frames++;
                }

                if (!seat_saved && seat_still_frames > 250) {
                    if (g::cfg.insert_or_update_seat_translation(*g::car_id_ptr, g::seat_translation)) {
                        g::game->WriteGameMessage("openRBRVR seat position saved.", 2.0, 100.0, 100.0);
                    } else {
                        g::game->WriteGameMessage("Failed to save openRBRVR seat position.", 2.0, 100.0, 100.0);
                    }
                    seat_saved = true;
                }
            } else if (g::seat_movement_request) {
                g::game->WriteGameMessage("openRBRVR seat position adjustment is implemented for internal camera only.", 2.0, 100.0, 100.0);
            }
        }

        return *reinterpret_cast<uint32_t*>(ptr + 0x720) == 0;
    }

    void* set_camera_target(void* a, float* cam_pos, float* cam_target)
    {
        if (*g::camera_type_ptr == SEAT_MOVE_MAGIC) {
            memcpy(cam_pos, glm::value_ptr(g::seat_translation), 3 * sizeof(float));
            // Align the position and target values to point the camera directly forward
            memcpy(cam_target, cam_pos, 3 * sizeof(float));
            cam_target[2] -= 100.f; // Adjust Z target far forward
        }
        return g::hooks::set_camera_target.call(a, cam_pos, cam_target);
    }

    // RBR 3D scene draw function is rerouted here
    void __fastcall render(void* p)
    {
        auto do_rendering = init_or_update_game_data(reinterpret_cast<uintptr_t>(p));

        if (g::d3d_dev->GetRenderTarget(0, &g::original_render_target) != D3D_OK) [[unlikely]] {
            dbg("Could not get render original target");
        }
        if (g::d3d_dev->GetDepthStencilSurface(&g::original_depth_stencil_target) != D3D_OK) [[unlikely]] {
            dbg("Could not get render original depth stencil surface");
        }

        if (!do_rendering) [[unlikely]] {
            return;
        }

        if (g::vr) [[likely]] {
            // UpdateVRPoses should be called as close to rendering as possible
            if (!g::vr->update_vr_poses()) {
                dbg("UpdateVRPoses failed, skipping frame");
                g::vr_error = true;
                return;
            }

            g::frame_start = std::chrono::steady_clock::now();

            if (g::is_rendering_3d) {
                dx::render_vr_eye(p, LeftEye);
                dx::render_vr_eye(p, RightEye);

                if (g::vr->is_using_quad_view_rendering()) {
                    dx::render_vr_eye(p, FocusLeft);
                    dx::render_vr_eye(p, FocusRight);
                }

                if (g::cfg.companion_mode == CompanionMode::Static && g::game_mode != PreStage && g::game_mode != Replay) {
                    auto orig_camera = *g::camera_type_ptr;
                    // 13 is the CFH camera. We don't want to change the camera while it is active.
                    if (orig_camera != 13) {
                        rbr::change_camera(p, 4);
                    }
                    if (g::d3d_dev->SetRenderTarget(0, g::original_render_target) != D3D_OK) {
                        dbg("Failed to reset render target to original");
                    }
                    if (g::d3d_dev->SetDepthStencilSurface(g::original_depth_stencil_target) != D3D_OK) {
                        dbg("Failed to reset depth stencil surface to original");
                    }
                    g::hooks::render.call(p);
                    if (orig_camera != 13) {
                        rbr::change_camera(p, orig_camera);
                    }
                }

                if (g::vr->prepare_vr_rendering(g::d3d_dev, Overlay)) {
                    g::current_2d_render_target = Overlay;
                } else {
                    dbg("Failed to set 2D render target");
                    g::current_2d_render_target = std::nullopt;
                    g::d3d_dev->SetRenderTarget(0, g::original_render_target);
                    g::d3d_dev->SetDepthStencilSurface(g::original_depth_stencil_target);
                }
            } else {
                // We are not driving, render the scene into a plane
                g::vr_render_target = std::nullopt;
                auto should_swap_render_target = !(is_loading_btb_stage() && !g::cfg.draw_loading_screen);
                if (should_swap_render_target) {
                    if (g::vr->prepare_vr_rendering(g::d3d_dev, GameMenu)) {
                        g::current_2d_render_target = GameMenu;
                    } else {
                        dbg("Failed to set 2D render target");
                        g::current_2d_render_target = std::nullopt;
                        g::d3d_dev->SetRenderTarget(0, g::original_render_target);
                        g::d3d_dev->SetDepthStencilSurface(g::original_depth_stencil_target);
                        return;
                    }
                }

                auto should_render = !(g::game_mode == GameMode::Loading && !g::cfg.draw_loading_screen);
                if (should_render) {
                    g::hooks::render.call(p);
                }
            }
        } else {
            g::hooks::render.call(p);
        }
    }

    uint32_t __stdcall load_texture(void* p, const char* tex_name, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f, uint32_t g, uint32_t h, uint32_t i, uint32_t j, uint32_t k, IDirect3DTexture9** pp_texture)
    {
        auto ret = g::hooks::load_texture.call(p, tex_name, a, b, c, d, e, f, g, h, i, j, k, pp_texture);
        auto tex = std::string(tex_name)
            | std::ranges::views::transform([](char c) { return std::tolower(c); })
            | std::ranges::to<std::string>();

        if (ret == 0 && (tex.starts_with("cars\\") || tex.starts_with("cars/") || tex.starts_with("textures/ws_broken") || tex.starts_with("textures\\ws_broken"))) {
            g::car_textures[tex] = *pp_texture;
        }
        return ret;
    }

    void __stdcall render_car(void* a, void* b)
    {
        g::is_rendering_car = true;
        g::hooks::render_car.call(a, b);
        g::is_rendering_car = false;
    }

    void __fastcall render_particles(void* This)
    {
        if (g::cfg.render_particles) {
            g::is_rendering_particles = true;
            g::hooks::render_particles.call(This);
            g::is_rendering_particles = false;
        }
    }

    void __fastcall render_particles_2(void* This)
    {
        if (g::cfg.render_particles) {
            g::is_rendering_particles = true;
            g::hooks::render_particles_2.call(This);
            g::is_rendering_particles = false;
        }
    }

    void __fastcall render_particles_3(void* This)
    {
        if (g::cfg.render_particles) {
            g::is_rendering_particles = true;
            g::hooks::render_particles_3.call(This);
            g::is_rendering_particles = false;
        }
    }

    void __fastcall render_particles_4(void* This)
    {
        if (g::cfg.render_particles) {
            g::is_rendering_particles = true;
            g::hooks::render_particles_4.call(This);
            g::is_rendering_particles = false;
        }
    }

    void __fastcall render_windscreen_effects(void* a)
    {
        g::is_rendering_wet_windscreen = true;
        g::hooks::render_windscreen_effects.call(a);
        g::is_rendering_wet_windscreen = false;
    }
}

namespace rbr_rx {
    bool is_loaded()
    {
        return reinterpret_cast<uintptr_t>(g::btb_track_status_ptr) != TRACK_STATUS_OFFSET;
    }
}

namespace rbrhud {
    bool is_loaded()
    {
        static bool tried_to_obtain;
        static bool is_loaded;

        if (!tried_to_obtain) {
            auto handle = GetModuleHandle("Plugins\\RBRHUD.dll");
            is_loaded = (handle != nullptr);
        }
        return is_loaded;
    }
}
