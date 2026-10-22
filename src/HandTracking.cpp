#include "HandTracking.hpp"
#include "HandDiagnostics.hpp"
#include "Util.hpp"
#include <algorithm>
#include <cmath>

namespace {
    bool valid_pose(const XrPosef& pose)
    {
        const auto& p = pose.position;
        const auto& q = pose.orientation;
        const float norm = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
        return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z)
            && std::isfinite(norm) && norm > 0.5f && norm < 1.5f;
    }
}

void HandTracking::initialize(XrInstance instance, XrSystemId system, bool extension_enabled, bool data_source_enabled)
{
    shutdown();
    if (!extension_enabled) {
        hand_log("Hand tracking unavailable: XR_EXT_hand_tracking not exposed by the runtime");
        return;
    }

    XrSystemHandTrackingPropertiesEXT hand_properties { XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT };
    XrSystemProperties properties { XR_TYPE_SYSTEM_PROPERTIES, &hand_properties };
    const auto result = xrGetSystemProperties(instance, system, &properties);
    if (XR_FAILED(result) || !hand_properties.supportsHandTracking) {
        hand_log(std::format("Hand tracking unavailable: system query result={}, supportsHandTracking={}", static_cast<int>(result), hand_properties.supportsHandTracking));
        return;
    }

    const auto load = [instance](const char* name, auto& fn) {
        return XR_SUCCEEDED(xrGetInstanceProcAddr(instance, name, reinterpret_cast<PFN_xrVoidFunction*>(&fn))) && fn;
    };
    if (!load("xrCreateHandTrackerEXT", create_) || !load("xrDestroyHandTrackerEXT", destroy_) || !load("xrLocateHandJointsEXT", locate_)
        || !load("xrCreateReferenceSpace", create_space_) || !load("xrDestroySpace", destroy_space_) || !load("xrLocateSpace", locate_space_)) {
        hand_log("Hand tracking unavailable: required extension functions could not be loaded");
        return;
    }

    supported_ = true;
    data_source_enabled_ = data_source_enabled;
    hand_log(std::format("Hand tracking supported; data source selection={}", data_source_enabled_));
}

void HandTracking::hide()
{
    reference_flags_ = 0;
    reference_result_ = XR_SUCCESS;
    for (auto& hand : hands_) {
        hand.active = false;
        hand.joints = {};
        hand.status = Status::NotLocated;
        hand.invalid_joint = -1;
    }
}

void HandTracking::destroy_trackers()
{
    hide();
    for (auto& hand : hands_) {
        if (hand.tracker && destroy_) {
            destroy_(hand.tracker);
        }
        hand = {};
    }
    if (view_space_ && destroy_space_) {
        destroy_space_(view_space_);
    }
    view_space_ = XR_NULL_HANDLE;
}

void HandTracking::shutdown()
{
    destroy_trackers();
    supported_ = false;
    creation_attempted_ = false;
    data_source_enabled_ = false;
    create_ = nullptr;
    destroy_ = nullptr;
    locate_ = nullptr;
    create_space_ = nullptr;
    destroy_space_ = nullptr;
    locate_space_ = nullptr;
}

void HandTracking::update(XrSession session, XrSpace reference_space, XrTime time, bool enabled)
{
    hide();
    if (!enabled) {
        destroy_trackers();
        creation_attempted_ = false;
        return;
    }
    if (!supported_ || !session || !reference_space || time <= 0) {
        return;
    }

    if (!creation_attempted_) {
        creation_attempted_ = true;
        const XrReferenceSpaceCreateInfo view_info {
            XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr, XR_REFERENCE_SPACE_TYPE_VIEW,
            { { 0, 0, 0, 1 }, { 0, 0, 0 } }
        };
        if (const auto result = create_space_(session, &view_info, &view_space_); XR_FAILED(result)) {
            view_space_ = XR_NULL_HANDLE;
            hand_log(std::format("Hand VIEW space creation failed ({}); hands hidden until toggled or restarted", static_cast<int>(result)));
            return;
        }
        XrHandTrackingDataSourceEXT source = XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT;
        XrHandTrackingDataSourceInfoEXT source_info { XR_TYPE_HAND_TRACKING_DATA_SOURCE_INFO_EXT, nullptr, 1, &source };
        for (size_t i = 0; i < hands_.size(); ++i) {
            const XrHandTrackerCreateInfoEXT info {
                XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT,
                data_source_enabled_ ? &source_info : nullptr,
                i == 0 ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT,
                XR_HAND_JOINT_SET_DEFAULT_EXT,
            };
            XrHandTrackerEXT tracker = XR_NULL_HANDLE;
            const auto result = create_(session, &info, &tracker);
            if (XR_FAILED(result)) {
                hand_log(std::format("Hand tracker creation failed ({}); hands hidden until toggled or restarted", static_cast<int>(result)));
                destroy_trackers();
                return;
            }
            hands_[i].tracker = tracker;
        }
        hand_log("Left and right hand trackers created; joints use VIEW -> rendering reference conversion (automatic motion compensation)");
    }
    if (!this->enabled()) {
        return;
    }

    // Motion-compensation layers correct this head pose, while older layers
    // leave xrLocateHandJointsEXT untouched. Head-relative joints multiplied
    // by this pose inherit the correction without knowing rig motion:
    // (C * H) * (inverse(H) * J) = C * J. With C=identity this is just J.
    // Use an identity VIEW space, independent of seat/recenter offsets, and
    // the exact same time for this pose and both hands' joint queries.
    XrSpaceLocation reference { XR_TYPE_SPACE_LOCATION };
    reference_result_ = locate_space_(view_space_, reference_space, time, &reference);
    reference_flags_ = reference.locationFlags;
    constexpr auto valid = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    if (XR_FAILED(reference_result_) || (reference_flags_ & valid) != valid || !valid_pose(reference.pose)) {
        for (auto& hand : hands_)
            hand.status = Status::InvalidReference;
        log_status();
        return;
    }
    const auto& rq = reference.pose.orientation;
    const auto reference_rotation = glm::normalize(glm::quat(rq.w, rq.x, rq.y, rq.z));
    const glm::vec3 reference_position(reference.pose.position.x, reference.pose.position.y, reference.pose.position.z);
    const XrHandJointsLocateInfoEXT info { XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT, nullptr, view_space_, time };
    for (auto& hand : hands_) {
        XrHandTrackingDataSourceStateEXT source { XR_TYPE_HAND_TRACKING_DATA_SOURCE_STATE_EXT };
        XrHandJointLocationsEXT locations {
            XR_TYPE_HAND_JOINT_LOCATIONS_EXT,
            data_source_enabled_ ? &source : nullptr,
            XR_FALSE,
            XR_HAND_JOINT_COUNT_EXT,
            hand.joints.data(),
        };
        const auto result = locate_(hand.tracker, &info, &locations);
        hand.locate_result = result;
        if (XR_FAILED(result)) {
            hand.status = Status::LocateFailed;
            if (!hand.locate_error_reported) {
                hand_log(std::format("Hand joint location failed ({}); hand hidden", static_cast<int>(result)));
                hand.locate_error_reported = true;
            }
            continue;
        }
        if (!locations.isActive || (data_source_enabled_ && !source.isActive)) {
            hand.status = Status::Inactive;
            continue;
        }
        if (data_source_enabled_ && source.dataSource != XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT) {
            hand.status = Status::ControllerSource;
            continue;
        }

        // isActive and valid joints are the visibility contract. Do not require
        // TRACKED bits: older Meta runtimes do not set them reliably for hands.
        const auto invalid = std::ranges::find_if_not(hand.joints, [](const auto& joint) {
            return (joint.locationFlags & valid) == valid
                && valid_pose(joint.pose)
                && std::isfinite(joint.radius) && joint.radius >= 0.0f && joint.radius < 0.1f;
        });
        hand.active = invalid == hand.joints.end();
        hand.status = hand.active ? Status::Tracked : Status::InvalidJoints;
        if (!hand.active)
            hand.invalid_joint = static_cast<int>(invalid - hand.joints.begin());
        if (hand.active) {
            for (size_t i = 0; i < hand.joints.size(); ++i) {
                auto& pose = hand.joints[i].pose;
                const auto position = reference_position + reference_rotation * glm::vec3(pose.position.x, pose.position.y, pose.position.z);
                const auto& q = pose.orientation;
                const auto orientation = glm::normalize(reference_rotation * glm::normalize(glm::quat(q.w, q.x, q.y, q.z)));
                pose = { { orientation.x, orientation.y, orientation.z, orientation.w }, { position.x, position.y, position.z } };
                if (!valid_pose(pose)) {
                    hand.active = false;
                    hand.status = Status::InvalidJoints;
                    hand.invalid_joint = static_cast<int>(i);
                    break;
                }
            }
        }
    }
    log_status();
}

std::string HandTracking::status() const
{
    if (!supported_)
        return "Unavailable";
    if (!enabled())
        return creation_attempted_ ? "Tracker creation failed" : "Trackers off";
    const auto name = [](Status status) -> const char* {
        switch (status) {
            case Status::NotLocated: return "not located";
            case Status::Inactive: return "inactive";
            case Status::LocateFailed: return "locate failed";
            case Status::ControllerSource: return "controller source";
            case Status::InvalidReference: return "invalid head pose";
            case Status::InvalidJoints: return "invalid joints";
            case Status::Tracked: return "tracked";
        }
        return "unknown";
    };
    return std::format("L: {}, R: {}", name(hands_[0].status), name(hands_[1].status));
}

void HandTracking::log_status()
{
    const auto now = std::chrono::steady_clock::now();
    if (now - last_status_log_ < std::chrono::seconds(5))
        return;
    last_status_log_ = now;
    hand_log(std::format("Hand joints: {}; locate results L={}, R={}; coordinates=VIEW -> LOCAL, headLocateResult={}, headFlags={}",
        status(), static_cast<int>(hands_[0].locate_result), static_cast<int>(hands_[1].locate_result),
        static_cast<int>(reference_result_), reference_flags_));
    for (size_t i = 0; i < hands_.size(); ++i) {
        const auto& hand = hands_[i];
        if (hand.invalid_joint >= 0) {
            const auto& joint = hand.joints[hand.invalid_joint];
            const auto& q = joint.pose.orientation;
            hand_log(std::format("{} hand invalid joint {}: flags={}, radius={}, orientation norm={}",
                i == 0 ? "Left" : "Right", hand.invalid_joint, joint.locationFlags, joint.radius,
                q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w));
        }
    }
}
