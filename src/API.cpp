#include "API.hpp"
#include "Config.hpp"
#include "Globals.hpp"
#include "IPlugin.h"
#include "OpenXR.hpp"
#include "VR.hpp"
#include "openRBRVR.hpp"

#include <MinHook.h>

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    MH_Initialize();
    return TRUE;
}

extern "C" __declspec(dllexport) IPlugin* RBR_CreatePlugin(IRBRGame* game)
{
    if (!g::openrbrvr) {
        g::openrbrvr = new openRBRVR(game);
    }
    return g::openrbrvr;
}

extern "C" __declspec(dllexport) VkResult CreateVulkanInstance(VkInstanceCreateInfo* create_info, PFN_vkGetInstanceProcAddr get_instance_proc_addr_fn, VkInstance* vulkan_instance)
{
    if (!g::vr || g::vr->get_runtime_type() != OPENXR) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    OpenXR* vr = reinterpret_cast<OpenXR*>(g::vr);
    VkResult vulkan_result;
    XrVulkanInstanceCreateInfoKHR instance_create_info = {
        .type = XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR,
        .next = nullptr,
        .systemId = vr->get_system_id(),
        .createFlags = 0,
        .pfnGetInstanceProcAddr = get_instance_proc_addr_fn,
        .vulkanCreateInfo = create_info,
        .vulkanAllocator = nullptr,
    };

    const auto instance = vr->get_instance();
    auto xrCreateVulkanInstanceKHR = OpenXR::get_extension<PFN_xrCreateVulkanInstanceKHR>(instance, "xrCreateVulkanInstanceKHR");
    if (auto err = xrCreateVulkanInstanceKHR(instance, &instance_create_info, vulkan_instance, &vulkan_result); err != XR_SUCCESS) {
        dbg(std::format("Failed to initialize OpenXR: xrCreateVulkanInstanceKHR {}", OpenXR::XrResultToString(instance, err)));
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    XrVulkanGraphicsDeviceGetInfoKHR gfx_device_get_info = {
        .type = XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR,
        .next = nullptr,
        .systemId = vr->get_system_id(),
        .vulkanInstance = *vulkan_instance,
    };

    auto xrGetVulkanGraphicsDevice2KHR = OpenXR::get_extension<PFN_xrGetVulkanGraphicsDevice2KHR>(instance, "xrGetVulkanGraphicsDevice2KHR");

    // This function needs to be called before calling xrCreateVulkanDeviceKHR
    // The OpenXR runtime might store some internal state (at least PimaxXR does)
    // even though we're not using the return value for anything here.
    VkPhysicalDevice vulkan_physical_device;
    if (auto err = xrGetVulkanGraphicsDevice2KHR(instance, &gfx_device_get_info, &vulkan_physical_device); err != XR_SUCCESS) {
        dbg(std::format("Failed to initialize OpenXR: xrGetVulkanGraphicsDevice2KHR {}", OpenXR::XrResultToString(instance, err)));
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vulkan_result;
}

extern "C" __declspec(dllexport) VkResult CreateVulkanDevice(VkPhysicalDevice vulkan_physical_device, VkDeviceCreateInfo* create_info, PFN_vkGetInstanceProcAddr get_instance_proc_addr_fn, VkDevice* vulkan_device)
{
    if (!g::vr || g::vr->get_runtime_type() != OPENXR) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    OpenXR* vr = reinterpret_cast<OpenXR*>(g::vr);

    XrVulkanDeviceCreateInfoKHR vulkan_device_create_info = {
        .type = XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR,
        .next = nullptr,
        .systemId = vr->get_system_id(),
        .createFlags = 0,
        .pfnGetInstanceProcAddr = get_instance_proc_addr_fn,
        .vulkanPhysicalDevice = vulkan_physical_device,
        .vulkanCreateInfo = create_info,
        .vulkanAllocator = nullptr,
    };

    const auto instance = vr->get_instance();
    auto xrCreateVulkanDeviceKHR = OpenXR::get_extension<PFN_xrCreateVulkanDeviceKHR>(instance, "xrCreateVulkanDeviceKHR");

    VkResult vulkan_result;
    if (auto err = xrCreateVulkanDeviceKHR(instance, &vulkan_device_create_info, vulkan_device, &vulkan_result); err != XR_SUCCESS) {
        dbg(std::format("Failed to initialize OpenXR: xrCreateVulkanDeviceKHR {}", OpenXR::XrResultToString(instance, err)));
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vulkan_result;
}

extern "C" __declspec(dllexport) int64_t openRBRVR_Exec(ApiOperation ops, uint64_t value)
{
    dbg(std::format("Exec: {} {}", (uint64_t)ops, (uint64_t)value));

    if (ops == API_VERSION) {
        return 3;
    }
    if (ops & MOVE_SEAT) {
        if (value > static_cast<uint64_t>(MOVE_SEAT_DOWN)) {
            return 1;
        }
        g::seat_movement_request = static_cast<SeatMovement>(value);
    }
    if (ops & RECENTER_VR_VIEW) {
        g::vr->reset_view();
    }
    if (ops & TOGGLE_DEBUG_INFO) {
        g::cfg.debug = !g::cfg.debug;
        g::draw_overlay_border = (g::cfg.debug && g::cfg.debug_mode == 0);
    }
    if (ops & GET_VR_RUNTIME) {
        if (!g::vr) {
            return 0;
        }
        return static_cast<int64_t>(g::vr->get_runtime_type());
    }
    return 0;
}
