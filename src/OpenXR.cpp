#define XR_USE_GRAPHICS_API_VULKAN
#include "OpenXR.hpp"
#include "Config.hpp"
#include "Globals.hpp"
#include "Util.hpp"
#include <d3d9_interop.h>
#include <gtx/quaternion.hpp>

#include "D3D.hpp"

static std::string XrResultToString(const XrInstance& instance, XrResult res)
{
    char buf[XR_MAX_RESULT_STRING_SIZE] = { 0 };

    if (auto err = xrResultToString(instance, res, buf); err != XR_SUCCESS) {
        dbg("Could not convert XrResult to string");
        return "";
    }

    return buf;
}

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
    const char* extensions[] = {
        "XR_KHR_vulkan_enable"
    };
    XrInstanceCreateInfo instanceInfo = {
        .type = XR_TYPE_INSTANCE_CREATE_INFO,
        .next = nullptr,
        .createFlags = 0,
        .applicationInfo = appInfo,
        .enabledApiLayerCount = 0,
        .enabledApiLayerNames = nullptr,
        .enabledExtensionCount = 1,
        .enabledExtensionNames = extensions,
    };

    if (auto err = xrCreateInstance(&instanceInfo, &instance); err != XR_SUCCESS) {
        if (err == XR_ERROR_EXTENSION_NOT_PRESENT) {
            throw std::runtime_error("xrCreateInstance failed. If you're running a WMR headset like Reverb, make sure https://github.com/mbucchia/OpenXR-Vk-D3D12 is installed.");
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

void OpenXR::init(IDirect3DDevice9* dev, const Config& cfg, IDirect3DVR9** vrdev, uint32_t companion_window_width, uint32_t companion_window_height)
{
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

    Direct3DCreateVR(dev, vrdev);
    if (!vrdev) {
        throw std::runtime_error("VR initialization failed: Direct3DCreateVR");
    }

    OXR_VK_DEVICE_DESC vkDesc;
    if ((*vrdev)->GetOXRVkDeviceDesc(&vkDesc) != D3D_OK) {
        throw std::runtime_error("VR initialization failed: GetOXRVkDeviceDesc");
    }

    VkPhysicalDevice phys;
    auto xrGetVulkanGraphicsDeviceKHR = get_extension<PFN_xrGetVulkanGraphicsDeviceKHR>(instance, "xrGetVulkanGraphicsDeviceKHR");
    // This needs to be called even though we get back the same pointer that we had already in OXR_VK_DEVICE_DESC
    xrGetVulkanGraphicsDeviceKHR(instance, system_id, vkDesc.Instance, &phys);

    XrGraphicsBindingVulkanKHR graphicsBinding = {
        .type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR,
        .next = nullptr,
        .instance = vkDesc.Instance,
        .physicalDevice = vkDesc.PhysicalDevice,
        .device = vkDesc.Device,
        .queueFamilyIndex = vkDesc.QueueFamilyIndex,
        .queueIndex = vkDesc.QueueIndex,
    };
    XrSessionCreateInfo sessionCreateInfo = {
        .type = XR_TYPE_SESSION_CREATE_INFO,
        .next = &graphicsBinding,
        .createFlags = 0,
        .systemId = system_id,
    };

    XrGraphicsRequirementsVulkanKHR gfx_requirements = { .type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR };
    auto xrGetVulkanGraphicsRequirementsKHR = get_extension<PFN_xrGetVulkanGraphicsRequirementsKHR>(instance, "xrGetVulkanGraphicsRequirementsKHR");
    if (auto err = xrGetVulkanGraphicsRequirementsKHR(instance, system_id, &gfx_requirements); err != XR_SUCCESS) {
        throw std::runtime_error(std::format("Failed to initialize OpenXR: xrGetVulkanGraphicsRequirementsKHR {}", XrResultToString(instance, err)));
    }

    if (auto err = xrCreateSession(instance, &sessionCreateInfo, &session); err != XR_SUCCESS) {
        throw std::runtime_error(std::format("Failed to initialize OpenXR: xrCreateSession {}", XrResultToString(instance, err)));
    }

    XrViewConfigurationType view_config_type = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    uint32_t view_config_count;
    if (auto err = xrEnumerateViewConfigurations(instance, system_id, 0, &view_config_count, &view_config_type); err != XR_SUCCESS) {
        throw std::runtime_error("Failed to enumerate view configurations");
    }
    std::vector<XrViewConfigurationType> view_configs(view_config_count);
    if (auto err = xrEnumerateViewConfigurations(instance, system_id, view_config_count, &view_config_count, view_configs.data()); err != XR_SUCCESS) {
        throw std::runtime_error("Failed to enumerate view configurations");
    }

    auto view_config = view_configs.front();
    uint32_t view_count;
    if (auto err = xrEnumerateViewConfigurationViews(instance, system_id, view_config, 0, &view_count, nullptr); err != XR_SUCCESS) {
        throw std::runtime_error("Failed to enumerate view config views");
    }
    std::vector<XrViewConfigurationView> view_config_views(view_count, { .type = XR_TYPE_VIEW_CONFIGURATION_VIEW });
    if (auto err = xrEnumerateViewConfigurationViews(instance, system_id, view_config, view_count, &view_count, view_config_views.data()); err != XR_SUCCESS) {
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

    dbg(std::format("OpenXR graphicsRequirements: version {}-{}",
        XrVersionToString(gfx_requirements.minApiVersionSupported),
        XrVersionToString(gfx_requirements.maxApiVersionSupported)));
    dbg(std::format("OpenXR instanceExtensions: {}", get_instance_extensions()));
    dbg(std::format("OpenXR deviceExtensions: {}", get_device_extensions()));

    std::stringstream ss;
    ss << "OpenXR swapchainFormats:";
    for (auto fmt : swapchain_formats) {
        ss << " " << fmt;
    }
    dbg(ss.str());

    if (std::find(swapchain_formats.begin(), swapchain_formats.end(), VK_FORMAT_R8G8B8A8_SRGB) != swapchain_formats.end()) {
        swapchain_format = VK_FORMAT_R8G8B8A8_SRGB;
    } else {
        swapchain_format = swapchain_formats.front();
    }

    for (const auto& gfx : cfg.gfx) {
        auto supersampling = std::get<0>(gfx.second);

        OpenXRRenderContext* xr_ctx = new OpenXRRenderContext {};
        RenderContext ctx = {
            .ext = xr_ctx
        };

        for (size_t i = 0; i < view_config_views.size(); ++i) {
            ctx.width[i] = static_cast<uint32_t>(view_config_views[i].recommendedImageRectWidth * supersampling);
            ctx.height[i] = static_cast<uint32_t>(view_config_views[i].recommendedImageRectHeight * supersampling);

            XrSwapchainCreateInfo swapchain_create_info = {
                .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
                .createFlags = 0,
                .usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT,
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
            xr_ctx->swapchain_images[i].resize(imageCount, { .type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR });
            if (auto err = xrEnumerateSwapchainImages(
                    xr_ctx->swapchains[i],
                    xr_ctx->swapchain_images[i].size(),
                    &imageCount,
                    reinterpret_cast<XrSwapchainImageBaseHeader*>(xr_ctx->swapchain_images[i].data()));
                err != XR_SUCCESS) {
                throw std::runtime_error(std::format("Failed to initialize OpenXR: xrEnumerateSwapchainImages {}", XrResultToString(instance, err)));
            }
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
                        .primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
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
            .primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
        };
        if (auto res = xrBeginSession(session, &sessionBeginInfo); res != XR_SUCCESS) {
            throw std::runtime_error(std::format("Failed to initialize OpenXR. xrBeginSession: {}", XrResultToString(instance, res)));
        }
    }

    // In OpenXR we don't have separate matrices for eye positions
    // The eye position is taken account in the projection matrix already
    eye_pos[LeftEye] = glm::identity<glm::mat4x4>();
    eye_pos[RightEye] = glm::identity<glm::mat4x4>();
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

XrSwapchainImageVulkanKHR& OpenXR::acquire_swapchain_image(RenderTarget tgt)
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
    if (g::cfg.companion_mode != CompanionMode::Off) {
        // To draw the companion window, we must have the scene drawn to the texture
        // We can't show the swapchain image because of DXVK/Vulkan/OpenXR incompatibilities
        // Technically it would be possible I think but this will do for now.

        IDirect3DSurface9* eye;
        if (current_render_context->dx_texture[g::cfg.companion_eye]->GetSurfaceLevel(0, &eye) != D3D_OK) {
            dbg("Could not get surface level");
            eye = nullptr;
        }

        if (eye) {
            dev->StretchRect(current_render_context->dx_surface[g::cfg.companion_eye], nullptr, eye, nullptr, D3DTEXF_NONE);
            eye->Release();
        }
    }

    auto& left = acquire_swapchain_image(LeftEye);
    auto& right = acquire_swapchain_image(RightEye);

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

    // Essentially StretchRect but works directly with VkImage as a target
    // We need to do this copy as OpenXR wants to handle the swapchain images internally
    // and in DXVK there is no way to create a texture that would use this swapchain image.
    // Also, if we're using anti-aliasing, this step is needed anyways.

    if (g::cfg.wmr_workaround) {
        g::d3d_vr->Flush();
    }

    g::d3d_vr->CopySurfaceToVulkanImage(
        current_render_context->dx_surface[LeftEye],
        left.image,
        swapchain_format,
        current_render_context->width[LeftEye],
        current_render_context->height[LeftEye]);

    g::d3d_vr->CopySurfaceToVulkanImage(
        current_render_context->dx_surface[RightEye],
        right.image,
        swapchain_format,
        current_render_context->width[RightEye],
        current_render_context->height[RightEye]);

    xrReleaseSwapchainImage(xr_context()->swapchains[LeftEye], &release_info);
    xrReleaseSwapchainImage(xr_context()->swapchains[RightEye], &release_info);

    if (g::cfg.debug && perf_query_free_to_use) [[unlikely]] {
        gpu_end_query->Issue(D3DISSUE_END);
        gpu_disjoint_query->Issue(D3DISSUE_END);
    }
}

void OpenXR::submit_frames_to_hmd(IDirect3DDevice9* dev)
{
    xr_context()->projection_views[LeftEye].fov = xr_context()->views[LeftEye].fov;
    xr_context()->projection_views[LeftEye].pose = xr_context()->views[LeftEye].pose;
    xr_context()->projection_views[RightEye].fov = xr_context()->views[RightEye].fov;
    xr_context()->projection_views[RightEye].pose = xr_context()->views[RightEye].pose;

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

    g::d3d_vr->BeginVRSubmit();
    if (auto res = xrEndFrame(session, &frame_end_info); res != XR_SUCCESS) {
        dbg(std::format("xrEndFrame failed: {}", XrResultToString(instance, res)));
    }
    g::d3d_vr->EndVRSubmit();
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
        .viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
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
    if (g::cfg.world_scale != 1000 && view_count >= 2) {
        // Ported from:
        // https://github.com/mbucchia/OpenXR-Toolkit/blob/main/XR_APILAYER_MBUCCHIA_toolkit/layer.cpp#L1775-L1782
        // MIT License
        // Copyright (c) 2021-2022 Matthieu Bucchianeri
        // Copyright (c) 2021-2022 Jean-Luc Dupiot - Reality XP

        auto& pos0xr = views[0].pose.position;
        auto& pos1xr = views[1].pose.position;
        const auto pos0 = glm::vec3(pos0xr.x, pos0xr.y, pos0xr.z);
        const auto pos1 = glm::vec3(pos1xr.x, pos1xr.y, pos1xr.z);
        const auto vec = pos1 - pos0;
        const auto ipd = glm::length(vec);
        const float newIpd = (ipd * 1000) / g::cfg.world_scale;
        const auto center = pos0 + (vec * 0.5f);
        const auto offset = glm::normalize(vec) * (newIpd * 0.5f);

        const auto left = center - offset;
        const auto right = center + offset;

        views[0].pose.position = XrVector3f { left.x, left.y, left.z };
        views[1].pose.position = XrVector3f { right.x, right.y, right.z };
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
    for (auto i = 0; i < 2; ++i) {
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
    for (size_t i = 0; i < xr_context()->views.size(); ++i) {
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
    }
    xrDestroySession(session);
    xrDestroyInstance(instance);
}
