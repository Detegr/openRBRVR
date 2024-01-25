#define XR_USE_GRAPHICS_API_VULKAN
#include "OpenXR.hpp"
#include "Util.hpp"
#include <d3d9_interop.h>
#include <gtx/quaternion.hpp>

#include "D3D.hpp"

extern Config gCfg;

static std::string XrResultToString(const XrInstance& instance, XrResult res)
{
    char buf[XR_MAX_RESULT_STRING_SIZE] = { 0 };

    if (auto err = xrResultToString(instance, res, buf); err != XR_SUCCESS) {
        Dbg("Could not convert XrResult to string");
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
T GetExtension(XrInstance instance, const std::string& fnName)
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

static glm::mat4 CreateProjectionMatrix(const XrFovf& fov, float nearClip, float farClip)
{
    // Convert angles from radians to degrees
    const auto tanLeft = std::tanf(fov.angleLeft);
    const auto tanRight = std::tanf(fov.angleRight);
    const auto tanDown = std::tanf(fov.angleDown);
    const auto tanUp = std::tanf(fov.angleUp);

    const auto tanWidth = tanRight - tanLeft;
    const auto tanHeight = tanUp - tanDown;

    auto projection = glm::mat4(0.0f);
    projection[0][0] = 2.0f / tanWidth;
    projection[1][1] = 2.0f / tanHeight;
    projection[2][0] = (tanRight + tanLeft) / tanWidth;
    projection[2][1] = (tanUp + tanDown) / tanHeight;
    projection[2][2] = -(farClip + nearClip) / (farClip - nearClip);
    projection[2][3] = -1.0f;
    projection[3][2] = -(2.0f * farClip * nearClip) / (farClip - nearClip);

    return projection;
}

static M4 XrPoseToM4(const XrPosef& pose)
{
    const glm::quat orientation(pose.orientation.w, pose.orientation.x, pose.orientation.y, pose.orientation.z);
    const glm::vec3 position(pose.position.x, pose.position.y, pose.position.z);

    const glm::mat4 rotationMatrix = glm::toMat4(orientation);
    const glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);

    return translationMatrix * rotationMatrix; // Combine rotation and translation
}

OpenXR::OpenXR()
    : session()
    , hasProjection(false)
    , resetViewRequested(false)
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
        throw std::runtime_error(std::format("Failed to initialize OpenXR: xrCreateInstance {}", static_cast<int>(err)));
    }

    XrSystemGetInfo systemGetInfo = {
        .type = XR_TYPE_SYSTEM_GET_INFO,
        .next = nullptr,
        .formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY,
    };
    if (auto err = xrGetSystem(instance, &systemGetInfo, &systemId); err != XR_SUCCESS) {
        throw std::runtime_error(std::format("Failed to initialize OpenXR: xrGetSystem {}", XrResultToString(instance, err)));
    }
}

void OpenXR::Init(IDirect3DDevice9* dev, const Config& cfg, IDirect3DVR9** vrdev, uint32_t companionWindowWidth, uint32_t companionWindowHeight)
{
    if (dev->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &gpuStartQuery) != D3D_OK) {
        throw std::runtime_error("VR initialization failed: CreateQuery");
    }
    if (dev->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &gpuEndQuery) != D3D_OK) {
        throw std::runtime_error("VR initialization failed: CreateQuery");
    }
    if (dev->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT, &gpuDisjointQuery) != D3D_OK) {
        throw std::runtime_error("VR initialization failed: CreateQuery");
    }
    if (dev->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, &gpuFreqQuery) != D3D_OK) {
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
    auto xrGetVulkanGraphicsDeviceKHR = GetExtension<PFN_xrGetVulkanGraphicsDeviceKHR>(instance, "xrGetVulkanGraphicsDeviceKHR");
    // This needs to be called even though we get back the same pointer that we had already in OXR_VK_DEVICE_DESC
    xrGetVulkanGraphicsDeviceKHR(instance, systemId, vkDesc.Instance, &phys);

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
        .systemId = systemId,
    };

    XrGraphicsRequirementsVulkanKHR graphicsRequirements = { .type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR };
    auto xrGetVulkanGraphicsRequirementsKHR = GetExtension<PFN_xrGetVulkanGraphicsRequirementsKHR>(instance, "xrGetVulkanGraphicsRequirementsKHR");
    if (auto err = xrGetVulkanGraphicsRequirementsKHR(instance, systemId, &graphicsRequirements); err != XR_SUCCESS) {
        throw std::runtime_error(std::format("Failed to initialize OpenXR: xrGetVulkanGraphicsRequirementsKHR {}", XrResultToString(instance, err)));
    }

    if (auto err = xrCreateSession(instance, &sessionCreateInfo, &session); err != XR_SUCCESS) {
        throw std::runtime_error(std::format("Failed to initialize OpenXR: xrCreateSession {}", XrResultToString(instance, err)));
    }

    XrViewConfigurationType viewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    uint32_t viewConfigCount;
    if (auto err = xrEnumerateViewConfigurations(instance, systemId, 0, &viewConfigCount, &viewConfigType); err != XR_SUCCESS) {
        throw std::runtime_error("Failed to enumerate view configurations");
    }
    std::vector<XrViewConfigurationType> viewConfigs(viewConfigCount);
    if (auto err = xrEnumerateViewConfigurations(instance, systemId, viewConfigCount, &viewConfigCount, viewConfigs.data()); err != XR_SUCCESS) {
        throw std::runtime_error("Failed to enumerate view configurations");
    }

    auto viewConfig = viewConfigs.front();
    uint32_t viewCount;
    if (auto err = xrEnumerateViewConfigurationViews(instance, systemId, viewConfig, 0, &viewCount, nullptr); err != XR_SUCCESS) {
        throw std::runtime_error("Failed to enumerate view config views");
    }
    std::vector<XrViewConfigurationView> viewConfigViews(viewCount, { .type = XR_TYPE_VIEW_CONFIGURATION_VIEW });
    if (auto err = xrEnumerateViewConfigurationViews(instance, systemId, viewConfig, viewCount, &viewCount, viewConfigViews.data()); err != XR_SUCCESS) {
        throw std::runtime_error("Failed to enumerate view config views");
    }

    uint32_t formatCount;
    if (auto err = xrEnumerateSwapchainFormats(session, 0, &formatCount, nullptr); err != XR_SUCCESS) {
        throw std::runtime_error("Failed to enumerate swapchain formats");
    }
    std::vector<int64_t> swapchainFormats(formatCount, 0);
    if (auto err = xrEnumerateSwapchainFormats(session, formatCount, &formatCount, swapchainFormats.data()); err != XR_SUCCESS) {
        throw std::runtime_error("Failed to enumerate swapchain formats");
    }

    Dbg(std::format("OpenXR graphicsRequirements: version {}-{}",
        XrVersionToString(graphicsRequirements.minApiVersionSupported),
        XrVersionToString(graphicsRequirements.maxApiVersionSupported)));
    Dbg(std::format("OpenXR instanceExtensions: {}", GetInstanceExtensions()));
    Dbg(std::format("OpenXR deviceExtensions: {}", GetDeviceExtensions()));

    std::stringstream ss;
    ss << "OpenXR swapchainFormats:";
    for (auto fmt : swapchainFormats) {
        ss << " " << fmt;
    }
    Dbg(ss.str());

    if (std::find(swapchainFormats.begin(), swapchainFormats.end(), VK_FORMAT_R8G8B8A8_SRGB) != swapchainFormats.end()) {
        swapchainFormat = VK_FORMAT_R8G8B8A8_SRGB;
    } else {
        swapchainFormat = swapchainFormats.front();
    }

    for (const auto& gfx : cfg.gfx) {
        auto superSampling = std::get<0>(gfx.second);

        OpenXRRenderContext* xrCtx = new OpenXRRenderContext {};
        RenderContext ctx = {
            .ext = xrCtx
        };

        for (size_t i = 0; i < viewConfigViews.size(); ++i) {
            ctx.width[i] = static_cast<uint32_t>(viewConfigViews[i].recommendedImageRectWidth * superSampling);
            ctx.height[i] = static_cast<uint32_t>(viewConfigViews[i].recommendedImageRectHeight * superSampling);

            XrSwapchainCreateInfo swapchainCreateInfo = {
                .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
                .createFlags = 0,
                .usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT,
                .format = swapchainFormat,
                .sampleCount = 1,
                .width = ctx.width[i],
                .height = ctx.height[i],
                .faceCount = 1,
                .arraySize = 1,
                .mipCount = 1,
            };
            Dbg(std::format("requesting swapchain: {}x{}, format {}", swapchainCreateInfo.width, swapchainCreateInfo.height, swapchainCreateInfo.format));

            if (auto err = xrCreateSwapchain(session, &swapchainCreateInfo, &xrCtx->swapchains[i]); err != XR_SUCCESS) {
                throw std::runtime_error(std::format("VR init failed. xrCreateSwapchain: {}", XrResultToString(instance, err)));
            }

            uint32_t imageCount;
            if (auto err = xrEnumerateSwapchainImages(xrCtx->swapchains[i], 0, &imageCount, nullptr); err != XR_SUCCESS) {
                throw std::runtime_error(std::format("Failed to initialize OpenXR: xrEnumerateSwapchainImages {}", XrResultToString(instance, err)));
            }
            xrCtx->swapchainImages[i].resize(imageCount, { .type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR });
            if (auto err = xrEnumerateSwapchainImages(
                    xrCtx->swapchains[i],
                    xrCtx->swapchainImages[i].size(),
                    &imageCount,
                    reinterpret_cast<XrSwapchainImageBaseHeader*>(xrCtx->swapchainImages[i].data()));
                err != XR_SUCCESS) {
                throw std::runtime_error(std::format("Failed to initialize OpenXR: xrEnumerateSwapchainImages {}", XrResultToString(instance, err)));
            }
        }

        InitSurfaces(dev, ctx, companionWindowWidth, companionWindowHeight);

        for (size_t i = 0; i < xrCtx->views.size(); ++i) {
            xrCtx->views[i] = { .type = XR_TYPE_VIEW };
            xrCtx->projectionViews[i] = {
                .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
                .next = nullptr,
                .subImage = {
                    .swapchain = xrCtx->swapchains[i],
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

        renderContexts[gfx.first] = ctx;
    }

    SetRenderContext("default");

    viewPose = { { 0, 0, 0, 1 }, { 0, 0, 0 } };

    XrReferenceSpaceCreateInfo spaceCreateInfo = {
        .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
        .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL,
        .poseInReferenceSpace = viewPose,
    };
    if (auto err = xrCreateReferenceSpace(session, &spaceCreateInfo, &space); err != XR_SUCCESS) {
        throw std::runtime_error(std::format("Failed to initialize OpenXR. xrCreateReferenceSpace {}", XrResultToString(instance, err)));
    }
    bool sessionRunning = false;
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

                    sessionRunning = true;
                    break;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (!sessionRunning) {
        Dbg("Did not receive XR_SESSION_STATE_READY event, launching the session anyway...");
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
    eyePos[LeftEye] = glm::identity<glm::mat4x4>();
    eyePos[RightEye] = glm::identity<glm::mat4x4>();
}

const char* OpenXR::GetDeviceExtensions()
{
    if (deviceExtensions.empty()) {
        uint32_t extensionListLen;
        auto xrGetVulkanDeviceExtensionsKHR = GetExtension<PFN_xrGetVulkanDeviceExtensionsKHR>(instance, "xrGetVulkanDeviceExtensionsKHR");
        if (auto err = xrGetVulkanDeviceExtensionsKHR(instance, systemId, 0, &extensionListLen, nullptr); err != XR_SUCCESS) {
            throw std::runtime_error(std::format("OpenXR init failed: xrGetVulkanDeviceExtensionsKHR: {}", XrResultToString(instance, err)));
        }
        deviceExtensions.resize(extensionListLen, 0);
        if (auto err = xrGetVulkanDeviceExtensionsKHR(instance, systemId, deviceExtensions.size(), &extensionListLen, deviceExtensions.data()); err != XR_SUCCESS) {
            throw std::runtime_error(std::format("OpenXR init failed: xrGetVulkanDeviceExtensionsKHR: {}", XrResultToString(instance, err)));
        }
    }
    return deviceExtensions.data();
}

const char* OpenXR::GetInstanceExtensions()
{
    if (instanceExtensions.empty()) {
        uint32_t extensionListLen;
        auto xrGetVulkanInstanceExtensionsKHR = GetExtension<PFN_xrGetVulkanInstanceExtensionsKHR>(instance, "xrGetVulkanInstanceExtensionsKHR");
        if (auto err = xrGetVulkanInstanceExtensionsKHR(instance, systemId, 0, &extensionListLen, nullptr); err != XR_SUCCESS) {
            throw std::runtime_error(std::format("OpenXR init failed: xrGetVulkanInstanceExtensionsKHR: {}", XrResultToString(instance, err)));
        }
        instanceExtensions.resize(extensionListLen, 0);
        if (auto err = xrGetVulkanInstanceExtensionsKHR(instance, systemId, instanceExtensions.size(), &extensionListLen, instanceExtensions.data()); err != XR_SUCCESS) {
            throw std::runtime_error(std::format("OpenXR init failed: xrGetVulkanInstanceExtensionsKHR: {}", XrResultToString(instance, err)));
        }
    }
    return instanceExtensions.data();
}

XrSwapchainImageVulkanKHR& OpenXR::AcquireSwapchainImage(RenderTarget tgt)
{
    XrSwapchainImageAcquireInfo acqInfo = {
        .type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
        .next = nullptr,
    };

    uint32_t idx;
    if (auto err = xrAcquireSwapchainImage(XrContext()->swapchains[tgt], &acqInfo, &idx); err != XR_SUCCESS) {
        Dbg(std::format("Could not acquire swapchain image: {}", XrResultToString(instance, err)));
    }

    return XrContext()->swapchainImages[tgt][idx];
}

void OpenXR::PrepareFramesForHMD(IDirect3DDevice9* dev)
{
    if (gCfg.drawCompanionWindow) {
        // To draw the companion window, we must have the scene drawn to the texture
        // We can't show the swapchain image because of DXVK/Vulkan/OpenXR incompatibilities
        // Technically it would be possible I think but this will do for now.

        IDirect3DSurface9* leftEye;
        if (currentRenderContext->dxTexture[LeftEye]->GetSurfaceLevel(0, &leftEye) != D3D_OK) {
            Dbg("Could not get surface level");
            leftEye = nullptr;
        }

        if (leftEye) {
            dev->StretchRect(currentRenderContext->dxSurface[LeftEye], nullptr, leftEye, nullptr, D3DTEXF_NONE);
            leftEye->Release();
        }
    }

    auto& left = AcquireSwapchainImage(LeftEye);
    auto& right = AcquireSwapchainImage(RightEye);

    XrSwapchainImageWaitInfo info = {
        .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
        .next = nullptr,
        .timeout = XR_INFINITE_DURATION,
    };
    XrSwapchainImageReleaseInfo releaseInfo = {
        .type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
        .next = nullptr,
    };

    if (auto res = xrWaitSwapchainImage(XrContext()->swapchains[LeftEye], &info); res != XR_SUCCESS) {
        Dbg(std::format("xrWaitSwapchainImage (left): {}", XrResultToString(instance, res)));
        return;
    }
    if (auto res = xrWaitSwapchainImage(XrContext()->swapchains[RightEye], &info); res != XR_SUCCESS) {
        Dbg(std::format("xrWaitSwapchainImage (right): {}", XrResultToString(instance, res)));
        xrReleaseSwapchainImage(XrContext()->swapchains[LeftEye], &releaseInfo);
        return;
    }

    // Essentially StretchRect but works directly with VkImage as a target
    // We need to do this copy as OpenXR wants to handle the swapchain images internally
    // and in DXVK there is no way to create a texture that would use this swapchain image.
    // Also, if we're using anti-aliasing, this step is needed anyways.

    if (gCfg.wmrWorkaround) {
        gD3DVR->Flush();
    }

    gD3DVR->CopySurfaceToVulkanImage(
        currentRenderContext->dxSurface[LeftEye],
        left.image,
        swapchainFormat,
        currentRenderContext->width[LeftEye],
        currentRenderContext->height[LeftEye]);

    gD3DVR->CopySurfaceToVulkanImage(
        currentRenderContext->dxSurface[RightEye],
        right.image,
        swapchainFormat,
        currentRenderContext->width[RightEye],
        currentRenderContext->height[RightEye]);

    xrReleaseSwapchainImage(XrContext()->swapchains[LeftEye], &releaseInfo);
    xrReleaseSwapchainImage(XrContext()->swapchains[RightEye], &releaseInfo);

    if (gCfg.debug && perfQueryFree) [[unlikely]] {
        gpuEndQuery->Issue(D3DISSUE_END);
        gpuDisjointQuery->Issue(D3DISSUE_END);
    }
}

void OpenXR::SubmitFramesToHMD(IDirect3DDevice9* dev)
{
    XrContext()->projectionViews[LeftEye].fov = XrContext()->views[LeftEye].fov;
    XrContext()->projectionViews[LeftEye].pose = XrContext()->views[LeftEye].pose;
    XrContext()->projectionViews[RightEye].fov = XrContext()->views[RightEye].fov;
    XrContext()->projectionViews[RightEye].pose = XrContext()->views[RightEye].pose;

    XrCompositionLayerProjection projectionLayer = {
        .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
        .next = nullptr,
        .layerFlags = 0,
        .space = space,
        .viewCount = XrContext()->projectionViews.size(),
        .views = XrContext()->projectionViews.data(),
    };

    XrCompositionLayerBaseHeader* layers[] = {
        reinterpret_cast<XrCompositionLayerBaseHeader*>(&projectionLayer),
    };

    XrFrameEndInfo frameEndInfo = {
        .type = XR_TYPE_FRAME_END_INFO,
        .displayTime = frameState.predictedDisplayTime,
        .environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
        .layerCount = 1,
        .layers = layers,
    };

    gD3DVR->BeginVRSubmit();
    if (auto res = xrEndFrame(session, &frameEndInfo); res != XR_SUCCESS) {
        Dbg(std::format("xrEndFrame failed: {}", XrResultToString(instance, res)));
    }
    gD3DVR->EndVRSubmit();
}

std::optional<XrViewState> OpenXR::UpdateViews()
{
    XrViewState viewState = {
        .type = XR_TYPE_VIEW_STATE,
        .next = nullptr,
        .viewStateFlags = 0,
    };
    const XrViewLocateInfo viewLocateInfo = {
        .type = XR_TYPE_VIEW_LOCATE_INFO,
        .next = nullptr,
        .viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
        .displayTime = frameState.predictedDisplayTime,
        .space = space,
    };

    uint32_t viewCount = 0;
    if (auto err = xrLocateViews(session, &viewLocateInfo, &viewState, XrContext()->views.size(), &viewCount, XrContext()->views.data()); err != XR_SUCCESS) {
        Dbg(std::format("xrLocateViews failed: {}", XrResultToString(instance, err)));
        return std::nullopt;
    }
    if (viewCount != XrContext()->views.size()) {
        Dbg(std::format("Unexpected viewcount: {}", viewCount));
        return std::nullopt;
    }

    return viewState;
}

bool OpenXR::UpdateVRPoses(Quaternion* carQuat, Config::HorizonLock lockSetting)
{
    frameState = {
        .type = XR_TYPE_FRAME_STATE,
        .next = nullptr,
    };

    if (auto res = xrWaitFrame(session, nullptr, &frameState); res != XR_SUCCESS) {
        Dbg(std::format("xrWaitFrame: {}", XrResultToString(instance, res)));
        return false;
    }

    if (frameState.shouldRender == XR_FALSE) {
        // Oculus runtime has a bug where shouldRender is always false... o_o
        // So we do nothing about it.
    }

    if (resetViewRequested) {
        resetViewRequested = false;
        RecenterView();
    }

    if (auto res = xrBeginFrame(session, nullptr); res != XR_SUCCESS) {
        Dbg(std::format("xrBeginFrame: {}", XrResultToString(instance, res)));
    }

    if (!hasProjection) {
        // TODO: Should this be called every frame or is it okay to do
        // like OpenVR does and just fetch the projection matrices once?
        hasProjection = GetProjectionMatrix();
    }

    UpdatePoses();

    horizonLock = GetHorizonLockMatrix(carQuat, lockSetting);

    if (gCfg.debug && perfQueryFree) [[unlikely]] {
        gpuDisjointQuery->Issue(D3DISSUE_BEGIN);
        gpuStartQuery->Issue(D3DISSUE_END);
    }

    return true;
}

bool OpenXR::GetProjectionMatrix()
{
    auto viewState = UpdateViews();
    if (!viewState) {
        Dbg("Failed to update OpenXR views");
        return false;
    }
    for (auto i = 0; i < 2; ++i) {
        stageProjection[i] = CreateProjectionMatrix(XrContext()->views[i].fov, zNearStage, zFar);
        cockpitProjection[i] = CreateProjectionMatrix(XrContext()->views[i].fov, zNearCockpit, zFar);
        mainMenuProjection[i] = CreateProjectionMatrix(XrContext()->views[i].fov, zNearMainMenu, zFar);
    }
    return true;
}

void OpenXR::UpdatePoses()
{
    auto viewState = UpdateViews();
    if (!viewState) {
        Dbg("Failed to update OpenXR views");
        return;
    }

    auto& vs = viewState.value();
    for (size_t i = 0; i < XrContext()->views.size(); ++i) {
        if (vs.viewStateFlags & (XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT)) {
            HMDPose[i] = glm::inverse(XrPoseToM4(XrContext()->views[i].pose));
        } else {
            Dbg("Invalid VR poses");
            return;
        }
    }
}

FrameTimingInfo OpenXR::GetFrameTiming()
{
    FrameTimingInfo ret = { 0 };

    BOOL disjoint;
    uint64_t gpuStart, gpuEnd;
    if (gpuDisjointQuery->GetData(&disjoint, sizeof(disjoint), 0) == D3D_OK && !disjoint) {
        gpuStartQuery->GetData(&gpuStart, sizeof(gpuStart), 0);
        gpuEndQuery->GetData(&gpuEnd, sizeof(gpuEnd), 0);

        uint64_t freq;
        gpuFreqQuery->Issue(D3DISSUE_END);
        gpuFreqQuery->GetData(&freq, sizeof(freq), 0);

        ret.gpuTotal = float(gpuEnd - gpuStart) / float(freq);

        perfQueryFree = true;
    } else {
        perfQueryFree = false;
    }

    return ret;
}

void OpenXR::ResetView()
{
    resetViewRequested = true;
}

void OpenXR::RecenterView()
{
    XrSpace viewSpace;
    XrReferenceSpaceCreateInfo viewSpaceCreateInfo = {
        .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
        .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW,
        .poseInReferenceSpace = { { 0, 0, 0, 1 }, { 0, 0, 0 } },
    };
    if (auto res = xrCreateReferenceSpace(session, &viewSpaceCreateInfo, &viewSpace); res != XR_SUCCESS) {
        Dbg(std::format("Failed to recenter VR view: {}", XrResultToString(instance, res)));
        return;
    }

    XrSpaceLocation spaceLocation = {
        .type = XR_TYPE_SPACE_LOCATION,
        .next = nullptr,
    };
    if (auto res = xrLocateSpace(viewSpace, space, frameState.predictedDisplayTime, &spaceLocation); res != XR_SUCCESS) {
        if (auto res = xrDestroySpace(viewSpace); res != XR_SUCCESS) {
            Dbg(std::format("Failed to destroy old viewSpace: {}", XrResultToString(instance, res)));
        }
        Dbg(std::format("Failed to recenter VR view: {}", XrResultToString(instance, res)));
        return;
    }

    auto& vpo = viewPose.orientation;
    auto& slo = spaceLocation.pose.orientation;
    auto orientationDiff = glm::quat(slo.w, 0, slo.y, 0);
    auto poseOrientation = glm::quat(vpo.w, 0, vpo.y, 0);
    auto newOrientation = glm::normalize(poseOrientation * orientationDiff);

    Dbg(std::format("viewPose position {}, {}, {}", viewPose.position.x, viewPose.position.y, viewPose.position.z));
    Dbg(std::format("spaceLocation position {}, {}, {}", spaceLocation.pose.position.x, spaceLocation.pose.position.y, spaceLocation.pose.position.z));
    Dbg(std::format("viewPose orientation {}, {}, {}", viewPose.orientation.x, viewPose.orientation.y, viewPose.orientation.z));
    Dbg(std::format("spaceLocation orientation {}, {}, {}", spaceLocation.pose.orientation.x, spaceLocation.pose.orientation.y, spaceLocation.pose.orientation.z));
    Dbg(std::format("new orientation: {}, {}, {}, {}", newOrientation.x, newOrientation.y, newOrientation.z, newOrientation.w));

    XrPosef pose = {
        .orientation = {
            .x = newOrientation.x,
            .y = newOrientation.y,
            .z = newOrientation.z,
            .w = newOrientation.w,
        },
        .position = {
            .x = viewPose.position.x + spaceLocation.pose.position.x,
            .y = viewPose.position.y + spaceLocation.pose.position.y,
            .z = viewPose.position.z + spaceLocation.pose.position.z,
        }
    };

    XrSpace newSpace;
    XrReferenceSpaceCreateInfo spaceCreateInfo = {
        .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
        .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL,
        .poseInReferenceSpace = pose,
    };
    if (auto res = xrCreateReferenceSpace(session, &spaceCreateInfo, &newSpace); res != XR_SUCCESS) {
        Dbg(std::format("Failed to recenter VR view: {}", XrResultToString(instance, res)));
        return;
    }

    if (auto res = xrDestroySpace(viewSpace); res != XR_SUCCESS) {
        Dbg(std::format("Failed to destroy old viewSpace: {}", XrResultToString(instance, res)));
    }
    if (auto res = xrDestroySpace(space); res != XR_SUCCESS) {
        Dbg(std::format("Failed to destroy old space: {}", XrResultToString(instance, res)));
        return;
    }

    space = newSpace;
    viewPose = pose;
}

void OpenXR::ShutdownVR()
{
    xrDestroySpace(space);
    for (const auto& v : renderContexts) {
        const auto& ctx = v.second;
        auto xrCtx = reinterpret_cast<OpenXRRenderContext*>(ctx.ext);
        xrDestroySwapchain(xrCtx->swapchains[LeftEye]);
        xrDestroySwapchain(xrCtx->swapchains[RightEye]);
    }
    xrDestroySession(session);
    xrDestroyInstance(instance);
}
