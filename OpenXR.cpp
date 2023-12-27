#define XR_USE_GRAPHICS_API_VULKAN
#include "OpenXR.hpp"
#include "Util.hpp"
#include "glm/gtx/quaternion.hpp"
#include <d3d9_interop.h>

#include "D3D.hpp"

static XrFrameState gFrameState; // TODO
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
    : swapchains()
    , session()
    , hasProjection(false)
{
    XrApplicationInfo appInfo = {
        .applicationName = "openRBRVR",
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

    swapchainFormat = swapchainFormats.front();

    for (size_t i = 0; i < viewConfigViews.size(); ++i) {
        XrSwapchainCreateInfo swapchainCreateInfo = {
            .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
            .createFlags = 0,
            .usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
            .format = swapchainFormat,
            .sampleCount = 1,
            .width = static_cast<uint32_t>(viewConfigViews[i].recommendedImageRectWidth * cfg.superSampling),
            .height = static_cast<uint32_t>(viewConfigViews[i].recommendedImageRectHeight * cfg.superSampling),
            .faceCount = 1,
            .arraySize = 1,
            .mipCount = 1,
        };
        if (auto err = xrCreateSwapchain(session, &swapchainCreateInfo, &swapchains[i]); err != XR_SUCCESS) {
            throw std::runtime_error(std::format("VR init failed. xrCreateSwapchain: {}", XrResultToString(instance, err)));
        }

        uint32_t imageCount;
        if (auto err = xrEnumerateSwapchainImages(swapchains[i], 0, &imageCount, nullptr); err != XR_SUCCESS) {
            throw std::runtime_error(std::format("Failed to initialize OpenXR: xrEnumerateSwapchainImages {}", XrResultToString(instance, err)));
        }
        swapchainImages[i].resize(imageCount, { .type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR });
        if (auto err = xrEnumerateSwapchainImages(
                swapchains[i],
                swapchainImages[i].size(),
                &imageCount,
                reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImages[i].data()));
            err != XR_SUCCESS) {
            throw std::runtime_error(std::format("Failed to initialize OpenXR: xrEnumerateSwapchainImages {}", XrResultToString(instance, err)));
        }
    }

    XrReferenceSpaceCreateInfo spaceCreateInfo = {
        .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
        .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL,
        .poseInReferenceSpace = { { 0, 0, 0, 1 }, { 0, 0, 0 } },
    };
    if (auto err = xrCreateReferenceSpace(session, &spaceCreateInfo, &space); err != XR_SUCCESS) {
        throw std::runtime_error(std::format("Failed to initialize OpenXR. xrCreateReferenceSpace {}", XrResultToString(instance, err)));
    }

    // TODO: The two eyes might have different resolutions
    auto wss = viewConfigViews[0].recommendedImageRectWidth * cfg.superSampling;
    auto hss = viewConfigViews[0].recommendedImageRectHeight * cfg.superSampling;
    InitSurfaces(dev, wss, hss, companionWindowWidth, companionWindowHeight);

    for (size_t i = 0; i < views.size(); ++i) {
        views[i] = { .type = XR_TYPE_VIEW };
        projectionViews[i] = {
            .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
            .next = nullptr,
            .subImage = {
                .swapchain = swapchains[i],
                .imageRect = {
                    .offset = { 0, 0 },
                    .extent = {
                        .width = static_cast<int>(viewConfigViews[i].recommendedImageRectWidth),
                        .height = static_cast<int>(viewConfigViews[i].recommendedImageRectHeight),
                    },
                },
                .imageArrayIndex = 0,
            }
        };
    }

    XrSessionBeginInfo sessionBeginInfo = {
        .type = XR_TYPE_SESSION_BEGIN_INFO,
        .next = nullptr,
        .primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
    };
    if (auto res = xrBeginSession(session, &sessionBeginInfo); res != XR_SUCCESS) {
        throw std::runtime_error(std::format("Failed to initialize OpenXR. xrBeginSession: {}", XrResultToString(instance, res)));
    }
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
    if (auto err = xrAcquireSwapchainImage(swapchains[tgt], &acqInfo, &idx); err != XR_SUCCESS) {
        Dbg(std::format("Could not acquire swapchain image: {}", XrResultToString(instance, err)));
    }

    return swapchainImages[tgt][idx];
}

void OpenXR::SubmitFramesToHMD(IDirect3DDevice9* dev)
{
    IDirect3DSurface9 *leftEye, *rightEye;

    gD3DVR->BeginVRSubmit();

    dxTexture[LeftEye]->GetSurfaceLevel(0, &leftEye);
    dxTexture[RightEye]->GetSurfaceLevel(0, &rightEye);

    if (gCfg.msaa != D3DMULTISAMPLE_NONE) {
        // Resolve multisampling
        dev->StretchRect(dxSurface[LeftEye], nullptr, leftEye, nullptr, D3DTEXF_NONE);
        dev->StretchRect(dxSurface[RightEye], nullptr, rightEye, nullptr, D3DTEXF_NONE);
    }

    gD3DVR->TransferSurfaceForVR(leftEye);
    gD3DVR->TransferSurfaceForVR(rightEye);

    auto left = AcquireSwapchainImage(LeftEye);
    auto right = AcquireSwapchainImage(RightEye);

    XrSwapchainImageWaitInfo info = {
        .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
        .next = nullptr,
        .timeout = XR_INFINITE_DURATION,
    };

    // TODO: Error handling
    xrWaitSwapchainImage(swapchains[LeftEye], &info);
    xrWaitSwapchainImage(swapchains[RightEye], &info);

    gD3DVR->CopySurfaceToVulkanImage(
        leftEye,
        left.image,
        swapchainFormat,
        projectionViews[LeftEye].subImage.imageRect.extent.width,
        projectionViews[LeftEye].subImage.imageRect.extent.height);

    gD3DVR->CopySurfaceToVulkanImage(
        rightEye,
        right.image,
        swapchainFormat,
        projectionViews[RightEye].subImage.imageRect.extent.width,
        projectionViews[RightEye].subImage.imageRect.extent.height);

    projectionViews[LeftEye].fov = views[LeftEye].fov;
    projectionViews[LeftEye].pose = views[LeftEye].pose;
    projectionViews[RightEye].fov = views[RightEye].fov;
    projectionViews[RightEye].pose = views[RightEye].pose;

    XrCompositionLayerProjection projectionLayer = {
        .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
        .next = nullptr,
        .layerFlags = 0,
        .space = space,
        .viewCount = projectionViews.size(),
        .views = projectionViews.data(),
    };

    XrCompositionLayerBaseHeader* layers[] = {
        reinterpret_cast<XrCompositionLayerBaseHeader*>(&projectionLayer),
    };

    XrFrameEndInfo frameEndInfo = {
        .type = XR_TYPE_FRAME_END_INFO,
        .displayTime = gFrameState.predictedDisplayTime,
        .environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
        .layerCount = 1,
        .layers = layers,
    };
    if (auto res = xrEndFrame(session, &frameEndInfo); res != XR_SUCCESS) {
        Dbg(std::format("xrEndFrame failed: {}", XrResultToString(instance, res)));
    }

    gD3DVR->EndVRSubmit();

    XrSwapchainImageReleaseInfo releaseInfo = {
        .type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
        .next = nullptr,
    };

    xrReleaseSwapchainImage(swapchains[LeftEye], &releaseInfo);
    xrReleaseSwapchainImage(swapchains[RightEye], &releaseInfo);
}

std::optional<XrViewState> OpenXR::UpdateViews(const XrFrameState& frameState)
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
    if (auto err = xrLocateViews(session, &viewLocateInfo, &viewState, views.size(), &viewCount, views.data()); err != XR_SUCCESS) {
        Dbg(std::format("xrLocateViews failed: {}", XrResultToString(instance, err)));
        return std::nullopt;
    }
    if (viewCount != views.size()) {
        Dbg(std::format("Unexpected viewcount: {}", viewCount));
        return std::nullopt;
    }

    return viewState;
}

bool OpenXR::UpdateVRPoses(Quaternion* carQuat, Config::HorizonLock lockSetting)
{
    gFrameState = {
        .type = XR_TYPE_FRAME_STATE,
        .next = nullptr,
    };

    if (auto res = xrWaitFrame(session, nullptr, &gFrameState); res != XR_SUCCESS) {
        Dbg(std::format("xrWaitFrame: {}", XrResultToString(instance, res)));
        return false;
    }
    if (auto res = xrBeginFrame(session, nullptr); res != XR_SUCCESS) {
        Dbg(std::format("xrBeginFrame: {}", XrResultToString(instance, res)));
        return false;
    }

    if (!hasProjection) {
        hasProjection = GetProjectionMatrix(gFrameState);
    }

    HMDPose = glm::identity<glm::mat4x4>();
    UpdatePoses(gFrameState);
    // TODO: Lock to horizon
    horizonLock = glm::identity<glm::mat4x4>();
    return false;
}

bool OpenXR::GetProjectionMatrix(const XrFrameState& frameState)
{
    auto viewState = UpdateViews(frameState);
    if (!viewState) {
        Dbg("Failed to update OpenXR views");
        return false;
    }
    for (auto i = 0; i < 2; ++i) {
        stageProjection[i] = CreateProjectionMatrix(views[i].fov, zNearStage, zFar);
        cockpitProjection[i] = CreateProjectionMatrix(views[i].fov, zNearCockpit, zFar);
        mainMenuProjection[i] = CreateProjectionMatrix(views[i].fov, zNearMainMenu, zFar);
    }
    return true;
}

void OpenXR::UpdatePoses(const XrFrameState& frameState)
{
    auto viewState = UpdateViews(frameState);
    if (!viewState) {
        Dbg("Failed to update OpenXR views");
        return;
    }

    auto& vs = viewState.value();
    for (size_t i = 0; i < views.size(); ++i) {
        if (vs.viewStateFlags & (XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT)) {
            eyePos[i] = glm::inverse(XrPoseToM4(views[i].pose));
        } else {
            Dbg("Invalid VR poses");
            return;
        }
    }
}

void OpenXR::ShutdownVR()
{
    xrDestroySpace(space);
    xrDestroySwapchain(swapchains[LeftEye]);
    xrDestroySwapchain(swapchains[RightEye]);
    xrDestroySession(session);
    xrDestroyInstance(instance);
}
