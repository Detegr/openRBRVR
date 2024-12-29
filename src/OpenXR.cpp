#include "OpenXR.hpp"
#include "Config.hpp"
#include "Globals.hpp"
#include "Util.hpp"
#include <d3d9_interop.h>
#include <gtx/quaternion.hpp>
#include <ranges>

#include "D3D.hpp"
#include <d3d11.h>
#include <d3d11_1.h>
#include <ranges>

namespace g {
    bool api_layer_search_path_fixed;
}

constexpr std::string xr_result_to_str(XrResult res)
{
    // XrResultToString can't be used without a valid XrInstance
    // so rolling out my own
    switch (res) {
        case 0: return "XR_SUCCESS";
        case 1: return "XR_TIMEOUT_EXPIRED";
        case 3: return "XR_SESSION_LOSS_PENDING";
        case 4: return "XR_EVENT_UNAVAILABLE";
        case 7: return "XR_SPACE_BOUNDS_UNAVAILABLE";
        case 8: return "XR_SESSION_NOT_FOCUSED";
        case 9: return "XR_FRAME_DISCARDED";
        case -1: return "XR_ERROR_VALIDATION_FAILURE";
        case -2: return "XR_ERROR_RUNTIME_FAILURE";
        case -3: return "XR_ERROR_OUT_OF_MEMORY";
        case -4: return "XR_ERROR_API_VERSION_UNSUPPORTED";
        case -6: return "XR_ERROR_INITIALIZATION_FAILED";
        case -7: return "XR_ERROR_FUNCTION_UNSUPPORTED";
        case -8: return "XR_ERROR_FEATURE_UNSUPPORTED";
        case -9: return "XR_ERROR_EXTENSION_NOT_PRESENT";
        case -10: return "XR_ERROR_LIMIT_REACHED";
        case -11: return "XR_ERROR_SIZE_INSUFFICIENT";
        case -12: return "XR_ERROR_HANDLE_INVALID";
        case -13: return "XR_ERROR_INSTANCE_LOST";
        case -14: return "XR_ERROR_SESSION_RUNNING";
        case -16: return "XR_ERROR_SESSION_NOT_RUNNING";
        case -17: return "XR_ERROR_SESSION_LOST";
        case -18: return "XR_ERROR_SYSTEM_INVALID";
        case -19: return "XR_ERROR_PATH_INVALID";
        case -20: return "XR_ERROR_PATH_COUNT_EXCEEDED";
        case -21: return "XR_ERROR_PATH_FORMAT_INVALID";
        case -22: return "XR_ERROR_PATH_UNSUPPORTED";
        case -23: return "XR_ERROR_LAYER_INVALID";
        case -24: return "XR_ERROR_LAYER_LIMIT_EXCEEDED";
        case -25: return "XR_ERROR_SWAPCHAIN_RECT_INVALID";
        case -26: return "XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED";
        case -27: return "XR_ERROR_ACTION_TYPE_MISMATCH";
        case -28: return "XR_ERROR_SESSION_NOT_READY";
        case -29: return "XR_ERROR_SESSION_NOT_STOPPING";
        case -30: return "XR_ERROR_TIME_INVALID";
        case -31: return "XR_ERROR_REFERENCE_SPACE_UNSUPPORTED";
        case -32: return "XR_ERROR_FILE_ACCESS_ERROR";
        case -33: return "XR_ERROR_FILE_CONTENTS_INVALID";
        case -34: return "XR_ERROR_FORM_FACTOR_UNSUPPORTED";
        case -35: return "XR_ERROR_FORM_FACTOR_UNAVAILABLE";
        case -36: return "XR_ERROR_API_LAYER_NOT_PRESENT";
        case -37: return "XR_ERROR_CALL_ORDER_INVALID";
        case -38: return "XR_ERROR_GRAPHICS_DEVICE_INVALID";
        case -39: return "XR_ERROR_POSE_INVALID";
        case -40: return "XR_ERROR_INDEX_OUT_OF_RANGE";
        case -41: return "XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED";
        case -42: return "XR_ERROR_ENVIRONMENT_BLEND_MODE_UNSUPPORTED";
        case -44: return "XR_ERROR_NAME_DUPLICATED";
        case -45: return "XR_ERROR_NAME_INVALID";
        case -46: return "XR_ERROR_ACTIONSET_NOT_ATTACHED";
        case -47: return "XR_ERROR_ACTIONSETS_ALREADY_ATTACHED";
        case -48: return "XR_ERROR_LOCALIZED_NAME_DUPLICATED";
        case -49: return "XR_ERROR_LOCALIZED_NAME_INVALID";
        case -50: return "XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING";
        case -51: return "XR_ERROR_RUNTIME_UNAVAILABLE";
        case -1000710001: return "XR_ERROR_EXTENSION_DEPENDENCY_NOT_ENABLED";
        case -1000710000: return "XR_ERROR_PERMISSION_INSUFFICIENT";
        case -1000003000: return "XR_ERROR_ANDROID_THREAD_SETTINGS_ID_INVALID_KHR";
        case -1000003001: return "XR_ERROR_ANDROID_THREAD_SETTINGS_FAILURE_KHR";
        case -1000039001: return "XR_ERROR_CREATE_SPATIAL_ANCHOR_FAILED_MSFT";
        case -1000053000: return "XR_ERROR_SECONDARY_VIEW_CONFIGURATION_TYPE_NOT_ENABLED_MSFT";
        case -1000055000: return "XR_ERROR_CONTROLLER_MODEL_KEY_INVALID_MSFT";
        case -1000066000: return "XR_ERROR_REPROJECTION_MODE_UNSUPPORTED_MSFT";
        case -1000097000: return "XR_ERROR_COMPUTE_NEW_SCENE_NOT_COMPLETED_MSFT";
        case -1000097001: return "XR_ERROR_SCENE_COMPONENT_ID_INVALID_MSFT";
        case -1000097002: return "XR_ERROR_SCENE_COMPONENT_TYPE_MISMATCH_MSFT";
        case -1000097003: return "XR_ERROR_SCENE_MESH_BUFFER_ID_INVALID_MSFT";
        case -1000097004: return "XR_ERROR_SCENE_COMPUTE_FEATURE_INCOMPATIBLE_MSFT";
        case -1000097005: return "XR_ERROR_SCENE_COMPUTE_CONSISTENCY_MISMATCH_MSFT";
        case -1000101000: return "XR_ERROR_DISPLAY_REFRESH_RATE_UNSUPPORTED_FB";
        case -1000108000: return "XR_ERROR_COLOR_SPACE_UNSUPPORTED_FB";
        case -1000113000: return "XR_ERROR_SPACE_COMPONENT_NOT_SUPPORTED_FB";
        case -1000113001: return "XR_ERROR_SPACE_COMPONENT_NOT_ENABLED_FB";
        case -1000113002: return "XR_ERROR_SPACE_COMPONENT_STATUS_PENDING_FB";
        case -1000113003: return "XR_ERROR_SPACE_COMPONENT_STATUS_ALREADY_SET_FB";
        case -1000118000: return "XR_ERROR_UNEXPECTED_STATE_PASSTHROUGH_FB";
        case -1000118001: return "XR_ERROR_FEATURE_ALREADY_CREATED_PASSTHROUGH_FB";
        case -1000118002: return "XR_ERROR_FEATURE_REQUIRED_PASSTHROUGH_FB";
        case -1000118003: return "XR_ERROR_NOT_PERMITTED_PASSTHROUGH_FB";
        case -1000118004: return "XR_ERROR_INSUFFICIENT_RESOURCES_PASSTHROUGH_FB";
        case -1000118050: return "XR_ERROR_UNKNOWN_PASSTHROUGH_FB";
        case -1000119000: return "XR_ERROR_RENDER_MODEL_KEY_INVALID_FB";
        case 1000119020: return "XR_RENDER_MODEL_UNAVAILABLE_FB";
        case -1000124000: return "XR_ERROR_MARKER_NOT_TRACKED_VARJO";
        case -1000124001: return "XR_ERROR_MARKER_ID_INVALID_VARJO";
        case -1000138000: return "XR_ERROR_MARKER_DETECTOR_PERMISSION_DENIED_ML";
        case -1000138001: return "XR_ERROR_MARKER_DETECTOR_LOCATE_FAILED_ML";
        case -1000138002: return "XR_ERROR_MARKER_DETECTOR_INVALID_DATA_QUERY_ML";
        case -1000138003: return "XR_ERROR_MARKER_DETECTOR_INVALID_CREATE_INFO_ML";
        case -1000138004: return "XR_ERROR_MARKER_INVALID_ML";
        case -1000139000: return "XR_ERROR_LOCALIZATION_MAP_INCOMPATIBLE_ML";
        case -1000139001: return "XR_ERROR_LOCALIZATION_MAP_UNAVAILABLE_ML";
        case -1000139002: return "XR_ERROR_LOCALIZATION_MAP_FAIL_ML";
        case -1000139003: return "XR_ERROR_LOCALIZATION_MAP_IMPORT_EXPORT_PERMISSION_DENIED_ML";
        case -1000139004: return "XR_ERROR_LOCALIZATION_MAP_PERMISSION_DENIED_ML";
        case -1000139005: return "XR_ERROR_LOCALIZATION_MAP_ALREADY_EXISTS_ML";
        case -1000139006: return "XR_ERROR_LOCALIZATION_MAP_CANNOT_EXPORT_CLOUD_MAP_ML";
        case -1000142001: return "XR_ERROR_SPATIAL_ANCHOR_NAME_NOT_FOUND_MSFT";
        case -1000142002: return "XR_ERROR_SPATIAL_ANCHOR_NAME_INVALID_MSFT";
        case 1000147000: return "XR_SCENE_MARKER_DATA_NOT_STRING_MSFT";
        case -1000169000: return "XR_ERROR_SPACE_MAPPING_INSUFFICIENT_FB";
        case -1000169001: return "XR_ERROR_SPACE_LOCALIZATION_FAILED_FB";
        case -1000169002: return "XR_ERROR_SPACE_NETWORK_TIMEOUT_FB";
        case -1000169003: return "XR_ERROR_SPACE_NETWORK_REQUEST_FAILED_FB";
        case -1000169004: return "XR_ERROR_SPACE_CLOUD_STORAGE_DISABLED_FB";
        case -1000266000: return "XR_ERROR_PASSTHROUGH_COLOR_LUT_BUFFER_SIZE_MISMATCH_META";
        case 1000291000: return "XR_ENVIRONMENT_DEPTH_NOT_AVAILABLE_META";
        case -1000306000: return "XR_ERROR_HINT_ALREADY_SET_QCOM";
        case -1000319000: return "XR_ERROR_NOT_AN_ANCHOR_HTC";
        case -1000429000: return "XR_ERROR_SPACE_NOT_LOCATABLE_EXT";
        case -1000429001: return "XR_ERROR_PLANE_DETECTION_PERMISSION_DENIED_EXT";
        case -1000469001: return "XR_ERROR_FUTURE_PENDING_EXT";
        case -1000469002: return "XR_ERROR_FUTURE_INVALID_EXT";
        default:
            return std::format("XR_ERROR_UNKNOWN_{}", static_cast<int64_t>(res));
    }
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

static glm::mat4 create_projection_matrix(const XrFovf& fov, float z_near, float z_far, bool reverse_z)
{
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

    if (reverse_z) {
        projection[2][2] = z_near / (z_near - z_far);
        projection[2][3] = -1.0f;
        projection[3][2] = z_near;
    } else {
        projection[2][2] = -(z_far + z_near) / (z_far - z_near);
        projection[2][3] = -1.0f;
        projection[3][2] = -(2.0f * z_far * z_near) / (z_far - z_near);
    }

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

static void set_openrbrvr_api_layer_path()
{
    std::filesystem::path quad_views_path = std::filesystem::current_path() / "Plugins" / "openRBRVR" / "quad-views-foveated";

    constexpr auto env_buf_chars = 32767;
    constexpr auto env_buf_bytes = env_buf_chars * sizeof(wchar_t);
    wchar_t* env_buf = reinterpret_cast<wchar_t*>(malloc(env_buf_bytes));
    if (!env_buf) {
        throw std::runtime_error("Could not allocate buffer for environment variable");
    }
    memset(env_buf, 0, env_buf_bytes);

    auto env_var_len = GetEnvironmentVariableW(L"XR_API_LAYER_PATH", env_buf, 32767);
    bool env_var_ok = false;
    if (env_var_len > 0) {
        std::wstring wide_xr_api_layer_path = std::wstring(env_buf) + L";" + quad_views_path.c_str();
        env_var_ok = SetEnvironmentVariableW(L"XR_API_LAYER_PATH", wide_xr_api_layer_path.c_str());
    } else {
        env_var_ok = SetEnvironmentVariableW(L"XR_API_LAYER_PATH", quad_views_path.c_str());
    }
    free(env_buf);

    if (!env_var_ok) {
        MessageBoxA(nullptr, "Could not alter XR_API_LAYER_PATH environment variable", "OpenXR init error", MB_OK);
    }
}

OpenXR::OpenXR()
    : session()
    , reset_view_requested(false)
    , has_views(false)
{
    XrApplicationInfo appInfo = {
        .applicationName = "openRBRVR",
        .applicationVersion = 1,
        .engineName = "",
        .engineVersion = 0,
        .apiVersion = XR_CURRENT_API_VERSION,
    };

    auto extensions = std::vector { "XR_KHR_D3D11_enable" };
    std::vector<const char*> api_layers;

    if (!g::api_layer_search_path_fixed) {
        set_openrbrvr_api_layer_path();
        g::api_layer_search_path_fixed = true;
    }

    DWORD runtime_path_len = 0;
    auto reg_err = RegGetValue(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Khronos\\OpenXR\\1", "ActiveRuntime", RRF_RT_REG_SZ, nullptr, nullptr, &runtime_path_len);
    if (reg_err != ERROR_SUCCESS) {
        if (reg_err == ERROR_FILE_NOT_FOUND) {
            MessageBoxA(nullptr, "No 32-bit OpenXR runtime active.\nOpenXR initialization will likely fail.\nCheck the openRBRVR FAQ for runtime and device support.", "Error", MB_OK);
        } else {
            MessageBoxA(nullptr, std::format("Could not check the registry for runtime (error {}). OpenXR initialization may not succeed.", reg_err).c_str(), "Error", MB_OK);
        }
    } else if (runtime_path_len == 0) {
        MessageBoxA(nullptr, "No 32-bit OpenXR runtime active.\nOpenXR initialization will likely fail.\nCheck the openRBRVR FAQ for runtime and device support.", "Error", MB_OK);
    }

    uint32_t api_layer_count;
    if (auto err = xrEnumerateApiLayerProperties(0, &api_layer_count, nullptr); err != XR_SUCCESS) {
        throw std::runtime_error(std::format("OpenXR: Failed to enumerate API layers, error: {}", xr_result_to_str(err)));
    }

    std::vector<XrApiLayerProperties> available_api_layers(api_layer_count, { .type = XR_TYPE_API_LAYER_PROPERTIES });
    if (auto err = xrEnumerateApiLayerProperties(api_layer_count, &api_layer_count, available_api_layers.data()); err != XR_SUCCESS) {
        throw std::runtime_error(std::format("OpenXR: Failed to enumerate API layers, error: {}", xr_result_to_str(err)));
    }

    uint32_t extension_count;
    if (auto err = xrEnumerateInstanceExtensionProperties(nullptr, 0, &extension_count, nullptr); err != XR_SUCCESS) {
        throw std::runtime_error(std::format("OpenXR: Failed to enumerate extension properties, error: {}", xr_result_to_str(err)));
    }

    std::vector<XrExtensionProperties> available_extensions(extension_count, { .type = XR_TYPE_EXTENSION_PROPERTIES });
    if (auto err = xrEnumerateInstanceExtensionProperties(nullptr, extension_count, &extension_count, available_extensions.data()); err != XR_SUCCESS) {
        throw std::runtime_error(std::format("OpenXR: Failed to enumerate extension properties, error: {}", xr_result_to_str(err)));
    }

    if (auto ext = std::ranges::find_if(available_extensions, [](const XrExtensionProperties& p) {
            return std::string(p.extensionName) == "XR_KHR_win32_convert_performance_counter_time";
        });
        ext != available_extensions.cend()) {
        extensions.push_back(ext->extensionName);
    } else {
        // Prediction dampening requires "XR_KHR_win32_convert_performance_counter_time" extension
        // If it does not exist, revert dampening setting
        dbg("Prediction dampening not in use as XR_KHR_win32_convert_performance_counter_time extension is not present");
        g::cfg.prediction_dampening = 0;
    }

    if (g::cfg.quad_view_rendering) {
        auto quad_views_extension = std::ranges::find_if(available_extensions, [](const XrExtensionProperties& p) {
            return std::string(p.extensionName) == "XR_VARJO_quad_views";
        });
        auto foveated_rendering_extension = std::ranges::find_if(available_extensions, [](const XrExtensionProperties& p) {
            return std::string(p.extensionName) == "XR_VARJO_foveated_rendering";
        });

        if (quad_views_extension == available_extensions.cend()) {
            // No native quad view support, use Quad-Views-Foveated
            auto quad_views_layer = std::ranges::find_if(available_api_layers, [](const XrApiLayerProperties& p) {
                return std::string(p.layerName) == "XR_APILAYER_MBUCCHIA_quad_views_foveated";
            });
            if (quad_views_layer == available_api_layers.cend()) {
                MessageBoxA(nullptr, "Tried to enable quad view rendering but Quad-Views-Foveated API layer was not found.\nPlease make sure all files are installed and that you're not running the application as an admin.", "OpenXR layer init error", MB_OK);
                g::cfg.quad_view_rendering = false;
            } else {
                native_quad_views = false;
                extensions.push_back("XR_VARJO_quad_views");
                extensions.push_back("XR_VARJO_foveated_rendering");
                api_layers.push_back("XR_APILAYER_MBUCCHIA_quad_views_foveated");
            }
        } else {
            native_quad_views = true;
            extensions.push_back("XR_VARJO_quad_views");
            if (foveated_rendering_extension != available_extensions.cend()) {
                extensions.push_back("XR_VARJO_foveated_rendering");
            }
        }
    }

    if (g::cfg.openxr_motion_compensation) {
        auto motion_compensation_layer = std::ranges::find_if(available_api_layers, [](const XrApiLayerProperties& p) {
            return std::string(p.layerName) == "XR_APILAYER_NOVENDOR_motion_compensation";
        });
        if (motion_compensation_layer == available_api_layers.cend()) {
            MessageBoxA(nullptr, "Tried to enable motion compensation without OpenXR Motion Compensation API layer installed.\n\nPlease install the layer from:\nhttps://github.com/BuzzteeBear/OpenXR-MotionCompensation", "OpenXR layer init error", MB_OK);
            g::cfg.openxr_motion_compensation = false;
        }
    }

    XrInstanceCreateInfo instanceInfo = {
        .type = XR_TYPE_INSTANCE_CREATE_INFO,
        .next = nullptr,
        .createFlags = 0,
        .applicationInfo = appInfo,
        .enabledApiLayerCount = api_layers.size(),
        .enabledApiLayerNames = api_layers.data(),
        .enabledExtensionCount = extensions.size(),
        .enabledExtensionNames = extensions.data(),
    };

    if (auto err = xrCreateInstance(&instanceInfo, &instance); err != XR_SUCCESS) {
        if (err == XR_ERROR_EXTENSION_NOT_PRESENT) {
            throw std::runtime_error("xrCreateInstance failed: XR_ERROR_EXTENSION_NOT_PRESENT\nA requested extension is missing.");
        } else if (err == XR_ERROR_FILE_ACCESS_ERROR) {
            throw std::runtime_error("xrCreateInstance failed. XR_ERROR_FILE_ACCESS_ERROR\nMake sure Visual C++ redistributables (https://aka.ms/vs/17/release/vc_redist.x64.exe) are installed. Please try uninstalling Reshade if it is installed.");
        }
        throw std::runtime_error(std::format("Failed to initialize OpenXR. xrCreateInstance failed: {}", xr_result_to_str(err)));
    }

    XrSystemGetInfo system_get_info = {
        .type = XR_TYPE_SYSTEM_GET_INFO,
        .next = nullptr,
        .formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY,
    };
    if (auto err = xrGetSystem(instance, &system_get_info, &system_id); err != XR_SUCCESS) {
        if (err == XR_ERROR_FORM_FACTOR_UNAVAILABLE) {
            throw std::runtime_error(std::format("The OpenXR runtime did not detect the VR headset. Make sure it is plugged in and powered on."));
        }
        throw std::runtime_error(std::format("Failed to initialize OpenXR: xrGetSystem {}", XrResultToString(instance, err)));
    }

    try {
        xr_convert_win32_performance_counter_to_time = get_extension<PFN_xrConvertWin32PerformanceCounterToTimeKHR>(instance, "xrConvertWin32PerformanceCounterToTimeKHR");
    } catch (const std::runtime_error&) {
        dbg("Not using prediction dampening as xrConvertWin32PerformanceCounterToTimeKHR function was not found");
        xr_convert_win32_performance_counter_to_time = nullptr;
    }
}

void OpenXR::init(IDirect3DDevice9* dev, IDirect3DVR9** vrdev, uint32_t companion_window_width, uint32_t companion_window_height, std::optional<XrPosef> old_view_pose)
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

    XrGraphicsRequirementsD3D11KHR gfx_requirements = { .type = XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
    auto xrGetD3D11GraphicsRequirementsKHR = get_extension<PFN_xrGetD3D11GraphicsRequirementsKHR>(instance, "xrGetD3D11GraphicsRequirementsKHR");
    if (auto err = xrGetD3D11GraphicsRequirementsKHR(instance, system_id, &gfx_requirements); err != XR_SUCCESS) {
        throw std::runtime_error(std::format("Failed to initialize OpenXR: xrGetVulkanGraphicsRequirementsKHR {}", XrResultToString(instance, err)));
    }

    dbg(std::format("OpenXR D3D11 feature level {:x}", (int)gfx_requirements.minFeatureLevel));

    if (!g::d3d11_dev) {
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
    }

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

    uint32_t view_config_count;
    if (auto err = xrEnumerateViewConfigurations(instance, system_id, 0, &view_config_count, nullptr); err != XR_SUCCESS) {
        throw std::runtime_error("Failed to enumerate view configurations");
    }
    std::vector<XrViewConfigurationType> view_configs(view_config_count);
    if (auto err = xrEnumerateViewConfigurations(instance, system_id, view_config_count, &view_config_count, view_configs.data()); err != XR_SUCCESS) {
        throw std::runtime_error("Failed to enumerate view configurations");
    }

    // Store view config type for later use
    primary_view_config_type = g::cfg.quad_view_rendering ? view_configs.front() : XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

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
            .quad_view_rendering = gfx.second.quad_view_rendering,
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

            if (auto ret = g::d3d11_dev->CreateTexture2D(&desc, nullptr, &xr_ctx->shared_textures[i]); ret != D3D_OK) {
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

    view_pose = old_view_pose.value_or(XrPosef { { 0, 0, 0, 1 }, { 0, 0, 0 } });

    // Start pose registration.
    if (g::cfg.openxr_motion_compensation) {
        auto err = init_motion_compensation_support();
        if (err.has_value()) {
            MessageBoxA(nullptr, err.value().c_str(), "OpenXR motion compensation init error", MB_OK);
        }
    }

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

static std::optional<XrPath> xr_string_to_path(const XrInstance& instance, const std::string& path)
{
    XrPath ret;
    if (auto res = xrStringToPath(instance, path.c_str(), &ret); res != XR_SUCCESS) {
        dbg(std::format("xrStringToPath failed: {}", OpenXR::XrResultToString(instance, res)));
        return std::nullopt;
    }

    return ret;
}

std::optional<std::string> OpenXR::init_motion_compensation_support()
{
    XrActionSetCreateInfo actionSetInfo = {
        .type = XR_TYPE_ACTION_SET_CREATE_INFO,
        .next = nullptr,
        .actionSetName = "gameplay",
        .localizedActionSetName = "Gameplay",
        .priority = 0,
    };

    if (auto res = xrCreateActionSet(instance, &actionSetInfo, &input_state.action_set); res != XR_SUCCESS) {
        return std::format("Failed xrCreateActionSet: {}", XrResultToString(instance, res));
    }

    input_state.hand_subaction_path[Side::LEFT] = xr_string_to_path(instance, "/user/hand/left").value();
    input_state.hand_subaction_path[Side::RIGHT] = xr_string_to_path(instance, "/user/hand/right").value();

    // Create an input action getting the left and right hand poses.
    XrActionCreateInfo actionInfo = {
        .type = XR_TYPE_ACTION_CREATE_INFO,
        .next = nullptr,
        .actionName = "hand_pose",
        .actionType = XR_ACTION_TYPE_POSE_INPUT,
        .countSubactionPaths = static_cast<uint32_t>(input_state.hand_subaction_path.size()),
        .subactionPaths = input_state.hand_subaction_path.data(),
        .localizedActionName = "Hand Pose",
    };

    if (auto res = xrCreateAction(input_state.action_set, &actionInfo, &input_state.pose_action); res != XR_SUCCESS) {
        return std::format("Failed pose xrCreateAction: {}", XrResultToString(instance, res));
    }

    if (!set_interaction_profile_bindings()) {
        return "Failed to set interaction profile bindings";
    }

    XrActionSpaceCreateInfo actionSpaceInfo = {
        .type = XR_TYPE_ACTION_SPACE_CREATE_INFO,
        .next = nullptr,
        .action = input_state.pose_action,
        .subactionPath = input_state.hand_subaction_path[Side::LEFT],
        .poseInActionSpace = {
            .orientation = { .w = 1.0f },
        }
    };

    if (auto res = xrCreateActionSpace(session, &actionSpaceInfo, &input_state.hand_space[Side::LEFT]); res != XR_SUCCESS) {
        return std::format("xrCreateActionSpace: {}", XrResultToString(instance, res));
    }
    actionSpaceInfo.subactionPath = input_state.hand_subaction_path[Side::RIGHT];
    if (auto res = xrCreateActionSpace(session, &actionSpaceInfo, &input_state.hand_space[Side::RIGHT]); res != XR_SUCCESS) {
        return std::format("xrCreateActionSpace {}", XrResultToString(instance, res));
    }

    XrSessionActionSetsAttachInfo attachInfo = {
        .type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,
        .next = nullptr,
        .countActionSets = 1,
        .actionSets = &input_state.action_set,
    };

    if (auto res = xrAttachSessionActionSets(session, &attachInfo); res != XR_SUCCESS) {
        return std::format("xrAttachSessionActionSets: {}", XrResultToString(instance, res));
    }

    return std::nullopt;
}

std::vector<XrInteractionProfileSuggestedBinding> OpenXR::get_supported_interaction_profiles(const std::array<XrActionSuggestedBinding, 2>& bindings)
{
    const auto supported_profiles = {
        xr_string_to_path(instance, "/interaction_profiles/khr/simple_controller"),
        xr_string_to_path(instance, "/interaction_profiles/oculus/touch_controller"),
        xr_string_to_path(instance, "/interaction_profiles/htc/vive_controller"),
        xr_string_to_path(instance, "/interaction_profiles/valve/index_controller"),
        xr_string_to_path(instance, "/interaction_profiles/microsoft/motion_controller"),
    };

    return supported_profiles
        | std::views::filter([](const std::optional<XrPath>& p) { return p.has_value(); })
        | std::views::transform([&](const std::optional<XrPath>& p) {
              return XrInteractionProfileSuggestedBinding {
                  .type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
                  .interactionProfile = p.value(),
                  .countSuggestedBindings = static_cast<uint32_t>(bindings.size()),
                  .suggestedBindings = bindings.data(),
              };
          })
        | std::ranges::to<std::vector<XrInteractionProfileSuggestedBinding>>();
}

bool OpenXR::set_interaction_profile_bindings()
{
    const std::array<XrActionSuggestedBinding, 2> bindings = { {
        { .action = input_state.pose_action, .binding = xr_string_to_path(instance, "/user/hand/left/input/grip/pose").value() },
        { .action = input_state.pose_action, .binding = xr_string_to_path(instance, "/user/hand/right/input/grip/pose").value() },
    } };

    bool found_profile = false;
    for (const auto& profile : get_supported_interaction_profiles(bindings)) {
        // It is allowed to call this multiple times
        // If the subsequent calls success, the previous result is being overwritten
        auto res = xrSuggestInteractionProfileBindings(instance, &profile);
        if (!found_profile && res == XR_SUCCESS) {
            // If any of the profiles match, we're happy
            found_profile = true;
        }
    }

    if (!found_profile) {
        dbg("Could not find suggested bindings for any profile");
        return false;
    }

    return true;
}

void OpenXR::update_hand_poses()
{
    XrEventDataBuffer eventData = {
        .type = XR_TYPE_EVENT_DATA_BUFFER,
        .next = nullptr,
    };

    auto eventAvailable = xrPollEvent(instance, &eventData);
    if (eventAvailable == XR_SUCCESS) {
        dbg(std::format("xrPollEvent: {}", (int)eventData.type));
    } else if (eventAvailable == XR_EVENT_UNAVAILABLE) {
        return;
    } else {
        dbg(std::format("xrPollEvent failed: {}", XrResultToString(instance, eventAvailable)));
        return;
    }

    const XrActiveActionSet active_action_set = {
        .actionSet = input_state.action_set,
        .subactionPath = XR_NULL_PATH
    };
    XrActionsSyncInfo syncInfo = {
        .type = XR_TYPE_ACTIONS_SYNC_INFO,
        .next = nullptr,
        .countActiveActionSets = 1,
        .activeActionSets = &active_action_set,
    };

    if (auto res = xrSyncActions(session, &syncInfo); res != XR_SUCCESS) {
        dbg(std::format("xrSyncActions: {}", XrResultToString(instance, res)));
        return;
    }

    // This code has side effects in OpenXR motion compensation layer.
    // In order for it to work correctly, we need to call xrLocateSpace for the hand space.
    for (const auto hand : { Side::LEFT, Side::RIGHT }) {
        XrActionStateGetInfo get_info {
            .type = XR_TYPE_ACTION_STATE_GET_INFO,
            .action = input_state.pose_action,
            .subactionPath = input_state.hand_subaction_path[hand],
        };
        XrActionStatePose pose_state { .type = XR_TYPE_ACTION_STATE_POSE };
        if (auto res = xrGetActionStatePose(session, &get_info, &pose_state); res != XR_SUCCESS) {
            dbg(std::format("xrGetActionStatePose: {}", XrResultToString(instance, res)));
            continue;
        }
        if (pose_state.isActive) {
            XrSpaceLocation space_location { .type = XR_TYPE_SPACE_LOCATION };
            XrResult res = xrLocateSpace(input_state.hand_space[hand], space, frame_state.predictedDisplayTime, &space_location);
        }
    }
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

static void resolve_msaa(IDirect3DDevice9* dev, RenderContext* ctx, RenderTarget tgt)
{
    IDirect3DSurface9* surface;
    if (ctx->dx_texture[tgt]->GetSurfaceLevel(0, &surface) != D3D_OK) {
        dbg("Resolve MSAA: failed to get surface");
        return;
    }
    dev->StretchRect(ctx->dx_surface[tgt], nullptr, surface, nullptr, D3DTEXF_NONE);
    surface->Release();
}

void OpenXR::prepare_frames_for_hmd(IDirect3DDevice9* dev)
{
    const auto msaa_enabled = current_render_context->msaa != D3DMULTISAMPLE_NONE;
    const auto peripheral_msaa_enabled = g::cfg.quad_view_rendering && g::cfg.peripheral_msaa != D3DMULTISAMPLE_NONE;
    if (g::cfg.multiview) {
        IDirect3DSurface9 *left_eye, *right_eye;
        if (current_render_context->dx_texture[LeftEye]->GetSurfaceLevel(0, &left_eye) != D3D_OK) {
            dbg("Failed to get left eye surface");
            return;
        }
        if (current_render_context->dx_texture[RightEye]->GetSurfaceLevel(0, &right_eye) != D3D_OK) {
            dbg("Failed to get right eye surface");
            left_eye->Release();
            return;
        }
        IDirect3DSurface9* eyes[2] = { left_eye, right_eye };
        g::d3d_vr->CopySurfaceLayers(current_render_context->dx_surface[LeftEye], eyes, 2);
        left_eye->Release();
        right_eye->Release();

        if (g::cfg.quad_view_rendering) {
            if (current_render_context->dx_texture[FocusLeft]->GetSurfaceLevel(0, &left_eye) != D3D_OK) {
                dbg("Failed to get left eye surface");
                return;
            }
            if (current_render_context->dx_texture[FocusRight]->GetSurfaceLevel(0, &right_eye) != D3D_OK) {
                dbg("Failed to get right eye surface");
                left_eye->Release();
                return;
            }
            eyes[0] = left_eye;
            eyes[1] = right_eye;

            g::d3d_vr->CopySurfaceLayers(current_render_context->dx_surface[FocusLeft], eyes, 2);
            left_eye->Release();
            right_eye->Release();
        }
    } else {
        resolve_msaa(dev, current_render_context, LeftEye);
        resolve_msaa(dev, current_render_context, RightEye);
        if (g::cfg.quad_view_rendering) {
            resolve_msaa(dev, current_render_context, FocusLeft);
            resolve_msaa(dev, current_render_context, FocusRight);
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

        // Make sure D3D11 side of work is done before displaying the textures to the HMD
        g::d3d11_ctx->Flush();

        xrReleaseSwapchainImage(xr_context()->swapchains[FocusLeft], &release_info);
        xrReleaseSwapchainImage(xr_context()->swapchains[FocusRight], &release_info);
    } else {
        // Make sure D3D11 side of work is done before displaying the textures to the HMD
        g::d3d11_ctx->Flush();
    }

    xrReleaseSwapchainImage(xr_context()->swapchains[LeftEye], &release_info);
    xrReleaseSwapchainImage(xr_context()->swapchains[RightEye], &release_info);

    if (g::cfg.debug && perf_query_free_to_use) [[unlikely]] {
        gpu_end_query->Issue(D3DISSUE_END);
        gpu_disjoint_query->Issue(D3DISSUE_END);
    }
}

void OpenXR::synchronize_graphics_apis(bool wait_for_cpu)
{
    cross_api_fence.value += 1;

    g::d3d_vr->Flush();
    g::d3d_vr->LockSubmissionQueue();
    g::d3d_vr->SignalFence(cross_api_fence.value);
    g::d3d_vr->UnlockSubmissionQueue();

    if (wait_for_cpu) [[unlikely]] {
        HANDLE evt = CreateEventEx(nullptr, "Flush event", 0, EVENT_ALL_ACCESS);
        if (evt != INVALID_HANDLE_VALUE) {
            cross_api_fence.fence->SetEventOnCompletion(cross_api_fence.value, evt);
            if (auto ret = g::d3d11_ctx->Wait(cross_api_fence.fence, cross_api_fence.value); ret != D3D_OK) {
                dbg(std::format("Failed to wait shared semaphore: {}", ret));
            }
            WaitForSingleObject(evt, INFINITE);
        }
    } else {
        if (auto ret = g::d3d11_ctx->Wait(cross_api_fence.fence, cross_api_fence.value); ret != D3D_OK) {
            dbg(std::format("Failed to wait shared semaphore: {}", ret));
        }
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
        for (size_t i = 0; i < xr_context()->views.size(); i += 2) {
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
    if (g::cfg.openxr_motion_compensation) {
        update_hand_poses();
    }

    frame_state = {
        .type = XR_TYPE_FRAME_STATE,
        .next = nullptr,
    };

    if (auto res = xrWaitFrame(session, nullptr, &frame_state); res != XR_SUCCESS) {
        dbg(std::format("xrWaitFrame: {}", XrResultToString(instance, res)));
        return false;
    }

    if (g::cfg.prediction_dampening > 0 && xr_convert_win32_performance_counter_to_time) {
        // Ported from:
        // https://github.com/mbucchia/OpenXR-Toolkit/blob/main/XR_APILAYER_MBUCCHIA_toolkit/layer.cpp#L2175-L2191
        // MIT License
        // Copyright (c) 2021-2022 Matthieu Bucchianeri
        // Copyright (c) 2021-2022 Jean-Luc Dupiot - Reality XP

        LARGE_INTEGER pc_now;
        QueryPerformanceCounter(&pc_now);

        XrTime xr_now;
        xr_convert_win32_performance_counter_to_time(instance, &pc_now, &xr_now);

        XrTime prediction_amount = frame_state.predictedDisplayTime - xr_now;
        if (prediction_amount > 0) {
            frame_state.predictedDisplayTime = xr_now + ((100 - g::cfg.prediction_dampening) * prediction_amount) / 100;
        }
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

    get_projection_matrix();

    update_poses();

    if (g::cfg.debug && perf_query_free_to_use) [[unlikely]] {
        gpu_disjoint_query->Issue(D3DISSUE_BEGIN);
        gpu_start_query->Issue(D3DISSUE_END);
    }

    return true;
}

bool OpenXR::get_projection_matrix()
{
    if (!has_views) {
        auto view_state = update_views();
        if (!view_state) {
            dbg("Failed to update OpenXR views");
            return false;
        }
        has_views = true;
    }

    const auto view_count = xr_context()->views.size();
    for (size_t i = 0; i < view_count; ++i) {
        projection[i] = create_projection_matrix(xr_context()->views[i].fov, z_near, z_far, rbr::should_use_reverse_z_buffer());
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
    synchronize_graphics_apis(true);
    g::d3d_vr->WaitDeviceIdle(true);
    cross_api_fence.fence->Release();

    if (g::cfg.openxr_motion_compensation) {
        // TODO: More cleanup?
        if (input_state.action_set != XR_NULL_HANDLE) {
            xrDestroyActionSet(input_state.action_set);
        }
    }
    xrDestroySpace(space);
    xrDestroySpace(view_space);

    for (const auto& v : render_contexts) {
        const auto& ctx = v.second;

        for (int i = 0; i < 6; ++i) {
            if (ctx.dx_texture[i]) {
                ctx.dx_texture[i]->Release();
            }
            if (ctx.dx_surface[i]) {
                ctx.dx_surface[i]->Release();
            }
            if (ctx.dx_depth_stencil_surface[i]) {
                ctx.dx_depth_stencil_surface[i]->Release();
            }
            if (i < 4 && ctx.dx_shared_handle[i] != nullptr && ctx.dx_shared_handle != INVALID_HANDLE_VALUE) {
                CloseHandle(ctx.dx_shared_handle[i]);
            }
        }

        auto xr_ctx = reinterpret_cast<OpenXRRenderContext*>(ctx.ext);
        for (size_t i = 0; i < xr_ctx->swapchains.size(); ++i) {
            xrDestroySwapchain(xr_ctx->swapchains[i]);
            xr_ctx->shared_textures[i]->Release();
        }
    }

    xrDestroySession(session);
    xrDestroyInstance(instance);

    gpu_start_query->Release();
    gpu_end_query->Release();
    gpu_freq_query->Release();
    gpu_disjoint_query->Release();
}
