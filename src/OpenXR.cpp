#define XR_USE_GRAPHICS_API_D3D11

#include "OpenXR.hpp"
#include "Config.hpp"
#include "Globals.hpp"
#include "Util.hpp"
#include <d3d9_interop.h>
#include <gtx/quaternion.hpp>

#include "D3D.hpp"
#include <d3d11.h>
#include <d3d11_1.h>
#include <ranges>

static std::string XrVersionToString(const XrVersion version)
{
    return std::format("{}.{}.{}",
        version >> 48,
        (version >> 32) & 0xFFFF,
        version & 0xFFFFFFFF);
}

template <typename T>
T get_extension(XrInstance instance, const std::string& fnName)
{
    T fn = nullptr;
    if (auto err = xrGetInstanceProcAddr(instance, fnName.c_str(), reinterpret_cast<PFN_xrVoidFunction*>(&fn)); err != XR_SUCCESS) {
        throw std::runtime_error(std::format("Failed to initialize OpenXR: xrGetInstanceProcAddr {}", XrResultToString(instance, err)));
    }
    if (!fn) {
        throw std::runtime_error(std::format("Failed to initialize OpenXR: Could not get extension function {}", fnName));
    }
    return fn;
}

static glm::mat4 create_projection_matrix(const XrFovf& fov, float near_z, float far_z)
{
    // Convert angles from radians to degrees
    const auto tan_left = std::tanf(fov.angleLeft);
    const auto tan_right = std::tanf(fov.angleRight);
    const auto tan_down = std::tanf(fov.angleDown);
    const auto tan_up = std::tanf(fov.angleUp);

    const auto tan_width = tan_right - tan_left;
    const auto tan_height = tan_up - tan_down;

    auto projection = glm::mat4(0.0f);
    projection[0][0] = 2.0f / tan_width;
    projection[1][1] = 2.0f / tan_height;
    projection[2][0] = (tan_right + tan_left) / tan_width;
    projection[2][1] = (tan_up + tan_down) / tan_height;
    projection[2][2] = -(far_z + near_z) / (far_z - near_z);
    projection[2][3] = -1.0f;
    projection[3][2] = -(2.0f * far_z * near_z) / (far_z - near_z);

    return projection;
}

static M4 xr_pose_to_m4(const XrPosef& pose)
{
    const glm::quat orientation(pose.orientation.w, pose.orientation.x, pose.orientation.y, pose.orientation.z);
    const glm::vec3 position(pose.position.x, pose.position.y, pose.position.z);

    const glm::mat4 rotation_matrix = glm::toMat4(orientation);
    const glm::mat4 translation_matrix = glm::translate(glm::mat4(1.0f), position);

    return translation_matrix * rotation_matrix;
}

OpenXR::OpenXR()
    : session()
    , has_projection(false)
    , reset_view_requested(false)
{
    XrApplicationInfo appInfo = {
        .applicationName = "openRBRVR",
        .applicationVersion = 1,
        .engineName = "",
        .engineVersion = 0,
        .apiVersion = XR_CURRENT_API_VERSION,
    };

    auto extensions = std::vector { "XR_KHR_D3D11_enable" };

    // TODO: query available extensions and check if XR_VARJO extensions are available before trying to use them
    if (g::cfg.quad_view_rendering) {
        extensions.push_back("XR_VARJO_quad_views");
        extensions.push_back("XR_VARJO_foveated_rendering");
    }

    XrInstanceCreateInfo instanceInfo = {
        .type = XR_TYPE_INSTANCE_CREATE_INFO,
        .next = nullptr,
        .createFlags = 0,
        .applicationInfo = appInfo,
        .enabledApiLayerCount = 0,
        .enabledApiLayerNames = nullptr,
        .enabledExtensionCount = extensions.size(),
        .enabledExtensionNames = extensions.data(),
    };

    if (auto err = xrCreateInstance(&instanceInfo, &instance); err != XR_SUCCESS) {
        if (err == XR_ERROR_EXTENSION_NOT_PRESENT) {
            throw std::runtime_error("xrCreateInstance failed. If you're running a WMR headset like Reverb, make sure https://github.com/mbucchia/OpenXR-Vk-D3D12 is installed.");
        } else if (err == XR_ERROR_FILE_ACCESS_ERROR) {
            throw std::runtime_error("xrCreateInstance failed. Make sure Visual C++ redistributables (https://aka.ms/vs/17/release/vc_redist.x64.exe) are installed. Please try uninstalling Reshade if it is installed.");
        }
        throw std::runtime_error(std::format("Failed to initialize OpenXR: xrCreateInstance {}", static_cast<int>(err)));
    }

    XrSystemGetInfo system_get_info = {
        .type = XR_TYPE_SYSTEM_GET_INFO,
        .next = nullptr,
        .formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY,
    };
    if (auto err = xrGetSystem(instance, &system_get_info, &system_id); err != XR_SUCCESS) {
        throw std::runtime_error(std::format("Failed to initialize OpenXR: xrGetSystem {}", XrResultToString(instance, err)));
    }
}

void OpenXR::init(IDirect3DDevice9* dev, IDirect3DVR9** vrdev, uint32_t companion_window_width, uint32_t companion_window_height)
{
    Direct3DCreateVR(g::d3d_dev, &g::d3d_vr);

    if (dev->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &gpu_start_query) != D3D_OK) {
        throw std::runtime_error("VR initialization failed: CreateQuery");
    }
    if (dev->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &gpu_end_query) != D3D_OK) {
        throw std::runtime_error("VR initialization failed: CreateQuery");
    }
    if (dev->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT, &gpu_disjoint_query) != D3D_OK) {
        throw std::runtime_error("VR initialization failed: CreateQuery");
    }
    if (dev->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, &gpu_freq_query) != D3D_OK) {
        throw std::runtime_error("VR initialization failed: CreateQuery");
    }

    XrGraphicsRequirementsD3D11KHR gfx_requirements = { .type = XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
    auto xrGetD3D11GraphicsRequirementsKHR = get_extension<PFN_xrGetD3D11GraphicsRequirementsKHR>(instance, "xrGetD3D11GraphicsRequirementsKHR");
    if (auto err = xrGetD3D11GraphicsRequirementsKHR(instance, system_id, &gfx_requirements); err != XR_SUCCESS) {
        throw std::runtime_error(std::format("Failed to initialize OpenXR: xrGetVulkanGraphicsRequirementsKHR {}", XrResultToString(instance, err)));
    }

    dbg(std::format("OpenXR D3D11 feature level {:x}", (int)gfx_requirements.minFeatureLevel));

    IDXGIFactory1* dxgi_factory;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgi_factory)))) {
        throw std::runtime_error("Failed to create DXGI factory");
    }
    IDXGIAdapter1* dxgi_adapter = nullptr;
    for (UINT adapterIndex = 0;; adapterIndex++) {
        // EnumAdapters1 will fail with DXGI_ERROR_NOT_FOUND when there are no more adapters to
        // enumerate.
        if (FAILED(dxgi_factory->EnumAdapters1(adapterIndex, &dxgi_adapter))) {
            throw std::runtime_error("Failed to create enumerate adapters");
        }
        DXGI_ADAPTER_DESC1 adapter_desc;
        if (FAILED(dxgi_adapter->GetDesc1(&adapter_desc))) {
            throw std::runtime_error("Failed to get adapter description");
        }
        if (!memcmp(&adapter_desc.AdapterLuid, &gfx_requirements.adapterLuid, sizeof(LUID))) {
            const auto adapter_description = std::wstring(adapter_desc.Description)
                | std::views::transform([](wchar_t c) { return static_cast<char>(c); })
                | std::ranges::to<std::string>();

            dbg(std::format("Using D3D11 adapter {}", adapter_description));
            break;
        }
    }
    if (!dxgi_adapter) {
        throw std::runtime_error("No adapter found");
    }

    ID3D11Device* d3d11dev;
    ID3D11DeviceContext* d3d11ctx;

    if (auto ret = D3D11CreateDevice(dxgi_adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, &gfx_requirements.minFeatureLevel, 1, D3D11_SDK_VERSION, &d3d11dev, nullptr, &d3d11ctx); ret != D3D_OK) {
        throw std::runtime_error(std::format("Failed to create D3D11 device: {}", ret));
    }

    dxgi_adapter->Release();
    dxgi_factory->Release();

    d3d11dev->QueryInterface(__uuidof(ID3D11Device5), reinterpret_cast<void**>(&g::d3d11_dev));
    d3d11ctx->QueryInterface(__uuidof(ID3D11DeviceContext4), reinterpret_cast<void**>(&g::d3d11_ctx));

    d3d11ctx->Release();
    d3d11dev->Release();

    XrGraphicsBindingD3D11KHR gfx_binding = {
        .type = XR_TYPE_GRAPHICS_BINDING_D3D11_KHR,
        .next = nullptr,
        .device = g::d3d11_dev,
    };

    XrSessionCreateInfo sessionCreateInfo = {
        .type = XR_TYPE_SESSION_CREATE_INFO,
        .next = &gfx_binding,
        .createFlags = 0,
        .systemId = system_id,
    };

    if (auto err = xrCreateSession(instance, &sessionCreateInfo, &session); err != XR_SUCCESS) {
        throw std::runtime_error(std::format("Failed to initialize OpenXR: xrCreateSession {}", XrResultToString(instance, err)));
    }

    XrViewConfigurationType view_config_types[] = { XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_QUAD_VARJO };
    uint32_t view_config_count;
    if (auto err = xrEnumerateViewConfigurations(instance, system_id, 0, &view_config_count, view_config_types); err != XR_SUCCESS) {
        throw std::runtime_error("Failed to enumerate view configurations");
    }
    std::vector<XrViewConfigurationType> view_configs(view_config_count);
    if (auto err = xrEnumerateViewConfigurations(instance, system_id, view_config_count, &view_config_count, view_configs.data()); err != XR_SUCCESS) {
        throw std::runtime_error("Failed to enumerate view configurations");
    }

    // Store view config type for later use
    primary_view_config_type = view_configs.front();

    uint32_t view_count;
    if (auto err = xrEnumerateViewConfigurationViews(instance, system_id, primary_view_config_type, 0, &view_count, nullptr); err != XR_SUCCESS) {
        throw std::runtime_error("Failed to enumerate view config views");
    }

    std::vector<XrViewConfigurationView> view_config_views(view_count, { .type = XR_TYPE_VIEW_CONFIGURATION_VIEW });
    if (auto err = xrEnumerateViewConfigurationViews(instance, system_id, primary_view_config_type, view_count, &view_count, view_config_views.data()); err != XR_SUCCESS) {
        throw std::runtime_error("Failed to enumerate view config views");
    }

    uint32_t format_count;
    if (auto err = xrEnumerateSwapchainFormats(session, 0, &format_count, nullptr); err != XR_SUCCESS) {
        throw std::runtime_error("Failed to enumerate swapchain formats");
    }
    std::vector<int64_t> swapchain_formats(format_count, 0);
    if (auto err = xrEnumerateSwapchainFormats(session, format_count, &format_count, swapchain_formats.data()); err != XR_SUCCESS) {
        throw std::runtime_error("Failed to enumerate swapchain formats");
    }

    std::stringstream ss;
    ss << "OpenXR swapchainFormats:";
    for (auto fmt : swapchain_formats) {
        ss << " " << fmt;
    }
    dbg(ss.str());

    if (std::find(swapchain_formats.begin(), swapchain_formats.end(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) != swapchain_formats.end()) {
        swapchain_format = static_cast<int64_t>(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    } else {
        swapchain_format = swapchain_formats.front();
    }

    // Create shared fence for D3D11/Vulkan synchronization
    cross_api_fence.value = 0;
    if (auto ret = g::d3d11_dev->CreateFence(cross_api_fence.value, D3D11_FENCE_FLAG_SHARED, __uuidof(ID3D11Fence), reinterpret_cast<void**>(&cross_api_fence.fence)); ret != D3D_OK) {
        throw std::runtime_error(std::format("Failed to create fence: {}", ret));
    }
    if (auto ret = cross_api_fence.fence->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &cross_api_fence.shared_handle); ret != D3D_OK) {
        throw std::runtime_error(std::format("Failed to create shared handle: {}", ret));
    }
    g::d3d_vr->ImportFence(cross_api_fence.shared_handle, cross_api_fence.value);

    for (const auto& gfx : g::cfg.gfx) {
        auto supersampling = gfx.second.supersampling;

        OpenXRRenderContext* xr_ctx = new OpenXRRenderContext(view_config_views.size());
        RenderContext ctx = {
            .dx_shared_handle = { 0 },
            .msaa = gfx.second.msaa.value_or(g::cfg.gfx["default"].msaa.value()),
            .ext = xr_ctx
        };

        for (size_t i = 0; i < view_config_views.size(); ++i) {
            ctx.width[i] = static_cast<uint32_t>(view_config_views[i].recommendedImageRectWidth * supersampling);
            ctx.height[i] = static_cast<uint32_t>(view_config_views[i].recommendedImageRectHeight * supersampling);

            XrSwapchainCreateInfo swapchain_create_info = {
                .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
                .createFlags = 0,
                .usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
                .format = swapchain_format,
                .sampleCount = 1,
                .width = ctx.width[i],
                .height = ctx.height[i],
                .faceCount = 1,
                .arraySize = 1,
                .mipCount = 1,
            };
            dbg(std::format("requesting swapchain: {}x{}, format {}", swapchain_create_info.width, swapchain_create_info.height, swapchain_create_info.format));

            if (auto err = xrCreateSwapchain(session, &swapchain_create_info, &xr_ctx->swapchains[i]); err != XR_SUCCESS) {
                throw std::runtime_error(std::format("VR init failed. xrCreateSwapchain: {}", XrResultToString(instance, err)));
            }

            uint32_t imageCount;
            if (auto err = xrEnumerateSwapchainImages(xr_ctx->swapchains[i], 0, &imageCount, nullptr); err != XR_SUCCESS) {
                throw std::runtime_error(std::format("Failed to initialize OpenXR: xrEnumerateSwapchainImages {}", XrResultToString(instance, err)));
            }

            xr_ctx->swapchain_images[i].resize(imageCount, { .type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR });
            if (auto err = xrEnumerateSwapchainImages(
                    xr_ctx->swapchains[i],
                    xr_ctx->swapchain_images[i].size(),
                    &imageCount,
                    reinterpret_cast<XrSwapchainImageBaseHeader*>(xr_ctx->swapchain_images[i].data()));
                err != XR_SUCCESS) {
                throw std::runtime_error(std::format("Failed to initialize OpenXR: xrEnumerateSwapchainImages {}", XrResultToString(instance, err)));
            }

            D3D11_TEXTURE2D_DESC desc = {
                .Width = ctx.width[i],
                .Height = ctx.height[i],
                .MipLevels = 1,
                .ArraySize = 1,
                .Format = static_cast<DXGI_FORMAT>(swapchain_format),
                .SampleDesc = 1,
                .Usage = D3D11_USAGE_DEFAULT,
                .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
                .CPUAccessFlags = 0,
                .MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE,
            };

            if (auto ret = d3d11dev->CreateTexture2D(&desc, nullptr, &xr_ctx->shared_textures[i]); ret != D3D_OK) {
                throw std::runtime_error(std::format("Failed to create shared texture: {}", ret));
            }

            IDXGIResource1* dxgi_res = nullptr;
            if (auto ret = xr_ctx->shared_textures[i]->QueryInterface(__uuidof(IDXGIResource1), (void**)&dxgi_res); ret != D3D_OK) {
                throw std::runtime_error(std::format("Failed to query interface IDXGIResource1: {}", ret));
            }

            if (auto ret = dxgi_res->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &ctx.dx_shared_handle[i]); ret != D3D_OK) {
                throw std::runtime_error(std::format("Failed to create shared handle: {}", ret));
            }

            dxgi_res->Release();
        }

        init_surfaces(dev, ctx, companion_window_width, companion_window_height);

        for (size_t i = 0; i < xr_ctx->views.size(); ++i) {
            xr_ctx->views[i] = { .type = XR_TYPE_VIEW };
            xr_ctx->projection_views[i] = {
                .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
                .next = nullptr,
                .subImage = {
                    .swapchain = xr_ctx->swapchains[i],
                    .imageRect = {
                        .offset = { 0, 0 },
                        .extent = {
                            .width = static_cast<int>(ctx.width[i]),
                            .height = static_cast<int>(ctx.height[i]),
                        },
                    },
                    .imageArrayIndex = 0,
                }
            };
        }

        render_contexts[gfx.first] = ctx;
    }

    set_render_context("default");

    view_pose = { { 0, 0, 0, 1 }, { 0, 0, 0 } };

    XrReferenceSpaceCreateInfo view_space_create_info = {
        .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
        .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW,
        .poseInReferenceSpace = view_pose,
    };

    if (auto res = xrCreateReferenceSpace(session, &view_space_create_info, &view_space); res != XR_SUCCESS) {
        dbg(std::format("Failed to recenter VR view: {}", XrResultToString(instance, res)));
        return;
    }

    XrReferenceSpaceCreateInfo space_create_info = {
        .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
        .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL,
        .poseInReferenceSpace = view_pose,
    };
    if (auto err = xrCreateReferenceSpace(session, &space_create_info, &space); err != XR_SUCCESS) {
        throw std::runtime_error(std::format("Failed to initialize OpenXR. xrCreateReferenceSpace {}", XrResultToString(instance, err)));
    }
    bool session_running = false;
    auto retries = 10;
    while (retries-- > 0) {
        XrEventDataBuffer eventData = {
            .type = XR_TYPE_EVENT_DATA_BUFFER,
            .next = nullptr,
        };
        if (auto res = xrPollEvent(instance, &eventData); res == XR_SUCCESS) {
            if (eventData.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                const XrEventDataSessionStateChanged* sessionStateChangedEvent = reinterpret_cast<const XrEventDataSessionStateChanged*>(&eventData);

                // Check for the XR_SESSION_STATE_READY state
                if (sessionStateChangedEvent->state == XR_SESSION_STATE_READY) {
                    XrSessionBeginInfo sessionBeginInfo = {
                        .type = XR_TYPE_SESSION_BEGIN_INFO,
                        .next = nullptr,
                        .primaryViewConfigurationType = primary_view_config_type,
                    };
                    if (auto res = xrBeginSession(session, &sessionBeginInfo); res != XR_SUCCESS) {
                        throw std::runtime_error(std::format("Failed to initialize OpenXR. xrBeginSession: {}", XrResultToString(instance, res)));
                    }

                    session_running = true;
                    break;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (!session_running) {
        dbg("Did not receive XR_SESSION_STATE_READY event, launching the session anyway...");
        XrSessionBeginInfo sessionBeginInfo = {
            .type = XR_TYPE_SESSION_BEGIN_INFO,
            .next = nullptr,
            .primaryViewConfigurationType = primary_view_config_type,
        };
        if (auto res = xrBeginSession(session, &sessionBeginInfo); res != XR_SUCCESS) {
            throw std::runtime_error(std::format("Failed to initialize OpenXR. xrBeginSession: {}", XrResultToString(instance, res)));
        }
    }

    // In OpenXR we don't have separate matrices for eye positions
    // The eye position is taken account in the projection matrix already
    eye_pos[LeftEye] = glm::identity<glm::mat4x4>();
    eye_pos[RightEye] = glm::identity<glm::mat4x4>();
    eye_pos[FocusLeft] = glm::identity<glm::mat4x4>();
    eye_pos[FocusRight] = glm::identity<glm::mat4x4>();
}

const char* OpenXR::get_device_extensions()
{
    if (device_extensions.empty()) {
        uint32_t extension_list_len;
        auto xrGetVulkanDeviceExtensionsKHR = get_extension<PFN_xrGetVulkanDeviceExtensionsKHR>(instance, "xrGetVulkanDeviceExtensionsKHR");
        if (auto err = xrGetVulkanDeviceExtensionsKHR(instance, system_id, 0, &extension_list_len, nullptr); err != XR_SUCCESS) {
            throw std::runtime_error(std::format("OpenXR init failed: xrGetVulkanDeviceExtensionsKHR: {}", XrResultToString(instance, err)));
        }
        device_extensions.resize(extension_list_len, 0);
        if (auto err = xrGetVulkanDeviceExtensionsKHR(instance, system_id, device_extensions.size(), &extension_list_len, device_extensions.data()); err != XR_SUCCESS) {
            throw std::runtime_error(std::format("OpenXR init failed: xrGetVulkanDeviceExtensionsKHR: {}", XrResultToString(instance, err)));
        }
    }
    return device_extensions.data();
}

const char* OpenXR::get_instance_extensions()
{
    if (instance_extensions.empty()) {
        uint32_t extension_list_len;
        auto xrGetVulkanInstanceExtensionsKHR = get_extension<PFN_xrGetVulkanInstanceExtensionsKHR>(instance, "xrGetVulkanInstanceExtensionsKHR");
        if (auto err = xrGetVulkanInstanceExtensionsKHR(instance, system_id, 0, &extension_list_len, nullptr); err != XR_SUCCESS) {
            throw std::runtime_error(std::format("OpenXR init failed: xrGetVulkanInstanceExtensionsKHR: {}", XrResultToString(instance, err)));
        }
        instance_extensions.resize(extension_list_len, 0);
        if (auto err = xrGetVulkanInstanceExtensionsKHR(instance, system_id, instance_extensions.size(), &extension_list_len, instance_extensions.data()); err != XR_SUCCESS) {
            throw std::runtime_error(std::format("OpenXR init failed: xrGetVulkanInstanceExtensionsKHR: {}", XrResultToString(instance, err)));
        }
    }
    return instance_extensions.data();
}

XrSwapchainImageD3D11KHR& OpenXR::acquire_swapchain_image(RenderTarget tgt)
{
    XrSwapchainImageAcquireInfo acquire_info = {
        .type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
        .next = nullptr,
    };

    uint32_t idx;
    if (auto err = xrAcquireSwapchainImage(xr_context()->swapchains[tgt], &acquire_info, &idx); err != XR_SUCCESS) {
        dbg(std::format("Could not acquire swapchain image: {}", XrResultToString(instance, err)));
    }

    return xr_context()->swapchain_images[tgt][idx];
}

void OpenXR::prepare_frames_for_hmd(IDirect3DDevice9* dev)
{
    if (current_render_context->msaa != D3DMULTISAMPLE_NONE) {
        // Resolve multisampling
        IDirect3DSurface9 *left_eye, *right_eye, *focus_left, *focus_right;

        if (current_render_context->dx_texture[LeftEye]->GetSurfaceLevel(0, &left_eye) != D3D_OK) {
            dbg("Failed to get left eye surface");
            return;
        }
        if (current_render_context->dx_texture[RightEye]->GetSurfaceLevel(0, &right_eye) != D3D_OK) {
            dbg("Failed to get right eye surface");
            left_eye->Release();
            return;
        }

        dev->StretchRect(current_render_context->dx_surface[LeftEye], nullptr, left_eye, nullptr, D3DTEXF_NONE);
        dev->StretchRect(current_render_context->dx_surface[RightEye], nullptr, right_eye, nullptr, D3DTEXF_NONE);

        left_eye->Release();
        right_eye->Release();

        if (g::cfg.quad_view_rendering) {
            if (current_render_context->dx_texture[FocusLeft]->GetSurfaceLevel(0, &focus_left) != D3D_OK) {
                dbg("Failed to get focus left eye surface");
                return;
            }
            if (current_render_context->dx_texture[FocusRight]->GetSurfaceLevel(0, &focus_right) != D3D_OK) {
                dbg("Failed to get focus right eye surface");
                focus_left->Release();
                return;
            }
            dev->StretchRect(current_render_context->dx_surface[FocusLeft], nullptr, focus_left, nullptr, D3DTEXF_NONE);
            dev->StretchRect(current_render_context->dx_surface[FocusRight], nullptr, focus_right, nullptr, D3DTEXF_NONE);
            focus_left->Release();
            focus_right->Release();
        }
    }

    auto& left = acquire_swapchain_image(LeftEye);
    auto& right = acquire_swapchain_image(RightEye);

    XrSwapchainImageD3D11KHR focus_left;
    XrSwapchainImageD3D11KHR focus_right;
    if (g::cfg.quad_view_rendering) {
        focus_left = acquire_swapchain_image(FocusLeft);
        focus_right = acquire_swapchain_image(FocusRight);
    }

    XrSwapchainImageWaitInfo info = {
        .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
        .next = nullptr,
        .timeout = XR_INFINITE_DURATION,
    };
    XrSwapchainImageReleaseInfo release_info = {
        .type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
        .next = nullptr,
    };

    if (auto res = xrWaitSwapchainImage(xr_context()->swapchains[LeftEye], &info); res != XR_SUCCESS) {
        dbg(std::format("xrWaitSwapchainImage (left): {}", XrResultToString(instance, res)));
        return;
    }
    if (auto res = xrWaitSwapchainImage(xr_context()->swapchains[RightEye], &info); res != XR_SUCCESS) {
        dbg(std::format("xrWaitSwapchainImage (right): {}", XrResultToString(instance, res)));
        xrReleaseSwapchainImage(xr_context()->swapchains[LeftEye], &release_info);
        return;
    }

    if (g::cfg.quad_view_rendering) {
        if (auto res = xrWaitSwapchainImage(xr_context()->swapchains[FocusLeft], &info); res != XR_SUCCESS) {
            dbg(std::format("xrWaitSwapchainImage (focus left): {}", XrResultToString(instance, res)));
            return;
        }
        if (auto res = xrWaitSwapchainImage(xr_context()->swapchains[FocusRight], &info); res != XR_SUCCESS) {
            dbg(std::format("xrWaitSwapchainImage (focus right): {}", XrResultToString(instance, res)));
            return;
        }
    }

    synchronize_graphics_apis();

    // Copy shared textures to OpenXR textures
    g::d3d11_ctx->CopyResource(left.texture, xr_context()->shared_textures[LeftEye]);
    g::d3d11_ctx->CopyResource(right.texture, xr_context()->shared_textures[RightEye]);
    if (g::cfg.quad_view_rendering) {
        g::d3d11_ctx->CopyResource(focus_left.texture, xr_context()->shared_textures[FocusLeft]);
        g::d3d11_ctx->CopyResource(focus_right.texture, xr_context()->shared_textures[FocusRight]);
        xrReleaseSwapchainImage(xr_context()->swapchains[FocusLeft], &release_info);
        xrReleaseSwapchainImage(xr_context()->swapchains[FocusRight], &release_info);
    }

    xrReleaseSwapchainImage(xr_context()->swapchains[LeftEye], &release_info);
    xrReleaseSwapchainImage(xr_context()->swapchains[RightEye], &release_info);

    if (g::cfg.debug && perf_query_free_to_use) [[unlikely]] {
        gpu_end_query->Issue(D3DISSUE_END);
        gpu_disjoint_query->Issue(D3DISSUE_END);
    }
}

void OpenXR::synchronize_graphics_apis()
{
    cross_api_fence.value += 1;

    g::d3d_vr->Flush();
    g::d3d_vr->LockSubmissionQueue();
    g::d3d_vr->SignalFence(cross_api_fence.value);
    g::d3d_vr->UnlockSubmissionQueue();

    if (auto ret = g::d3d11_ctx->Wait(cross_api_fence.fence, cross_api_fence.value); ret != D3D_OK) {
        dbg(std::format("Failed to wait shared semaphore: {}", ret));
    }
}

void OpenXR::submit_frames_to_hmd(IDirect3DDevice9* dev)
{
    xr_context()->projection_views[LeftEye].fov = xr_context()->views[LeftEye].fov;
    xr_context()->projection_views[LeftEye].pose = xr_context()->views[LeftEye].pose;
    xr_context()->projection_views[RightEye].fov = xr_context()->views[RightEye].fov;
    xr_context()->projection_views[RightEye].pose = xr_context()->views[RightEye].pose;
    if (g::cfg.quad_view_rendering) {
        xr_context()->projection_views[FocusLeft].fov = xr_context()->views[FocusLeft].fov;
        xr_context()->projection_views[FocusLeft].pose = xr_context()->views[FocusLeft].pose;
        xr_context()->projection_views[FocusRight].fov = xr_context()->views[FocusRight].fov;
        xr_context()->projection_views[FocusRight].pose = xr_context()->views[FocusRight].pose;
    }

    XrCompositionLayerProjection projection_layer = {
        .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
        .next = nullptr,
        .layerFlags = 0,
        .space = space,
        .viewCount = xr_context()->projection_views.size(),
        .views = xr_context()->projection_views.data(),
    };

    XrCompositionLayerBaseHeader* layers[] = {
        reinterpret_cast<XrCompositionLayerBaseHeader*>(&projection_layer),
    };

    XrFrameEndInfo frame_end_info = {
        .type = XR_TYPE_FRAME_END_INFO,
        .displayTime = frame_state.predictedDisplayTime,
        .environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
        .layerCount = 1,
        .layers = layers,
    };

    if (auto res = xrEndFrame(session, &frame_end_info); res != XR_SUCCESS) {
        dbg(std::format("xrEndFrame failed: {}", XrResultToString(instance, res)));
    }
}

std::optional<XrViewState> OpenXR::update_views()
{
    XrViewState view_state = {
        .type = XR_TYPE_VIEW_STATE,
        .next = nullptr,
        .viewStateFlags = 0,
    };
    const XrViewLocateInfo view_locate_info = {
        .type = XR_TYPE_VIEW_LOCATE_INFO,
        .next = nullptr,
        .viewConfigurationType = primary_view_config_type,
        .displayTime = frame_state.predictedDisplayTime,
        .space = space,
    };

    uint32_t view_count = 0;
    if (auto err = xrLocateViews(session, &view_locate_info, &view_state, xr_context()->views.size(), &view_count, xr_context()->views.data()); err != XR_SUCCESS) {
        dbg(std::format("xrLocateViews failed: {}", XrResultToString(instance, err)));
        return std::nullopt;
    }
    if (view_count != xr_context()->views.size()) {
        dbg(std::format("Unexpected viewcount: {}", view_count));
        return std::nullopt;
    }

    auto& views = xr_context()->views;

    // Adjust "World scale" if needed
    if (g::cfg.world_scale != 1000) {
        for (auto i = 0; i < xr_context()->views.size(); i += 2) {
            // Ported from:
            // https://github.com/mbucchia/OpenXR-Toolkit/blob/main/XR_APILAYER_MBUCCHIA_toolkit/layer.cpp#L1775-L1782
            // MIT License
            // Copyright (c) 2021-2022 Matthieu Bucchianeri
            // Copyright (c) 2021-2022 Jean-Luc Dupiot - Reality XP

            auto& pos0xr = views[i].pose.position;
            auto& pos1xr = views[i + 1].pose.position;
            const auto pos0 = glm::vec3(pos0xr.x, pos0xr.y, pos0xr.z);
            const auto pos1 = glm::vec3(pos1xr.x, pos1xr.y, pos1xr.z);
            const auto vec = pos1 - pos0;
            const auto ipd = glm::length(vec);
            const float newIpd = (ipd * 1000) / g::cfg.world_scale;
            const auto center = pos0 + (vec * 0.5f);
            const auto offset = glm::normalize(vec) * (newIpd * 0.5f);

            const auto left = center - offset;
            const auto right = center + offset;

            views[i].pose.position = XrVector3f { left.x, left.y, left.z };
            views[i + 1].pose.position = XrVector3f { right.x, right.y, right.z };
        }
    }

    return view_state;
}

bool OpenXR::update_vr_poses()
{
    frame_state = {
        .type = XR_TYPE_FRAME_STATE,
        .next = nullptr,
    };

    if (auto res = xrWaitFrame(session, nullptr, &frame_state); res != XR_SUCCESS) {
        dbg(std::format("xrWaitFrame: {}", XrResultToString(instance, res)));
        return false;
    }

    if (frame_state.shouldRender == XR_FALSE) {
        // Oculus runtime has a bug where shouldRender is always false... o_o
        // So we do nothing about it.
    }

    if (reset_view_requested) {
        reset_view_requested = false;
        recenter_view();
    }

    if (auto res = xrBeginFrame(session, nullptr); res != XR_SUCCESS) {
        dbg(std::format("xrBeginFrame: {}", XrResultToString(instance, res)));
    }

    if (!has_projection) {
        // TODO: Should this be called every frame or is it okay to do
        // like OpenVR does and just fetch the projection matrices once?
        has_projection = get_projection_matrix();
    }

    update_poses();

    if (g::cfg.debug && perf_query_free_to_use) [[unlikely]] {
        gpu_disjoint_query->Issue(D3DISSUE_BEGIN);
        gpu_start_query->Issue(D3DISSUE_END);
    }

    return true;
}

bool OpenXR::get_projection_matrix()
{
    auto view_state = update_views();
    if (!view_state) {
        dbg("Failed to update OpenXR views");
        return false;
    }
    const auto view_count = xr_context()->views.size();
    for (auto i = 0; i < view_count; ++i) {
        stage_projection[i] = create_projection_matrix(xr_context()->views[i].fov, zNearStage, zFar);
        cockpit_projection[i] = create_projection_matrix(xr_context()->views[i].fov, zNearCockpit, zFar);
        mainmenu_projection[i] = create_projection_matrix(xr_context()->views[i].fov, zNearMainMenu, zFar);
    }
    return true;
}

void OpenXR::update_poses()
{
    auto viewState = update_views();
    if (!viewState) {
        dbg("Failed to update OpenXR views");
        return;
    }

    auto& vs = viewState.value();
    const auto view_count = xr_context()->views.size();
    for (size_t i = 0; i < view_count; ++i) {
        if (vs.viewStateFlags & (XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT)) {
            hmd_pose[i] = glm::inverse(xr_pose_to_m4(xr_context()->views[i].pose));
        } else {
            dbg("Invalid VR poses");
            return;
        }
    }
}

FrameTimingInfo OpenXR::get_frame_timing()
{
    FrameTimingInfo ret = { 0 };

    BOOL disjoint;
    uint64_t gpu_start, gpu_end;
    if (gpu_disjoint_query->GetData(&disjoint, sizeof(disjoint), 0) == D3D_OK && !disjoint) {
        gpu_start_query->GetData(&gpu_start, sizeof(gpu_start), 0);
        gpu_end_query->GetData(&gpu_end, sizeof(gpu_end), 0);

        uint64_t freq;
        gpu_freq_query->Issue(D3DISSUE_END);
        gpu_freq_query->GetData(&freq, sizeof(freq), 0);

        ret.gpu_total = float(gpu_end - gpu_start) / float(freq);

        perf_query_free_to_use = true;
    } else {
        perf_query_free_to_use = false;
    }

    return ret;
}

void OpenXR::reset_view()
{
    reset_view_requested = true;
}

void OpenXR::recenter_view()
{
    XrSpaceLocation space_location = {
        .type = XR_TYPE_SPACE_LOCATION,
        .next = nullptr,
    };
    if (auto res = xrLocateSpace(view_space, space, frame_state.predictedDisplayTime, &space_location); res != XR_SUCCESS) {
        if (auto res = xrDestroySpace(view_space); res != XR_SUCCESS) {
            dbg(std::format("Failed to destroy old viewSpace: {}", XrResultToString(instance, res)));
        }
        dbg(std::format("Failed to recenter VR view: {}", XrResultToString(instance, res)));
        return;
    }

    constexpr auto validation_bits = std::to_array({
        XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT,
        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT,
        XR_SPACE_LOCATION_POSITION_TRACKED_BIT,
        XR_SPACE_LOCATION_POSITION_VALID_BIT,
    });

    for (auto bit : validation_bits) {
        if ((space_location.locationFlags & bit) == 0) {
            dbg("SpaceLocation invalid");
            return;
        }
    }

    const auto& vpo = view_pose.orientation;
    const auto& slo = space_location.pose.orientation;
    const auto& spo = space_location.pose.position;
    const auto spq = glm::quat(slo.w, 0, slo.y, 0);
    const auto vpq = glm::quat(vpo.w, 0, vpo.y, 0);
    const auto vp = glm::vec3 { view_pose.position.x, view_pose.position.y, view_pose.position.z };
    const auto pos = glm::rotate(glm::inverse(vpq), vp) + glm::vec3 { spo.x, spo.y, spo.z };
    const auto newOrientation = glm::normalize(vpq * spq);
    const auto finalPos = glm::rotate(newOrientation, pos);

    XrPosef pose = {
        .orientation = {
            .x = newOrientation.x,
            .y = newOrientation.y,
            .z = newOrientation.z,
            .w = newOrientation.w,
        },
        .position = {
            .x = finalPos.x,
            .y = finalPos.y,
            .z = finalPos.z,
        }
    };

    XrSpace new_space;
    XrReferenceSpaceCreateInfo space_create_info = {
        .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
        .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL,
        .poseInReferenceSpace = pose,
    };
    if (auto res = xrCreateReferenceSpace(session, &space_create_info, &new_space); res != XR_SUCCESS) {
        dbg(std::format("Failed to recenter VR view: {}", XrResultToString(instance, res)));
        return;
    }

    if (auto res = xrDestroySpace(space); res != XR_SUCCESS) {
        dbg(std::format("Failed to destroy old space: {}", XrResultToString(instance, res)));
        return;
    }

    space = new_space;
    view_pose = pose;
}

void OpenXR::shutdown_vr()
{
    xrDestroySpace(space);
    for (const auto& v : render_contexts) {
        const auto& ctx = v.second;
        auto xr_ctx = reinterpret_cast<OpenXRRenderContext*>(ctx.ext);
        xrDestroySwapchain(xr_ctx->swapchains[LeftEye]);
        xrDestroySwapchain(xr_ctx->swapchains[RightEye]);
        if (g::cfg.quad_view_rendering) {
            xrDestroySwapchain(xr_ctx->swapchains[FocusLeft]);
            xrDestroySwapchain(xr_ctx->swapchains[FocusRight]);
        }
    }
    xrDestroySession(session);
    xrDestroyInstance(instance);
}
