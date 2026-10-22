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
    bool supported() const { return supported_; }
    bool enabled() const { return view_space_ && hands_[0].tracker && hands_[1].tracker; }
    const std::array<Hand, 2>& hands() const { return hands_; }
    std::string status() const;

private:
    void destroy_trackers();
    void log_status();
    std::chrono::steady_clock::time_point last_status_log_ {};
    bool supported_ = false;
    bool data_source_enabled_ = false;
    bool creation_attempted_ = false;
    XrSpace view_space_ = XR_NULL_HANDLE;
    XrResult reference_result_ = XR_SUCCESS;
    XrSpaceLocationFlags reference_flags_ = 0;
    std::array<Hand, 2> hands_ {};
    PFN_xrCreateHandTrackerEXT create_ = nullptr;
    PFN_xrDestroyHandTrackerEXT destroy_ = nullptr;
    PFN_xrLocateHandJointsEXT locate_ = nullptr;
    PFN_xrCreateReferenceSpace create_space_ = nullptr;
    PFN_xrDestroySpace destroy_space_ = nullptr;
    PFN_xrLocateSpace locate_space_ = nullptr;
};
