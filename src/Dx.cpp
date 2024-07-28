#include "Dx.hpp"
#include "Globals.hpp"
#include "IPlugin.h"
#include "OpenVR.hpp"
#include "OpenXR.hpp"
#include "RBR.hpp"
#include "Util.hpp"
#include "Version.hpp"

#include <gtx/matrix_decompose.hpp>

// Compilation unit global variables
namespace g {
    static std::chrono::steady_clock::time_point second_start;
    static int fps;
    static int target_fps;
    static int current_frames;
}

namespace dx {
    namespace shader {
        static M4 current_projection_matrix;
        static M4 current_projection_matrix_inverse;
    }

    namespace fixedfunction {
        static D3DMATRIX current_projection_matrix;
        static D3DMATRIX current_view_matrix;
    }

    using rbr::GameMode;

    // Call the RBR render function with a texture as the render target
    // Even though the render pipeline changes the render target while rendering,
    // the original render target is respected and restored at the end of the pipeline.
    void render_vr_eye(void* p, RenderTarget eye, bool clear)
    {
        g::vr_render_target = eye;
        if (g::vr->prepare_vr_rendering(g::d3d_dev, eye, clear)) {
            g::hooks::render.call(p);
            g::vr->finish_vr_rendering(g::d3d_dev, eye);
        } else {
            dbg("Failed to set 3D render target");
            g::d3d_dev->SetRenderTarget(0, g::original_render_target);
            g::d3d_dev->SetDepthStencilSurface(g::original_depth_stencil_target);
        }
        g::vr_render_target = std::nullopt;
    }

    HRESULT __stdcall CreateVertexShader(IDirect3DDevice9* This, const DWORD* pFunction, IDirect3DVertexShader9** ppShader)
    {
        static int i = 0;
        auto ret = g::hooks::create_vertex_shader.call(g::d3d_dev, pFunction, ppShader);
        if (i < 40) {
            // These are the base game shaders for RBR that need
            // to be patched with the VR projection.
            g::base_game_shaders.push_back(*ppShader);
        }
        i++;
        return ret;
    }

    static void draw_debug_info(uint64_t cpu_frametime_us)
    {
        g::game->SetColor(1, 0, 1, 1.0);
        g::game->SetFont(IRBRGame::EFonts::FONT_DEBUG);

        float i = 0.0f;
        auto t = g::vr->get_frame_timing();
        const float cpuTime = cpu_frametime_us / 1000.0f;
        static float gpu_total = 0.0f;

        if (g::vr->get_runtime_type() == OPENXR) {
            if (t.gpu_total > 0.0) {
                // Cache non-zero GPU total value as we won't get a new value for every frame
                gpu_total = t.gpu_total * 1000.0f;
            }
        }

        if (g::cfg.debug_mode == 0) {
            g::game->WriteText(0, 18 * ++i, std::format("openRBRVR {}", VERSION_STR).c_str());
            g::game->WriteText(0, 18 * ++i, std::format("VR runtime: {}", g::vr->get_runtime_type() == OPENVR ? "OpenVR (SteamVR)" : "OpenXR").c_str());

            if (g::vr->get_runtime_type() == OPENVR) {
                static uint32_t totalDroppedFrames = 0;
                static uint32_t totalMispresentedFrames = 0;
                totalDroppedFrames += t.dropped_frames;
                totalMispresentedFrames += t.mispresented_frames;

                g::game->WriteText(0, 18 * ++i,
                    std::format("CPU: render time: {:.2f}ms, present: {:.2f}ms, waitforpresent: {:.2f}ms",
                        cpuTime,
                        t.cpu_present_call,
                        t.cpu_wait_for_present)
                        .c_str());
                g::game->WriteText(0, 18 * ++i,
                    std::format("GPU: presubmit {:.2f}ms, postSubmit: {:.2f}ms, total: {:.2f}ms",
                        t.gpu_pre_submit,
                        t.gpu_post_submit,
                        t.gpu_total)
                        .c_str());
                g::game->WriteText(0, 18 * ++i,
                    std::format("Compositor: CPU: {:.2f}ms, GPU: {:.2f}ms, submit: {:.2f}ms",
                        t.compositor_cpu,
                        t.compositor_gpu,
                        t.compositor_submit_frame)
                        .c_str());
                g::game->WriteText(0, 18 * ++i,
                    std::format("Total mispresented frames: {}, Total dropped frames: {}, Reprojection flags: {:X}",
                        totalMispresentedFrames,
                        totalDroppedFrames,
                        t.reprojection_flags)
                        .c_str());
            } else {
                g::game->WriteText(0, 18 * ++i, std::format("CPU: render time: {:.2f}ms", cpuTime).c_str());
                g::game->WriteText(0, 18 * ++i, std::format("GPU: render time: {:.2f}ms", gpu_total).c_str());
            }

            g::game->WriteText(0, 18 * ++i, std::format("Mods: {} {}", rbr_rx::is_loaded() ? "RBRRX" : "", rbrhud::is_loaded() ? "RBRHUD" : "").c_str());
            const auto& [lw, lh] = g::vr->get_render_resolution(LeftEye);
            const auto& [rw, rh] = g::vr->get_render_resolution(RightEye);
            g::game->WriteText(0, 18 * ++i, std::format("Render resolution: {}x{} (left), {}x{} (right)", lw, lh, rw, rh).c_str());
            if (g::vr->is_using_quad_view_rendering()) {
                const auto& [flw, flh] = g::vr->get_render_resolution(FocusLeft);
                const auto& [frw, frh] = g::vr->get_render_resolution(FocusRight);
                g::game->WriteText(0, 18 * ++i, std::format("                         {}x{} (focus left), {}x{} (focus right)", flw, flh, frw, frh).c_str());
            }
            if (g::vr->is_using_quad_view_rendering()) {
                g::game->WriteText(0, 18 * ++i, std::format("Anti-aliasing: {}x, peripheral: {}x", static_cast<int>(g::vr->get_current_render_context()->msaa), static_cast<int>(g::cfg.peripheral_msaa)).c_str());
            } else {
                g::game->WriteText(0, 18 * ++i, std::format("Anti-aliasing: {}x", static_cast<int>(g::vr->get_current_render_context()->msaa)).c_str());
            }
            g::game->WriteText(0, 18 * ++i, std::format("Anisotropic filtering: {}x", g::cfg.anisotropy).c_str());
            g::game->WriteText(0, 18 * ++i, std::format("Current stage ID: {}", rbr::get_current_stage_id()).c_str());
        } else {
            float frameTime;
            if (g::vr->get_runtime_type() == OPENVR) {
                frameTime = std::max<float>(cpuTime, t.gpu_total);
            } else {
                frameTime = std::max<float>(cpuTime, gpu_total);
            }

            if (g::fps > 0 && g::target_fps > 0) {
                const auto target = 1000.0 / g::target_fps;
                if (frameTime / target < 0.7) {
                    g::game->SetColor(0.7f, 1.0f, 0, 1.0f);
                } else if (g::fps < (g::target_fps - 1)) {
                    g::game->SetColor(1.0f, 0.0f, 0, 1.0f);
                } else {
                    g::game->SetColor(1.0f, 0.6f, 0, 1.0f);
                }

                g::game->WriteText(0, 0, std::format("{}", g::fps).c_str());
            }
        }
    }

    // Render `renderTarget2d` on a plane for both eyes
    static void render_vr_overlay(RenderTarget render_target_2d, bool clear)
    {
        const auto& size = render_target_2d == GameMenu ? g::cfg.menu_size : g::cfg.overlay_size;
        const auto& translation = render_target_2d == Overlay ? g::cfg.overlay_translation : glm::vec3 { 0.0f, -0.1f, 0.0f };
        const auto& horizon_lock = render_target_2d == Overlay ? std::make_optional(rbr::get_horizon_lock_matrix()) : std::nullopt;
        const auto projection_type = rbr::get_game_mode() == GameMode::MainMenu ? Projection::MainMenu : Projection::Cockpit;
        const auto& texture = g::vr->get_texture(render_target_2d);

        if (g::vr->prepare_vr_rendering(g::d3d_dev, LeftEye, clear)) [[likely]] {
            render_menu_quad(g::d3d_dev, g::vr, texture, LeftEye, render_target_2d, projection_type, size, translation, horizon_lock);
            g::vr->finish_vr_rendering(g::d3d_dev, LeftEye);
        } else {
            dbg("Failed to render left eye overlay");
        }

        if (g::vr->prepare_vr_rendering(g::d3d_dev, RightEye, clear)) [[likely]] {
            render_menu_quad(g::d3d_dev, g::vr, texture, RightEye, render_target_2d, projection_type, size, translation, horizon_lock);
            g::vr->finish_vr_rendering(g::d3d_dev, RightEye);
        } else {
            dbg("Failed to render left eye overlay");
        }

        if (g::vr->is_using_quad_view_rendering()) {
            if (g::vr->prepare_vr_rendering(g::d3d_dev, FocusLeft, clear)) {
                render_menu_quad(g::d3d_dev, g::vr, texture, FocusLeft, render_target_2d, projection_type, size, translation, horizon_lock);
                g::vr->finish_vr_rendering(g::d3d_dev, FocusLeft);
            } else {
                dbg("Failed to render left eye overlay");
            }

            if (g::vr->prepare_vr_rendering(g::d3d_dev, FocusRight, clear)) {
                render_menu_quad(g::d3d_dev, g::vr, texture, FocusRight, render_target_2d, projection_type, size, translation, horizon_lock);
                g::vr->finish_vr_rendering(g::d3d_dev, FocusRight);
            } else {
                dbg("Failed to render left eye overlay");
            }
        }
    }

    HRESULT __stdcall Present(IDirect3DDevice9* This, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion)
    {
        if (g::vr && !g::vr_error) [[likely]] {
            auto shouldRender = g::current_2d_render_target && !(rbr::is_loading_btb_stage() && !g::cfg.draw_loading_screen);
            if (shouldRender) {
                if (g::draw_overlay_border) {
                    render_overlay_border(g::d3d_dev, g::vr->get_current_render_context()->overlay_border);
                }
                g::vr->finish_vr_rendering(g::d3d_dev, g::current_2d_render_target.value());
                render_vr_overlay(g::current_2d_render_target.value(), !rbr::is_rendering_3d());
            }
        }

        if (g::d3d_dev->SetRenderTarget(0, g::original_render_target) != D3D_OK) {
            dbg("Failed to reset render target to original");
        }
        if (g::d3d_dev->SetDepthStencilSurface(g::original_depth_stencil_target) != D3D_OK) {
            dbg("Failed to reset depth stencil surface to original");
        }
        auto ret = 0;
        if (g::vr && !g::vr_error) {
            auto game_mode = rbr::get_game_mode();
            const auto companion_eye = static_cast<RenderTarget>(g::cfg.companion_eye + (g::vr->is_using_quad_view_rendering() ? 2 : 0));
            if (g::cfg.companion_mode == CompanionMode::Static) {
                if (game_mode == GameMode::Driving || game_mode == GameMode::Pause) {
                    // Render the overlay over the 3D content, if we're running the static view on desktop window
                    // Otherwise, the help texts, pause menu, pacenote plugin UI etc. won't be visible
                    render_companion_window_from_render_target(g::d3d_dev, g::vr, Overlay);
                } else if (game_mode == GameMode::PreStage) {
                    render_companion_window_from_render_target(g::d3d_dev, g::vr, g::cfg.render_prestage_3d ? companion_eye : GameMenu);
                } else if (game_mode != GameMode::Driving) {
                    render_companion_window_from_render_target(g::d3d_dev, g::vr, rbr::is_rendering_3d() ? companion_eye : GameMenu);
                }
            } else if (g::cfg.companion_mode == CompanionMode::VREye || game_mode != GameMode::Driving) {
                render_companion_window_from_render_target(g::d3d_dev, g::vr, rbr::is_rendering_3d() ? companion_eye : GameMenu);
            }
            g::vr->prepare_frames_for_hmd(g::d3d_dev);
        }

        auto frame_end = std::chrono::steady_clock::now();
        auto cpu_frame_time = std::chrono::duration_cast<std::chrono::microseconds>(frame_end - g::frame_start);

        ret = g::hooks::present.call(g::d3d_dev, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
        g::current_frames++;

        if (g::vr && !g::vr_error) {
            g::vr->submit_frames_to_hmd(g::d3d_dev);

            if (g::cfg.debug) {
                draw_debug_info(cpu_frame_time.count());
            }
        }

        g::original_render_target->Release();
        g::original_depth_stencil_target->Release();

        if (std::chrono::steady_clock::now() - g::second_start > std::chrono::seconds(1)) {
            g::second_start = std::chrono::steady_clock::now();
            g::fps = g::current_frames - 1;
            g::target_fps = std::max(g::fps, g::target_fps);
            g::current_frames = 0;
        }

        g::vr_error = false;
        return ret;
    }

    HRESULT __stdcall SetVertexShaderConstantF(IDirect3DDevice9* This, UINT StartRegister, const float* pConstantData, UINT Vector4fCount)
    {
        IDirect3DVertexShader9* shader;
        if (auto ret = g::d3d_dev->GetVertexShader(&shader); ret != D3D_OK) {
            dbg("Could not get vertex shader");
            return ret;
        }

        auto is_base_shader = true;
        if (rbr::is_on_btb_stage()) {
            is_base_shader = std::find(g::base_game_shaders.cbegin(), g::base_game_shaders.cend(), shader) != g::base_game_shaders.end();
        }
        if (shader)
            shader->Release();

        if (is_base_shader && g::vr_render_target && Vector4fCount == 4) {
            // The cockpit of the car is drawn with a projection matrix that has smaller near Z value
            // Otherwise with wide FoV headsets part of the car will be clipped out.
            // Separate projection matrix is used because if the small near Z value is used for all shaders
            // it will cause Z fighting (flickering) in the trackside objects. With this we get best of both worlds.
            IDirect3DBaseTexture9* texture;
            if (auto ret = g::d3d_dev->GetTexture(0, &texture); ret != D3D_OK) {
                dbg("Could not get texture");
                return ret;
            }

            auto is_rendering_cockpit = rbr::is_using_cockpit_camera() && (rbr::is_rendering_car() || rbr::is_car_texture(texture) || rbr::is_rendering_particles());
            if (texture) {
                texture->Release();
            }

            if (StartRegister == 0) {
                const auto orig = glm::transpose(m4_from_shader_constant_ptr(pConstantData));
                const auto& projection = g::vr->get_projection(g::vr_render_target.value(), is_rendering_cockpit ? Projection::Cockpit : Projection::Stage);
                const auto& eyepos = g::vr->get_eye_pos(g::vr_render_target.value());
                const auto& pose = g::vr->get_pose(g::vr_render_target.value());

                // MVP matrix
                // MV = P^-1 * MVP
                // MVP[VRRenderTarget] = P[VRRenderTarget] * MV
                const auto mv = shader::current_projection_matrix_inverse * orig;
                const auto mvp = glm::transpose(projection * eyepos * pose * g::flip_z_matrix * rbr::get_horizon_lock_matrix() * mv);
                return g::hooks::set_vertex_shader_constant_f.call(g::d3d_dev, StartRegister, glm::value_ptr(mvp), Vector4fCount);
            } else if (StartRegister == 20) {
                // Sky/fog
                // It seems this parameter contains the orientation of the car and the
                // skybox and fog is rendered correctly only in the direction where the car
                // points at. By rotating this with the HMD's rotation, the skybox and possible
                // fog is rendered correctly.
                const auto orig = glm::transpose(m4_from_shader_constant_ptr(pConstantData));
                glm::vec4 perspective;
                glm::vec3 scale, translation, skew;
                glm::quat orientation;

                // Always use left eye for the pose, as some effects (like the "darkness" effect in Mitterbach Tarmac night version)
                // may render differently in each eye, especially if the object is far away, which makes it look awful.
                // For the fog, the orientation is close enough for both eyes when always rendered with the same eye.
                glm::decompose(g::vr->get_pose(LeftEye), scale, orientation, translation, skew, perspective);
                const auto rotation = glm::mat4_cast(glm::conjugate(orientation));

                const auto m = glm::transpose(rotation * orig);
                return g::hooks::set_vertex_shader_constant_f.call(g::d3d_dev, StartRegister, glm::value_ptr(m), Vector4fCount);
            }
        }
        return g::hooks::set_vertex_shader_constant_f.call(g::d3d_dev, StartRegister, pConstantData, Vector4fCount);
    }

    HRESULT __stdcall SetTransform(IDirect3DDevice9* This, D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix)
    {
        if (g::vr_render_target && State == D3DTS_PROJECTION) {
            shader::current_projection_matrix = m4_from_d3d(*pMatrix);
            shader::current_projection_matrix_inverse = glm::inverse(shader::current_projection_matrix);

            // Use the large space z-value projection matrix by default. The car specific parts
            // are patched to be rendered in gCockpitProjection in DrawIndexedPrimitive.
            const auto& projection = g::vr->get_projection(g::vr_render_target.value(), Projection::Stage);
            fixedfunction::current_projection_matrix = d3d_from_m4(projection);
            return g::hooks::set_transform.call(g::d3d_dev, State, &fixedfunction::current_projection_matrix);
        } else if (g::vr_render_target && State == D3DTS_VIEW) {
            fixedfunction::current_view_matrix = d3d_from_m4(
                g::vr->get_eye_pos(g::vr_render_target.value()) * g::vr->get_pose(g::vr_render_target.value()) * g::flip_z_matrix * rbr::get_horizon_lock_matrix() * m4_from_d3d(*pMatrix));
            return g::hooks::set_transform.call(g::d3d_dev, State, &fixedfunction::current_view_matrix);
        }

        return g::hooks::set_transform.call(g::d3d_dev, State, pMatrix);
    }

    HRESULT __stdcall BTB_SetRenderTarget(IDirect3DDevice9* This, DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget)
    {
        // This was found purely by luck after testing all kinds of things.
        // For some reason, if this call is called with the original This pointer (from RBRRX)
        // plugins switching the render target (i.e. RBRHUD) will cause the stage geometry
        // to not be rendered at all. Routing the call to the D3D device created by openRBRVR,
        // it seems to work correctly.
        return g::d3d_dev->SetRenderTarget(RenderTargetIndex, pRenderTarget);
    }

    HRESULT __stdcall DrawPrimitive(IDirect3DDevice9* This, D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
    {
        if (rbr::is_on_btb_stage()) {
            IDirect3DVertexShader9* shader;
            g::d3d_dev->GetVertexShader(&shader);

            if (shader) {
                // Shader #39 causes strange "shadows" on BTB stages
                // Clearly visible during CFH, and otherwise visible too when looking up
                // Probably some projection matrix issue, but changing the projection matrix like
                // we do normally had no effect, so on BTB stages we just won't draw this primitive with this shader.
                auto should_skip_drawing = shader == g::base_game_shaders[39];
                shader->Release();
                if (should_skip_drawing) {
                    return 0;
                }
            }
        }
        return g::hooks::draw_primitive.call(This, PrimitiveType, StartVertex, PrimitiveCount);
    }

    HRESULT __stdcall DrawIndexedPrimitive(IDirect3DDevice9* This, D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount)
    {
        IDirect3DVertexShader9* shader;
        IDirect3DBaseTexture9* texture;
        g::d3d_dev->GetVertexShader(&shader);
        g::d3d_dev->GetTexture(0, &texture);
        if (g::vr_render_target && !shader && !texture) {
            if (!rbr::is_using_cockpit_camera()) {
                // Don't draw these if we're not in a cockpit camera.
                // In this mode, a black transparent square is drawn in front of the car
                // if car shadows are enabled.
                return 0;
            }
            // Some parts of the car are drawn without a shader and without a texture (just black meshes)
            // We need to render these with the cockpit Z clipping values in order for them to look correct
            const auto projection = fixedfunction::current_projection_matrix;
            const auto cockpit_projection = d3d_from_m4(g::vr->get_projection(g::vr_render_target.value(), Projection::Cockpit));
            g::hooks::set_transform.call(This, D3DTS_PROJECTION, &cockpit_projection);
            auto ret = g::hooks::draw_indexed_primitive.call(g::d3d_dev, PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
            g::hooks::set_transform.call(This, D3DTS_PROJECTION, &fixedfunction::current_projection_matrix);
            return ret;
        } else {
            if (shader)
                shader->Release();
            if (texture)
                texture->Release();
            return g::hooks::draw_indexed_primitive.call(g::d3d_dev, PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
        }
    }

    HRESULT __stdcall CreateDevice(
        IDirect3D9* This,
        UINT Adapter,
        D3DDEVTYPE DeviceType,
        HWND hFocusWindow,
        DWORD BehaviorFlags,
        D3DPRESENT_PARAMETERS* pPresentationParameters,
        IDirect3DDevice9** ppReturnedDeviceInterface)
    {
        IDirect3DDevice9* dev = nullptr;

        pPresentationParameters->PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

        auto ret = g::hooks::create_device.call(This, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, &dev);
        if (FAILED(ret)) {
            dbg("D3D initialization failed: CreateDevice");
            return ret;
        }

        D3DCAPS9 caps;
        dev->GetDeviceCaps(&caps);

        if (static_cast<int32_t>(caps.MaxAnisotropy) < g::cfg.anisotropy) {
            g::cfg.anisotropy = caps.MaxAnisotropy;
        }

        *ppReturnedDeviceInterface = dev;

        dev->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, 1);
        for (auto i = D3DVERTEXTEXTURESAMPLER0; i <= D3DVERTEXTEXTURESAMPLER3; ++i) {
            dev->SetSamplerState(i, D3DSAMP_MAXANISOTROPY, g::cfg.anisotropy);
        }

        auto devvtbl = get_vtable<IDirect3DDevice9Vtbl>(dev);
        try {
            g::hooks::set_vertex_shader_constant_f = Hook(devvtbl->SetVertexShaderConstantF, SetVertexShaderConstantF);
            g::hooks::set_transform = Hook(devvtbl->SetTransform, SetTransform);
            g::hooks::present = Hook(devvtbl->Present, Present);
            g::hooks::create_vertex_shader = Hook(devvtbl->CreateVertexShader, CreateVertexShader);
            g::hooks::draw_indexed_primitive = Hook(devvtbl->DrawIndexedPrimitive, DrawIndexedPrimitive);
            g::hooks::draw_primitive = Hook(devvtbl->DrawPrimitive, DrawPrimitive);
        } catch (const std::runtime_error& e) {
            dbg(e.what());
            MessageBoxA(hFocusWindow, e.what(), "Hooking failed", MB_OK);
        }

        g::d3d_dev = dev;

        try {
            if (g::vr) {
                const auto companion_window_width = pPresentationParameters->BackBufferWidth;
                const auto companion_window_height = pPresentationParameters->BackBufferHeight;

                if (g::vr->get_runtime_type() == OPENXR) {
                    reinterpret_cast<OpenXR*>(g::vr)->init(dev, &g::d3d_vr, companion_window_width, companion_window_height);
                } else {
                    reinterpret_cast<OpenVR*>(g::vr)->init(dev, &g::d3d_vr, companion_window_width, companion_window_height);
                }
            }
        } catch (const std::runtime_error& e) {
            MessageBoxA(hFocusWindow, e.what(), "VR init failed", MB_OK);
        }
        // Initialize this pointer here, as it's too early to do this in openRBRVR constructor
        auto handle = GetModuleHandle("Plugins\\rbr_rx.dll");
        if (handle) {
            auto rx_addr = reinterpret_cast<uintptr_t>(handle);
            g::btb_track_status_ptr = reinterpret_cast<uint8_t*>(rx_addr + rbr_rx::TRACK_STATUS_OFFSET);

            IDirect3DDevice9Vtbl* rbrrxdev = reinterpret_cast<IDirect3DDevice9Vtbl*>(rx_addr + rbr_rx::DEVICE_VTABLE_OFFSET);
            try {
                g::hooks::btb_set_render_target = Hook(rbrrxdev->SetRenderTarget, BTB_SetRenderTarget);
            } catch (const std::runtime_error& e) {
                dbg(e.what());
                MessageBoxA(hFocusWindow, e.what(), "Hooking failed", MB_OK);
            }
        }

        return ret;
    }

    IDirect3D9* __stdcall Direct3DCreate9(UINT SDKVersion)
    {
        if (g::cfg.runtime == OPENXR) {
            // OpenXR must be initialized before calling Direct3DCreate9
            // because it will query extensions when initializing DXVK
            try {
                g::vr = new OpenXR();
            } catch (const std::runtime_error& e) {
                MessageBoxA(nullptr, e.what(), "OpenXR init failed", MB_OK);
            }
        } else {
            // OpenVR must be initialized before creating the d3d device
            // otherwise it cause a crash when the Reshade VK layer is used
            // while SteamVR is already running and vrclient.dll loaded
            try {
                g::vr = new OpenVR();
            } catch (const std::runtime_error& e) {
                MessageBoxA(nullptr, e.what(), "OpenVR init failed", MB_OK);
            }
        }

        auto d3d = g::hooks::create.call(SDKVersion);
        if (!d3d) {
            dbg("Could not initialize Vulkan");
            return nullptr;
        }
        auto d3d_vtbl = get_vtable<IDirect3D9Vtbl>(d3d);
        try {
            g::hooks::create_device = Hook(d3d_vtbl->CreateDevice, CreateDevice);
        } catch (const std::runtime_error& e) {
            dbg(e.what());
            MessageBoxA(nullptr, e.what(), "Hooking failed", MB_OK);
        }
        return d3d;
    }
}
