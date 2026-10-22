#pragma once

#include <array>
#include <chrono>
#include <openxr.h>
#include <string>

// Independent of the controller actions used by motion compensation.
class HandTracking {
public:
    enum class Status { NotLocated,
        Inactive,
        LocateFailed,
        ControllerSource,
        InvalidReference,
        InvalidJoints,
        Tracked };
    struct Hand {
        XrHandTrackerEXT tracker = XR_NULL_HANDLE;
        std::array<XrHandJointLocationEXT, XR_HAND_JOINT_COUNT_EXT> joints {};
        bool active = false;
        bool locate_error_reported = false;
        Status status = Status::NotLocated;
        XrResult locate_result = XR_SUCCESS;
        int invalid_joint = -1;
    };

    void initialize(XrInstance instance, XrSystemId system, bool extension_enabled, bool data_source_enabled);
    // Locate joints in an identity VIEW space, then convert through the VIEW
    // pose returned by the API layer into the current rendering reference space.
    void update(XrSession session, XrSpace reference_space, XrTime time, bool enabled);
    void hide();
    void shutdown();
    bool supported() const { return tracking_supported; }
    bool enabled() const { return view_space && hand_states[0].tracker && hand_states[1].tracker; }
    const std::array<Hand, 2>& hands() const { return hand_states; }
    std::string status() const;

private:
    void destroy_trackers();
    void log_status();
    std::chrono::steady_clock::time_point last_status_log {};
    bool tracking_supported = false;
    bool data_source_enabled = false;
    bool creation_attempted = false;
    XrSpace view_space = XR_NULL_HANDLE;
    XrResult reference_result = XR_SUCCESS;
    XrSpaceLocationFlags reference_flags = 0;
    std::array<Hand, 2> hand_states {};
    PFN_xrCreateHandTrackerEXT create = nullptr;
    PFN_xrDestroyHandTrackerEXT destroy = nullptr;
    PFN_xrLocateHandJointsEXT locate = nullptr;
    PFN_xrCreateReferenceSpace create_space = nullptr;
    PFN_xrDestroySpace destroy_space = nullptr;
    PFN_xrLocateSpace locate_space = nullptr;
};
