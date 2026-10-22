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

    static rbr::GameMode game_mode;
    static rbr::GameMode previous_game_mode;
    static bool previously_on_btb_stage;
    static uint32_t current_stage_id;
    static M4 horizon_lock_matrix = glm::identity<M4>();
    static glm::vec3 seat_translation;
    static bool is_driving;
    static bool is_rendering_3d;
    static bool is_rendering_car;
    static bool is_rendering_wet_windscreen;
    static M3* car_rotation_ptr;
    static int session_recenter_frame_counter;
    static int stage_recenter_frame_counter = INT32_MAX;
    static bool allow_writetext = true;
    static Hook<decltype(IRBRGameVtbl::WriteText)> writetext_hook;
    static double previous_frame_pitch;
    static double previous_frame_roll;
    static double previous_frame_yaw;
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

    static uintptr_t get_hedgehog_base_address()
    {
        // If ASLR is enabled, the base address is randomized
        static uintptr_t addr;
        if (addr) {
            return addr;
        }

        addr = reinterpret_cast<uintptr_t>(GetModuleHandle("HedgeHog3D.dll"));
        if (!addr) {
            dbg("Could not retrieve Hedgehog base address, this may be bad.");
        }
        return addr;
    }

    static constexpr bool should_render_particles()
    {
        return g::cfg.render_particles || (g::game_mode == GameMode::Replay && g::cfg.always_render_particles_in_replay);
    }

    uintptr_t get_hedgehog_address(uintptr_t target)
    {
        constexpr uintptr_t HEDGEHOG_ABSOLUTE_LOAD_ADDR = 0x10000000;
        return get_hedgehog_base_address() + target - HEDGEHOG_ABSOLUTE_LOAD_ADDR;
    }

    static uintptr_t RENDER_FUNCTION_ADDR = get_address(0x47E1E0);
    static uintptr_t RENDER_PARTICLES_FUNCTION_ADDR = get_address(0x5eff60);
    static uintptr_t RENDER_PARTICLES_FUNCTION_ADDR_2 = get_address(0x5efed0);
    static uintptr_t RENDER_PARTICLES_FUNCTION_ADDR_3 = get_address(0x5effd0);
    static uintptr_t RENDER_PARTICLES_FUNCTION_ADDR_4 = get_address(0x5eca60);
    static uintptr_t PARTICLE_HANDLING_CHECK_ADDR = 0x10076672; // From hedgehog3d.dll. We can't resolve the real address here.
    static uintptr_t CAR_INFO_ADDR = get_address(0x165FC68);
    static uintptr_t* GAME_MODE_EXT_2_PTR = reinterpret_cast<uintptr_t*>(get_address(0x7EA678));
    static auto CAR_ROTATION_OFFSET = 0x16C;
    constexpr auto SEAT_MOVE_MAGIC = 0x0DDB411;

    using ChangeCameraFn = void(__thiscall*)(void* p, int cameraType, uint32_t a);
    using PrepareCameraFn = void(__thiscall*)(void* This, uint32_t a);
    static ChangeCameraFn change_camera_fn = reinterpret_cast<ChangeCameraFn>(get_address(0x487680));
    static PrepareCameraFn apply_camera_position = reinterpret_cast<PrepareCameraFn>(get_address(0x4825B0));
    static PrepareCameraFn apply_camera_fov = reinterpret_cast<PrepareCameraFn>(get_address(0x4BF690));
    using SetupBumperCamFn = void(__thiscall*)(void* This, float a);
    static SetupBumperCamFn setup_bumper_cam = reinterpret_cast<SetupBumperCamFn>(get_address(0x485760));

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

    bool is_using_cfh_camera()
    {
        if (!g::camera_type_ptr) {
            return false;
        }
        return *g::camera_type_ptr == 13;
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

    bool is_rendering_wet_windscreen()
    {
        return g::is_rendering_wet_windscreen;
    }

    static uintptr_t get_camera_info_ptr()
    {
        uintptr_t cameraData = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(CAR_INFO_ADDR) + 0x758);
        return *reinterpret_cast<uintptr_t*>(cameraData + 0x10);
    }

    void change_camera(void* p, uint32_t camera_type)
    {
        const auto camera_info = reinterpret_cast<void*>(get_camera_info_ptr());
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
        return g::is_rendering_3d && g::game_mode != GameMode::MainMenu;
    }

    static constexpr double unwrap_angle(double angle, double reference)
    {
        constexpr auto pi = glm::pi<double>();
        constexpr auto twopi = glm::two_pi<double>();

        double diff = std::fmod(angle - reference, twopi);
        if (diff > pi) {
            return reference + diff - twopi;
        } else if (diff < -pi) {
            return reference + diff + twopi;
        }

        return reference + diff;
    }

    void update_horizon_lock_matrix()
    {
        const auto horizon_lock_game_mode = is_using_cockpit_camera() && (g::game_mode == Driving || g::game_mode == Replay);
        const auto horizon_lock_enabled = g::cfg.lock_to_horizon != HorizonLock::LOCK_NONE;
        if (horizon_lock_enabled) {
            // If car quaternion is given, calculate matrix for low-pass filter
            const auto q = glm::quat_cast(*g::car_rotation_ptr);
            const auto multiplier = static_cast<float>(g::cfg.horizon_lock_multiplier);
            const auto pitch = (g::cfg.lock_to_horizon & HorizonLock::LOCK_PITCH) ? glm::pitch(q) * multiplier : 0.0f;
            const auto roll = (g::cfg.lock_to_horizon & HorizonLock::LOCK_ROLL) ? glm::yaw(q) * multiplier : 0.0f; // somehow in glm the axis is yaw
            const auto yaw = (g::cfg.lock_to_horizon & HorizonLock::LOCK_YAW) ? glm::roll(q) * multiplier : 0.0f; // somehow in glm the axis is roll

            if (horizon_lock_game_mode) {
                const auto pitch_alpha = 1.0 - exp(-(1.0 / g::target_fps) / g::cfg.lowpass_pitch_filter);
                const auto roll_alpha = 1.0 - exp(-(1.0 / g::target_fps) / g::cfg.lowpass_roll_filter);
                const auto yaw_alpha = 1.0 - exp(-(1.0 / g::target_fps) / g::cfg.lowpass_yaw_filter);

                // Unwrap angles to prevent discontinuity at +-pi
                const auto pitch_unwrapped = unwrap_angle(pitch, g::previous_frame_pitch);
                const auto roll_unwrapped = unwrap_angle(roll, g::previous_frame_roll);
                const auto yaw_unwrapped = unwrap_angle(yaw, g::previous_frame_yaw);

                const auto pitch_new = pitch_alpha * pitch_unwrapped + (1.0 - pitch_alpha) * g::previous_frame_pitch;
                const auto roll_new = roll_alpha * roll_unwrapped + (1.0 - roll_alpha) * g::previous_frame_roll;
                const auto yaw_new = yaw_alpha * yaw_unwrapped + (1.0 - yaw_alpha) * g::previous_frame_yaw;

                const auto pitch_offset = pitch_unwrapped - pitch_new;
                const auto roll_offset = roll_unwrapped - roll_new;
                const auto yaw_offset = yaw_unwrapped - yaw_new;

                g::previous_frame_pitch = pitch_new;
                g::previous_frame_roll = roll_new;
                g::previous_frame_yaw = yaw_new;

                glm::quat cancel_car_rotation = glm::normalize(glm::quat(glm::vec3(pitch_offset, -yaw_offset, roll_offset)));
                g::horizon_lock_matrix = glm::mat4_cast(cancel_car_rotation);
            } else {
                // If lowpass mode is selected but we're not in the cockpit camera
                // Update the previous frame values in order to prevent sudden movements
                // when we get back to locking the horizon
                g::previous_frame_pitch = unwrap_angle(pitch, g::previous_frame_pitch);
                g::previous_frame_roll = unwrap_angle(roll, g::previous_frame_roll);
                g::previous_frame_yaw = unwrap_angle(yaw, g::previous_frame_yaw);
                g::horizon_lock_matrix = glm::identity<M4>();
            }
        } else {
            g::horizon_lock_matrix = glm::identity<M4>();
        }
    }

    static bool is_profile_loaded()
    {
        // RSF draws custom menu screen on profile load which does not work well in VR
        // Therefore we enable the 3D menu scene after the profile is loaded and the custom
        // menu screen is cleared.
        static bool loaded = false;
        if (!loaded) {
            const auto menu = *reinterpret_cast<uintptr_t*>(get_address(0x165fa48));
            const auto current_menu = *reinterpret_cast<uintptr_t*>(menu + 0x8);
            const auto menus = reinterpret_cast<void**>(menu + 0x78);
            const auto profile_menu = reinterpret_cast<uintptr_t>(menus[82]);
            if (auto profile = *reinterpret_cast<uintptr_t*>(get_address(0x7d2554)); profile) {
                auto profile_name = reinterpret_cast<const char*>(profile + 0x8);
                if (profile_name[0] != 0 && current_menu != profile_menu) {
                    loaded = true;
                }
            }
        }
        return loaded;
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

    static void __stdcall WriteText(float x, float y, const char* ptxtText)
    {
        // Must be careful here, ECX must not change or we lose
        // the this pointer to the original WriteText method call
        if (!g::allow_writetext) {
            return;
        }

        g::writetext_hook.call(x, y, ptxtText);
    }

    static bool init_or_update_game_data(uintptr_t ptr)
    {
        static bool deprecation_warning_shown = false;
        if (g::cfg.experimental.disable_multiview && !deprecation_warning_shown) [[unlikely]] {
            MessageBoxA(nullptr, "experimental.disableMultiview option is deprecated!\n\nPlease disable the option or remove it from openRBRVR.toml\nto get rid of this message.\n\nIf the game crashes with the option turned off,\nplease report it as a bug.\n\nThis option will be removed in the next release of openRBRVR.", "Deprecation warning", MB_OK);
            deprecation_warning_shown = true;
        }

        if (!g::writetext_hook.call && g::game_mode == GameMode::MainMenu) {
            auto vtbl = get_vtable<IRBRGameVtbl>(g::game);
            g::writetext_hook = Hook(vtbl->WriteText, WriteText);
        }

        if (!g::hooks::load_texture.call) [[unlikely]] {
            auto handle = reinterpret_cast<uintptr_t>(GetModuleHandle("HedgeHog3D.dll"));
            if (!handle) {
                dbg("Could not get handle for HedgeHog3D");
            } else {
                if (!g::hooks::load_texture.call) {
                    g::hooks::load_texture = Hook(*reinterpret_cast<decltype(load_texture)*>(handle + 0xAEC15), load_texture);
                }
            }
        }

        // Swap JZ to JNZ as we want to skip the code if we're not interested
        // in rendering particles. The skipped code is very expensive on stages like Mlynky R.
        // If render_particles has been changed and we already patched the code, revert the change.
        const auto addr = get_hedgehog_address(PARTICLE_HANDLING_CHECK_ADDR);
        const auto current = should_render_particles() ? 0x75 : 0x74;
        const auto wanted = should_render_particles() ? 0x74 : 0x75;
        const volatile uint8_t* p = reinterpret_cast<volatile uint8_t*>(addr);
        if (*p == current) {
            write_byte(addr, wanted);
        }

        auto game_mode = *reinterpret_cast<GameMode*>(ptr + 0x728);
        if (game_mode != g::game_mode) [[unlikely]] {
            g::previous_game_mode = g::game_mode;
            g::game_mode = game_mode;
        }

        if (g::previous_game_mode != g::game_mode && (g::game_mode == GameMode::PreStage || g::game_mode == GameMode::Pause)) {
            // Make sure we reload the seat position whenever the stage is restarted
            g::seat_position_loaded = false;
            // Reset the horizon lock matrix when restarting
            g::horizon_lock_matrix = glm::identity<M4>();
        }

        g::is_driving = g::game_mode == GameMode::Driving;
        g::is_rendering_3d = g::is_driving
            || (g::cfg.menu_scene && g::game_mode == MainMenu && is_profile_loaded() && g::current_stage_id == 4)
            || (g::cfg.render_pausemenu_3d && g::game_mode == GameMode::Pause && g::previous_game_mode != GameMode::Replay)
            || (g::cfg.render_pausemenu_3d && g::game_mode == GameMode::Pause && (g::previous_game_mode == GameMode::Replay && g::cfg.render_replays_3d))
            || (g::cfg.render_prestage_3d && g::game_mode == GameMode::PreStage)
            || (g::cfg.render_replays_3d && g::game_mode == GameMode::Replay);

        if (!g::camera_type_ptr) [[unlikely]] {
            uintptr_t cameraData = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(CAR_INFO_ADDR) + 0x758);
            uintptr_t cameraInfo = *reinterpret_cast<uintptr_t*>(cameraData + 0x10);
            g::camera_type_ptr = reinterpret_cast<uint32_t*>(cameraInfo);
        }

        if (!g::car_rotation_ptr) [[unlikely]] {
            g::car_rotation_ptr = reinterpret_cast<M3*>((ptr + CAR_ROTATION_OFFSET));
        } else {
            update_horizon_lock_matrix();
        }

        // Cache the initial value of quad view rendering
        static bool quad_view_rendering_in_use = g::vr && g::vr->is_using_quad_view_rendering();

        if (quad_view_rendering_in_use) {
            bool restart_session = false;
            const auto on_btb_without_multiview = (!dx::multiview_rendering_enabled()) && is_on_btb_stage();

            if (!g::previously_on_btb_stage && on_btb_without_multiview && g::game_mode == PreStage) {
                // BTB rendering does not play along well with quad view rendering so revert back to stereo rendering for BTB stages
                g::previously_on_btb_stage = true;
                g::cfg.quad_view_rendering = false;
                restart_session = true;
            } else if (g::previously_on_btb_stage && !is_on_btb_stage() && g::game_mode == MainMenu) {
                // Turn quad view rendering back on if we were using it previously
                g::previously_on_btb_stage = false;
                g::cfg.quad_view_rendering = true;
                restart_session = true;
            } else if (!g::previously_on_btb_stage && !on_btb_without_multiview) {
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

                // Run auto-recentering again if needed
                g::session_recenter_frame_counter = 0;
            }
        }

        if (g::game_mode == GameMode::MainMenu && !g::car_textures.empty()) [[unlikely]] {
            // Clear saved car textures if we're in the menu
            // Not sure if this is needed, but better be safe than sorry,
            // the car textures will be reloaded when loading the stage.
            g::car_textures.clear();
            dx::free_btb_shaders();
        }

        if (!g::stage_id_ptr) [[unlikely]] {
            auto game_mode_ext_2 = *reinterpret_cast<uintptr_t*>(*GAME_MODE_EXT_2_PTR + 0x70);
            g::car_id_ptr = reinterpret_cast<uint32_t*>(game_mode_ext_2 + 0x1C);
            g::stage_id_ptr = reinterpret_cast<uint32_t*>(game_mode_ext_2 + 0x20);
        } else if (g::vr && g::current_stage_id != *g::stage_id_ptr) {
            // Stage has changed
            g::stage_recenter_frame_counter = 0;
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

        if (g::vr) {
            if (g::cfg.recenter_at_session_start && g::session_recenter_frame_counter <= 150) {
                g::session_recenter_frame_counter += 1;
                if (g::session_recenter_frame_counter <= 150 && (g::session_recenter_frame_counter % 50 == 0)) {
                    dbg("Auto-recentering");
                    g::vr->reset_view();
                }
            }
            if (g::cfg.recenter_at_stage_start && g::stage_recenter_frame_counter <= 150) {
                g::stage_recenter_frame_counter += 1;
                if (g::stage_recenter_frame_counter <= 150 && (g::stage_recenter_frame_counter % 50 == 0)) {
                    dbg("Auto-recentering");
                    g::vr->reset_view();
                }
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
                const auto menu_scene_active = g::cfg.menu_scene && g::game_mode == GameMode::MainMenu && is_profile_loaded();

                if (menu_scene_active && g::current_stage_id == 4) {
                    // Prevent calling IRBRGame::WriteText so we don't flood the internal
                    // structures by calling it multiple times
                    g::allow_writetext = false;

                    // Use external cam as the camera, but call the same setup function that's used for
                    // camera type 3, which allows custom camera locations.
                    // Using camera type 3 directly doesn't work as it doesn't seem to draw the car model for some reason.
                    rbr::change_camera(p, 1);

                    const auto camera = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(p) + 0x70);
                    float* cam_data = reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(camera) + 0x318);
                    rbr::setup_bumper_cam(camera, 0);

                    // Load the camera matrix that's stored in zxy order
                    M4 cam;
                    cam[0] = glm::vec4(cam_data[1], cam_data[2], cam_data[0], 0.0f);
                    cam[1] = glm::vec4(cam_data[4], cam_data[5], cam_data[3], 0.0f);
                    cam[2] = glm::vec4(cam_data[7], cam_data[8], cam_data[6], 0.0f);
                    cam[3] = glm::vec4(cam_data[10], cam_data[11], cam_data[9], 0.0f);

                    // Put the camera into the garage
                    cam[3].x = 40.4f;
                    cam[3].y = 0.8f;
                    cam[3].z = -14.7f;

                    // Rotate the camera to point directly at the wall where we render the menu
                    static M4 original_cam = cam;
                    const auto yrotated = glm::rotate(original_cam, glm::radians(114.0f), { 0, 0, 1 });
                    const auto xrotated = glm::rotate(yrotated, glm::radians(-4.5f), { 0, 1, 0 });
                    const auto rotated = glm::rotate(xrotated, glm::radians(-2.0f), { 1, 0, 0 });
                    const auto translated = glm::translate(rotated, { 0, -(0.65f - g::cfg.menu_size), 0.1f });

                    // Store data along with wide FoV to reduce rendering glitches (rocks etc.)
                    cam_data[0] = translated[0].z;
                    cam_data[1] = translated[0].x;
                    cam_data[2] = translated[0].y;
                    cam_data[3] = translated[1].z;
                    cam_data[4] = translated[1].x;
                    cam_data[5] = translated[1].y;
                    cam_data[6] = translated[2].z;
                    cam_data[7] = translated[2].x;
                    cam_data[8] = translated[2].y;
                    cam_data[9] = translated[3].z;
                    cam_data[10] = translated[3].x;
                    cam_data[11] = translated[3].y;
                    cam_data[12] = glm::degrees(2.4f);
                }

                dx::render_vr_eye(p, LeftEye);
                if (!dx::multiview_rendering_enabled()) {
                    dx::render_vr_eye(p, RightEye);
                }

                if (g::vr->is_using_quad_view_rendering()) {
                    dx::render_vr_eye(p, FocusLeft);
                    if (!dx::multiview_rendering_enabled()) {
                        dx::render_vr_eye(p, FocusRight);
                    }
                }

                if (g::cfg.companion_mode == CompanionMode::Static && g::game_mode != PreStage && g::game_mode != Replay && g::game_mode != MainMenu) {
                    g::is_rendering_3d = false;

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

                    // We need to clear the target here to use the correct logic in the hooked call
                    if (g::d3d_dev->Clear(0, nullptr, D3DCLEAR_STENCIL | D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0, 0) != D3D_OK) {
                        dbg("Failed to clear co-driver render target");
                    }

                    g::hooks::render.call(p);

                    if (orig_camera != 13) {
                        rbr::change_camera(p, orig_camera);
                    }

                    g::is_rendering_3d = true;
                }

                const auto next_2d_render_target = g::game_mode == GameMode::MainMenu ? GameMenu : Overlay;
                if (g::vr->prepare_vr_rendering(g::d3d_dev, next_2d_render_target)) {
                    g::current_2d_render_target = next_2d_render_target;
                } else {
                    dbg("Failed to set 2D render target");
                    g::current_2d_render_target = std::nullopt;
                    g::d3d_dev->SetRenderTarget(0, g::original_render_target);
                    g::d3d_dev->SetDepthStencilSurface(g::original_depth_stencil_target);
                }

                if (menu_scene_active) {
                    // Change the camera back to the original menu camera
                    rbr::change_camera(p, 7);
                    g::allow_writetext = true;
                    g::hooks::render.call(p);
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

    void __fastcall render_particles(void* This)
    {
        if (should_render_particles()) {
            g::hooks::render_particles.call(This);
        }
    }

    void __fastcall render_particles_2(void* This)
    {
        if (should_render_particles()) {
            g::hooks::render_particles_2.call(This);
        }
    }

    void __fastcall render_particles_3(void* This)
    {
        if (should_render_particles()) {
            g::hooks::render_particles_3.call(This);
        }
    }

    void __fastcall render_particles_4(void* This)
    {
        if (should_render_particles()) {
            g::hooks::render_particles_4.call(This);
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
