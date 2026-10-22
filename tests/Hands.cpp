#include "Config.hpp"
#include "D3DSceneScope.hpp"
#include "HandButtonLayout.hpp"
#include "HandMenu.hpp"
#include "HandMesh.hpp"
#include "HandTexture.hpp"
#include "Hook.hpp"
#include "IgnitionButton.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::cerr << "Failed: " #condition " at line " << __LINE__ << '\n'; \
            std::exit(1);                                                       \
        }                                                                       \
    } while (false)

namespace HookTest {
    using Fn = int(__cdecl*)(int);
    Hook<Fn>* installed = nullptr;
    Fn detour = nullptr;
    bool create_failure = false, enable_failure = false;
    int entries = 0, removals = 0;
    int __cdecl original(int value) { return value + 1; }
    int __cdecl replacement(int value)
    {
        CHECK(installed && installed->call == original);
        CHECK(installed->src == original);
        ++entries;
        return installed->call(value) + 1;
    }
}

extern "C" MH_STATUS WINAPI MH_CreateHook(LPVOID source, LPVOID target, LPVOID* original)
{
    CHECK(source == reinterpret_cast<void*>(HookTest::original));
    if (HookTest::create_failure)
        return MH_ERROR_UNSUPPORTED_FUNCTION;
    *original = source;
    HookTest::detour = reinterpret_cast<HookTest::Fn>(target);
    return MH_OK;
}
extern "C" MH_STATUS WINAPI MH_EnableHook(LPVOID source)
{
    CHECK(source == reinterpret_cast<void*>(HookTest::original));
    if (HookTest::enable_failure)
        return MH_ERROR_MEMORY_PROTECT;
    // Worst-case scheduling: the physics thread enters the detour before
    // enabling returns, not after a temporary hook is moved into its owner.
    CHECK(HookTest::detour(40) == 42);
    return MH_OK;
}
extern "C" MH_STATUS WINAPI MH_DisableHook(LPVOID) { return MH_OK; }
extern "C" MH_STATUS WINAPI MH_RemoveHook(LPVOID)
{
    ++HookTest::removals;
    return MH_OK;
}

namespace {
    const auto instance = static_cast<XrInstance>(1);
    const auto session = static_cast<XrSession>(2);
    const auto space = static_cast<XrSpace>(3);
    const auto hand_view_space = static_cast<XrSpace>(5);
    struct Mock {
        bool supports = true;
        bool missing_function = false;
        bool property_failure = false;
        int create_failure = 0;
        int queries = 0;
        int creates = 0;
        int destroys = 0;
        int locates = 0;
        int space_creates = 0;
        int space_destroys = 0;
        int space_locates = 0;
        bool space_create_failure = false;
        bool space_locate_failure = false;
        XrSpaceLocationFlags head_flags = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        XrPosef raw_head { { 0, 0, 0, 1 }, { 0, 0, 0 } };
        XrPosef compensated_head { { 0, 0, 0, 1 }, { 0, 0, 0 } };
        XrPosef joint_motion { { 0, 0, 0, 1 }, { 0, 0, 0 } };
        XrSpace reference_space = XR_NULL_HANDLE;
        XrTime head_time = 0;
        std::array<bool, 2> active { true, true };
        std::array<bool, 2> locate_failure {};
        int invalid_joint = -1;
        bool nonfinite_position = false;
        bool invalid_orientation = false;
        bool invalid_radius = false;
        bool controller_source = false;
        bool source_active = true;
        bool source_requested = false;
        XrTime time = 0;
        XrSpace space = XR_NULL_HANDLE;
    } mock;

    M4 pose_matrix(const XrPosef& pose)
    {
        auto result = glm::mat4_cast(glm::quat(pose.orientation.w, pose.orientation.x, pose.orientation.y, pose.orientation.z));
        result[3] = { pose.position.x, pose.position.y, pose.position.z, 1 };
        return result;
    }

    XrPosef matrix_pose(const M4& matrix)
    {
        const auto q = glm::normalize(glm::quat_cast(matrix));
        return { { q.x, q.y, q.z, q.w }, { matrix[3].x, matrix[3].y, matrix[3].z } };
    }

    void check_pose(const XrPosef& actual, const XrPosef& expected)
    {
        const auto a = pose_matrix(actual);
        const auto b = pose_matrix(expected);
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                CHECK(std::abs(a[column][row] - b[column][row]) < 0.00001f);
    }

    void fill_joints(XrHandJointLocationEXT* joints, int hand)
    {
        const float x = hand == 0 ? -0.16f : 0.16f;
        for (int i = 0; i < XR_HAND_JOINT_COUNT_EXT; ++i) {
            joints[i] = {
                XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT,
                { { 0, 0, 0, 1 }, { x, -0.15f, -0.4f } }, 0.008f
            };
        }
        joints[XR_HAND_JOINT_PALM_EXT].radius = 0.03f;
        joints[XR_HAND_JOINT_WRIST_EXT].pose.position.z = -0.36f;
        for (int finger = 0; finger < 5; ++finger) {
            const int start = finger == 0 ? 2 : 6 + (finger - 1) * 5;
            const int count = finger == 0 ? 4 : 5;
            for (int i = 0; i < count; ++i) {
                joints[start + i].pose.position = { x + (finger - 2) * 0.018f, -0.15f, -0.39f - 0.025f * i };
            }
        }
    }

    XrResult XRAPI_CALL create_tracker(XrSession actual_session, const XrHandTrackerCreateInfoEXT* info, XrHandTrackerEXT* tracker)
    {
        CHECK(actual_session == session);
        CHECK(info->type == XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT);
        CHECK(info->handJointSet == XR_HAND_JOINT_SET_DEFAULT_EXT);
        ++mock.creates;
        if (mock.creates == mock.create_failure)
            return XR_ERROR_FEATURE_UNSUPPORTED;
        if (info->next) {
            const auto& source = *static_cast<const XrHandTrackingDataSourceInfoEXT*>(info->next);
            CHECK(source.type == XR_TYPE_HAND_TRACKING_DATA_SOURCE_INFO_EXT);
            CHECK(source.requestedDataSourceCount == 1);
            CHECK(source.requestedDataSources[0] == XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT);
            mock.source_requested = true;
        }
        *tracker = static_cast<XrHandTrackerEXT>(info->hand == XR_HAND_LEFT_EXT ? 10 : 11);
        return XR_SUCCESS;
    }

    XrResult XRAPI_CALL destroy_tracker(XrHandTrackerEXT tracker)
    {
        CHECK(tracker == static_cast<XrHandTrackerEXT>(10) || tracker == static_cast<XrHandTrackerEXT>(11));
        ++mock.destroys;
        return XR_SUCCESS;
    }

    XrResult XRAPI_CALL create_view_space(XrSession actual_session, const XrReferenceSpaceCreateInfo* info, XrSpace* result)
    {
        CHECK(actual_session == session && info->type == XR_TYPE_REFERENCE_SPACE_CREATE_INFO);
        CHECK(info->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_VIEW);
        check_pose(info->poseInReferenceSpace, XrPosef { { 0, 0, 0, 1 }, { 0, 0, 0 } });
        ++mock.space_creates;
        if (mock.space_create_failure)
            return XR_ERROR_RUNTIME_FAILURE;
        *result = hand_view_space;
        return XR_SUCCESS;
    }

    XrResult XRAPI_CALL destroy_view_space(XrSpace actual_space)
    {
        CHECK(actual_space == hand_view_space);
        ++mock.space_destroys;
        return XR_SUCCESS;
    }

    XrResult XRAPI_CALL locate_view_space(XrSpace actual_space, XrSpace reference, XrTime time, XrSpaceLocation* location)
    {
        CHECK(actual_space == hand_view_space && location->type == XR_TYPE_SPACE_LOCATION);
        ++mock.space_locates;
        mock.reference_space = reference;
        mock.head_time = time;
        if (mock.space_locate_failure)
            return XR_ERROR_RUNTIME_FAILURE;
        location->pose = mock.compensated_head;
        location->locationFlags = mock.head_flags;
        return XR_SUCCESS;
    }

    XrResult XRAPI_CALL locate_joints(XrHandTrackerEXT tracker, const XrHandJointsLocateInfoEXT* info, XrHandJointLocationsEXT* locations)
    {
        const int hand = tracker == static_cast<XrHandTrackerEXT>(10) ? 0 : 1;
        CHECK(info->type == XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT);
        CHECK(locations->type == XR_TYPE_HAND_JOINT_LOCATIONS_EXT);
        CHECK(locations->jointCount == XR_HAND_JOINT_COUNT_EXT);
        CHECK(info->baseSpace == hand_view_space && info->time == mock.head_time);
        ++mock.locates;
        mock.time = info->time;
        mock.space = info->baseSpace;
        if (mock.locate_failure[hand])
            return XR_ERROR_RUNTIME_FAILURE;
        fill_joints(locations->jointLocations, hand);
        // The runtime sees physical poses. API-layer compensation affects the
        // head pose above, but not these head-relative hand-joint results.
        const auto from_world = glm::inverse(pose_matrix(mock.raw_head)) * pose_matrix(mock.joint_motion);
        for (size_t i = 0; i < locations->jointCount; ++i)
            locations->jointLocations[i].pose = matrix_pose(from_world * pose_matrix(locations->jointLocations[i].pose));
        locations->isActive = mock.active[hand];
        if (mock.invalid_joint >= 0)
            locations->jointLocations[mock.invalid_joint].locationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        if (mock.nonfinite_position)
            locations->jointLocations[0].pose.position.x = std::numeric_limits<float>::quiet_NaN();
        if (mock.invalid_orientation)
            locations->jointLocations[0].pose.orientation = {};
        if (mock.invalid_radius)
            locations->jointLocations[0].radius = std::numeric_limits<float>::infinity();
        for (auto* item = static_cast<XrBaseOutStructure*>(locations->next); item; item = item->next) {
            CHECK(item->type == XR_TYPE_HAND_TRACKING_DATA_SOURCE_STATE_EXT);
            auto& source = *reinterpret_cast<XrHandTrackingDataSourceStateEXT*>(item);
            source.isActive = mock.source_active;
            source.dataSource = mock.controller_source ? XR_HAND_TRACKING_DATA_SOURCE_CONTROLLER_EXT : XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT;
        }
        return XR_SUCCESS;
    }

    void update(HandTracking& hands, bool enabled = true)
    {
        hands.update(session, space, 123456, enabled);
    }

    void test_capabilities()
    {
        HandTracking hands;
        mock = {};
        hands.initialize(instance, 1, false, false);
        update(hands);
        CHECK(!hands.supported() && !hands.enabled() && mock.queries == 0 && mock.creates == 0);
        mock.supports = false;
        hands.initialize(instance, 1, true, false);
        update(hands);
        CHECK(!hands.supported() && mock.creates == 0);
        mock = {};
        mock.property_failure = true;
        hands.initialize(instance, 1, true, false);
        CHECK(!hands.supported());
        mock = {};
        mock.missing_function = true;
        hands.initialize(instance, 1, true, false);
        update(hands);
        CHECK(!hands.supported() && mock.creates == 0);
    }

    void test_lifecycle()
    {
        mock = {};
        HandTracking hands;
        hands.initialize(instance, 1, true, false);
        CHECK(hands.supported());
        hands.update(XR_NULL_HANDLE, space, 123456, true);
        hands.update(session, XR_NULL_HANDLE, 123456, true);
        hands.update(session, space, 0, true);
        CHECK(mock.creates == 0);
        mock.create_failure = 2;
        update(hands);
        CHECK(!hands.enabled() && mock.creates == 2 && mock.destroys == 1);
        CHECK(mock.space_creates == 1 && mock.space_destroys == 1);
        update(hands);
        CHECK(mock.creates == 2); // Optional failures must not retry/log each frame.
        update(hands, false);
        mock.create_failure = 0;
        update(hands);
        CHECK(hands.enabled() && mock.creates == 4);
        update(hands, false);
        CHECK(!hands.enabled() && mock.destroys == 3 && !hands.hands()[0].active);
        update(hands);
        CHECK(hands.enabled() && mock.creates == 6);
        hands.shutdown();
        CHECK(mock.destroys == 5 && !hands.supported() && !hands.enabled());
        CHECK(mock.space_creates == 3 && mock.space_destroys == 3);
        hands.shutdown();
        CHECK(mock.destroys == 5);
    }

    void test_visibility()
    {
        mock = {};
        HandTracking hands;
        hands.initialize(instance, 1, true, true);
        update(hands);
        CHECK(mock.source_requested && mock.time == 123456 && mock.space == hand_view_space);
        CHECK(mock.reference_space == space && mock.head_time == mock.time);
        CHECK(hands.hands()[0].active && hands.hands()[1].active);
        CHECK(hands.status() == "L: tracked, R: tracked");
        mock.active[0] = false;
        update(hands);
        CHECK(!hands.hands()[0].active && hands.hands()[1].active);
        CHECK(hands.status() == "L: inactive, R: tracked");
        mock.active[0] = true;
        update(hands);
        CHECK(hands.hands()[0].active && mock.creates == 2);
        mock.locate_failure[1] = true;
        update(hands);
        CHECK(hands.hands()[0].active && !hands.hands()[1].active);
        mock.locate_failure[1] = false;
        update(hands);
        CHECK(hands.hands()[1].active);
        mock.invalid_joint = XR_HAND_JOINT_LITTLE_TIP_EXT;
        update(hands);
        CHECK(!hands.hands()[0].active && !hands.hands()[1].active);
        CHECK(hands.hands()[0].invalid_joint == XR_HAND_JOINT_LITTLE_TIP_EXT);
        mock.invalid_joint = -1;
        mock.nonfinite_position = true;
        update(hands);
        CHECK(!hands.hands()[0].active);
        mock.nonfinite_position = false;
        mock.invalid_orientation = true;
        update(hands);
        CHECK(!hands.hands()[0].active);
        mock.invalid_orientation = false;
        mock.invalid_radius = true;
        update(hands);
        CHECK(!hands.hands()[0].active);
        mock.invalid_radius = false;
        mock.controller_source = true;
        update(hands);
        CHECK(!hands.hands()[0].active && !hands.hands()[1].active);
        mock.controller_source = false;
        mock.source_active = false;
        update(hands);
        CHECK(!hands.hands()[0].active);
        mock.source_active = true;
        const auto recentered_space = static_cast<XrSpace>(4);
        hands.update(session, recentered_space, 234567, true);
        CHECK(hands.hands()[0].active && mock.reference_space == recentered_space && mock.time == 234567);
        CHECK(mock.space == hand_view_space && mock.head_time == mock.time);
        hands.hide();
        CHECK(!hands.hands()[0].active && hands.hands()[0].joints[0].locationFlags == 0);
        update(hands);
        CHECK(hands.hands()[0].active);
        hands.shutdown();

        // The core extension alone is sufficient; no data-source chain required.
        hands.initialize(instance, 1, true, false);
        update(hands);
        CHECK(hands.hands()[0].active && hands.hands()[1].active);
        hands.shutdown();
    }

    void test_motion_compensation()
    {
        mock = {};
        HandTracking hands;
        hands.initialize(instance, 1, true, true);
        update(hands);
        const auto original = hands.hands();
        const auto check_hands = [&](const M4& expected_motion) {
            for (size_t side = 0; side < original.size(); ++side) {
                const auto& actual = hands.hands()[side];
                CHECK(actual.active && actual.status == HandTracking::Status::Tracked);
                for (size_t joint = 0; joint < actual.joints.size(); ++joint) {
                    check_pose(actual.joints[joint].pose, matrix_pose(expected_motion * pose_matrix(original[side].joints[joint].pose)));
                    CHECK(actual.joints[joint].radius == original[side].joints[joint].radius);
                    CHECK(actual.joints[joint].locationFlags == original[side].joints[joint].locationFlags);
                }
            }
            CHECK(mock.space == hand_view_space && mock.head_time == mock.time);
        };

        // Ordinary leaning/turning with hands fixed on the wheel must leave
        // their world poses unchanged. This catches a head-attached offset.
        const auto lean = glm::translate(M4(1), glm::vec3(0.08f, 0.03f, -0.10f))
            * glm::rotate(M4(1), 0.3f, glm::vec3(0, 1, 0))
            * glm::rotate(M4(1), -0.2f, glm::vec3(1, 0, 0));
        mock.raw_head = mock.compensated_head = matrix_pose(lean);
        update(hands);
        check_hands(M4(1));
        const auto hand_movement = glm::translate(M4(1), glm::vec3(0, 0, -0.08f));
        mock.joint_motion = matrix_pose(hand_movement);
        update(hands);
        check_hands(hand_movement); // Actual hand movement is preserved.

        // Simulate braking surge/pitch plus yaw/heave: physical head and hands
        // move with the rig. The layer removes that rig transform from the head.
        const auto rig = glm::translate(M4(1), glm::vec3(0.14f, 0.05f, 0.09f))
            * glm::rotate(M4(1), 0.23f, glm::vec3(0, 1, 0))
            * glm::rotate(M4(1), -0.18f, glm::vec3(1, 0, 0));
        mock.raw_head = matrix_pose(rig * lean);
        mock.compensated_head = matrix_pose(lean);
        mock.joint_motion = matrix_pose(rig);
        update(hands);
        check_hands(M4(1));
        mock.joint_motion = matrix_pose(rig * hand_movement);
        update(hands);
        check_hands(hand_movement);

        // Compensation toggles live: no rig/layer state or stale correction is
        // cached in the plugin. Inactive compensation preserves physical motion.
        mock.joint_motion = matrix_pose(rig);
        mock.compensated_head = mock.raw_head;
        update(hands);
        check_hands(rig);
        mock.compensated_head = matrix_pose(lean);
        update(hands);
        check_hands(M4(1));
        CHECK(mock.creates == 2 && mock.space_creates == 1);

        // Follow an arbitrary/partial correction supplied by a layer, then a
        // rotated/translated recentered LOCAL space. No perfect-MC assumption.
        const auto correction = glm::translate(M4(1), glm::vec3(-0.04f, 0.02f, -0.07f))
            * glm::rotate(M4(1), 0.11f, glm::normalize(glm::vec3(1, 2, 3)));
        const auto recenter = glm::translate(M4(1), glm::vec3(0.2f, -0.3f, 0.1f))
            * glm::rotate(M4(1), -0.4f, glm::vec3(0, 1, 0));
        mock.compensated_head = matrix_pose(recenter * correction * pose_matrix(mock.raw_head));
        const auto recentered_space = static_cast<XrSpace>(6);
        hands.update(session, recentered_space, 987654, true);
        check_hands(recenter * correction * rig);
        CHECK(mock.reference_space == recentered_space && mock.head_time == 987654);
        hands.shutdown();
        CHECK(mock.space_creates == mock.space_destroys);
    }

    void test_hand_reference_failures()
    {
        mock = {};
        HandTracking hands;
        hands.initialize(instance, 1, true, false);
        mock.space_create_failure = true;
        update(hands);
        CHECK(!hands.enabled() && mock.space_creates == 1 && mock.creates == 0);
        update(hands);
        CHECK(mock.space_creates == 1);
        update(hands, false);
        mock.space_create_failure = false;
        update(hands);
        CHECK(hands.enabled() && mock.space_creates == 2 && mock.creates == 2);

        const auto check_hidden = [&] {
            const auto joint_queries = mock.locates;
            update(hands);
            CHECK(mock.locates == joint_queries); // Never use a stale head pose.
            CHECK(hands.enabled()); // Keep trackers to recover next frame.
            for (const auto& hand : hands.hands()) {
                CHECK(!hand.active && hand.status == HandTracking::Status::InvalidReference);
                CHECK(hand.joints[0].locationFlags == 0);
            }
        };
        mock.space_locate_failure = true;
        check_hidden();
        mock.space_locate_failure = false;
        for (const XrSpaceLocationFlags flags : { 0ULL, static_cast<unsigned long long>(XR_SPACE_LOCATION_ORIENTATION_VALID_BIT),
                 static_cast<unsigned long long>(XR_SPACE_LOCATION_POSITION_VALID_BIT) }) {
            mock.head_flags = flags;
            check_hidden();
        }
        mock.head_flags = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        const auto identity = mock.compensated_head;
        mock.compensated_head.position.x = std::numeric_limits<float>::quiet_NaN();
        check_hidden();
        mock.compensated_head = identity;
        mock.compensated_head.position.z = std::numeric_limits<float>::infinity();
        check_hidden();
        mock.compensated_head = identity;
        mock.compensated_head.orientation = {};
        check_hidden();
        mock.compensated_head.orientation.w = std::numeric_limits<float>::infinity();
        check_hidden();
        mock.compensated_head = identity;
        update(hands);
        CHECK(hands.hands()[0].active && hands.hands()[1].active);
        CHECK(mock.creates == 2 && mock.space_creates == 2);
        mock.active[0] = false;
        update(hands);
        CHECK(!hands.hands()[0].active && hands.hands()[1].active);
        mock.active[0] = true;
        update(hands);
        CHECK(hands.hands()[0].active);
        hands.shutdown();
        CHECK(mock.space_destroys == 1 && mock.destroys == 2);
        hands.shutdown();
        CHECK(mock.space_destroys == 1);
    }

    void test_menu_gestures()
    {
        HandMenuPinch pinch;
        CHECK(!pinch.update(true, true, 0.01f, 0)); // Held on acquisition.
        CHECK(!pinch.update(true, true, 0.01f, 0.4));
        CHECK(!pinch.update(true, true, 0.05f, 0.5));
        CHECK(pinch.armed);
        CHECK(pinch.update(true, true, 0.01f, 0.6));
        CHECK(!pinch.update(true, true, 0.01f, 3)); // Held beyond cooldown.
        CHECK(!pinch.update(true, true, 0.03f, 3.1)); // Hysteresis band.
        CHECK(!pinch.update(true, true, 0.01f, 3.2));
        CHECK(!pinch.update(true, true, 0.04f, 3.3));
        CHECK(pinch.update(true, true, 0.01f, 3.4));
        CHECK(!pinch.update(true, true, 0.04f, 3.5));
        CHECK(!pinch.update(true, true, 0.01f, 3.6)); // Cooldown consumes early pinch.
        CHECK(!pinch.update(true, true, 0.01f, 4.1));
        CHECK(!pinch.update(false, true, 0.01f, 4.2));
        CHECK(!pinch.update(true, true, 0.01f, 4.3));
        CHECK(!pinch.update(true, true, 0.01f, 5));
        CHECK(!pinch.update(true, false, 0.05f, 5.1));
        CHECK(!pinch.update(true, true, 0.01f, 5.2)); // Too soon after facing.
        CHECK(!pinch.update(true, true, 0.01f, 6));
        CHECK(!pinch.update(true, true, 0.05f, 6.1));
        CHECK(pinch.update(true, true, 0.01f, 6.2));
        CHECK(!pinch.update(true, true, std::numeric_limits<float>::quiet_NaN(), 6.3));
        CHECK(!pinch.released && !pinch.armed);
    }

    void test_menu_touch()
    {
        using namespace HandMenuTuning;
        HandMenuTouch touch;
        const glm::vec3 contact(0, button_y, 0);
        CHECK(touch.update(true, contact) == -1); // Already touching on acquisition.
        CHECK(touch.update(true, { 0, button_y, 0.04f }) == -1);
        CHECK(touch.hover == 0 && touch.ready);
        CHECK(touch.update(true, contact) == 0);
        for (int i = 0; i < 100; ++i)
            CHECK(touch.update(true, contact) == -1);
        CHECK(touch.update(true, { 0, -button_y, 0 }) == -1); // Sliding cannot select another.
        CHECK(touch.update(true, { 0, -button_y, 0.02f }) == -1); // Insufficient withdrawal.
        CHECK(touch.update(true, { 0, -button_y, release_depth }) == -1);
        CHECK(touch.update(true, { 0, -button_y, press_depth }) == 1);
        CHECK(touch.update(false, {}) == -1);
        CHECK(touch.pressed == -1 && !touch.ready);
        CHECK(touch.update(true, contact) == -1);
        CHECK(touch.update(true, { 0, button_y, 0.04f }) == -1);
        CHECK(touch.update(true, { 0, button_y, back_depth - 0.001f }) == -1); // No back entry.
        CHECK(touch.update(true, contact) == -1);
        touch.reset();
        CHECK(touch.update(true, { 0, button_y, 0.04f }) == -1);
        CHECK(touch.update(true, { button_half_width + 0.001f, button_y, 0 }) == -1);
        CHECK(touch.update(true, contact) == -1); // No sideways entry at press depth.
        CHECK(HandMenuTouch::button({ button_half_width, button_y, 0 }) == 0);
        CHECK(HandMenuTouch::button({ button_half_width + 0.001f, button_y, 0 }) == -1);
        CHECK(HandMenuTouch::button({ 0, 0, 0 }) == -1); // Gap.
        CHECK(HandMenuTouch::button({ 0, -button_y - button_half_height - 0.001f, 0 }) == -1);
        touch.reset();
        touch.update(true, { 0, button_y, 0.04f });
        CHECK(touch.update(true, contact) == 0);
        touch.update(true, { button_half_width + 0.005f, button_y, 0 });
        CHECK(touch.pressed == 0); // Expanded rectangle holds press feedback.
        touch.update(true, { button_half_width + release_margin + 0.005f, button_y, 0 });
        CHECK(touch.pressed == -1 && !touch.ready);
        CHECK(touch.update(true, { std::numeric_limits<float>::infinity(), 0, 0 }) == -1);
    }

    void test_menu_integration()
    {
        std::array<HandTracking::Hand, 2> hands;
        for (int i = 0; i < 2; ++i) {
            hands[i].active = true;
            fill_joints(hands[i].joints.data(), i);
        }
        XrPosef head { { 0, 0, 0, 1 }, { 0, 0, 0 } };
        // Rotate palm -Y toward +Z/head, exactly as the OpenXR convention requires.
        const auto palm_rotation = glm::angleAxis(-1.5707963f, glm::vec3(1, 0, 0));
        hands[0].joints[0].pose.orientation = { palm_rotation.x, palm_rotation.y, palm_rotation.z, palm_rotation.w };
        const auto pinch = [&](bool pressed) {
            auto& thumb = hands[0].joints[XR_HAND_JOINT_THUMB_TIP_EXT].pose.position;
            thumb = hands[0].joints[XR_HAND_JOINT_INDEX_TIP_EXT].pose.position;
            thumb.x += pressed ? 0.01f : 0.05f;
        };
        HandMenu menu;
        const auto update_menu = [&](double now, bool eligible = true) {
            return menu.update(hands, head, eligible, now);
        };
        pinch(false);
        update_menu(0);
        update_menu(0.3);
        HandMesh icon;
        menu.append_mesh(icon);
        CHECK(!icon.vertices.empty());
        pinch(true);
        update_menu(0.4); // Local palm-facing pinch opens the menu.
        CHECK(menu.is_open());
        const auto captured = menu.panel_pose();
        hands[0].joints[0].pose.position.x += 0.05f;
        update_menu(0.5);
        CHECK(menu.panel_pose() == captured); // Fixed placement, independent of hand motion.
        HandMesh panel;
        menu.append_mesh(panel);
        CHECK(panel.vertices.size() > 500);
        for (auto i : panel.indices)
            CHECK(i < panel.vertices.size());
        // Interaction and rendering share the same rotated/translated panel pose.
        const auto tip = [&](glm::vec3 local) {
            const auto p = glm::vec3(menu.panel_pose() * glm::vec4(local, 1));
            hands[1].joints[XR_HAND_JOINT_INDEX_TIP_EXT].pose.position = { p.x, p.y, p.z };
        };
        tip({ 0, HandMenuTuning::button_y, 0 });
        CHECK(update_menu(5.5) == HandMenu::Action::None);
        tip({ 0, HandMenuTuning::button_y, 0.04f });
        update_menu(5.6);
        tip({ 0, HandMenuTuning::button_y, 0 });
        CHECK(update_menu(5.7) == HandMenu::Action::None);
        CHECK(menu.is_open());
        // Place start button stays armed for placement; held panel contact does not toggle it.
        CHECK(update_menu(5.8) == HandMenu::Action::None && menu.is_open());
        CHECK(menu.start_button().state == HandPlacedButton::State::Placing);
        menu.reset(); // Lifecycle reset still closes it.
        CHECK(update_menu(6) == HandMenu::Action::None && !menu.is_open());
        pinch(false);
        update_menu(6.1);
        update_menu(6.4);
        pinch(true);
        update_menu(6.5);
        CHECK(menu.is_open());
        hands[1].active = false;
        const auto retained_panel = menu.panel_pose();
        update_menu(6.6);
        CHECK(menu.is_open() && !menu.is_active());
        CHECK(menu.panel_pose() == retained_panel);
        HandMesh suspended_panel;
        menu.append_mesh(suspended_panel);
        CHECK(!suspended_panel.vertices.empty());
        hands[0].active = false; // Neither tracked hand is needed to retain the panel.
        update_menu(6.601);
        CHECK(menu.is_open());
        hands[0].active = true;
        hands[1].active = true;
        update_menu(7);
        update_menu(7.5);
        CHECK(menu.is_open()); // A held pinch on recovery cannot toggle.
        pinch(false);
        update_menu(7.6);
        pinch(true);
        update_menu(7.7);
        CHECK(!menu.is_open()); // A fresh released pinch still toggles.
        update_menu(7.8, false); // Mode/focus/config loss.
        CHECK(!menu.is_open());
        menu.reset();
        hands[0].joints[0].pose.orientation = { 0, 0, 0, 1 };
        pinch(false);
        update_menu(8);
        update_menu(8.3);
        pinch(true);
        update_menu(8.4);
        CHECK(!menu.is_open()); // Back of hand is not palm-facing.
        menu.reset();
        hands[0].joints[0].pose.position.z = -2;
        pinch(false);
        update_menu(10);
        update_menu(10.3);
        pinch(true);
        update_menu(10.4);
        CHECK(!menu.is_open()); // Out of range.
        menu.reset();
        hands[0].joints[0].pose.position.z = -0.4f;
        hands[0].joints[0].pose.orientation = { palm_rotation.x, palm_rotation.y, palm_rotation.z, palm_rotation.w };
        pinch(false);
        update_menu(11);
        pinch(true);
        update_menu(11.1); // Pinch before stable: no local activation.
        CHECK(!menu.is_open());
        update_menu(11.2);
        CHECK(!menu.is_open()); // An early held pinch cannot activate later.
        head.orientation = {};
        update_menu(11.3);
        CHECK(!menu.is_open());
        menu.reset();
        head.orientation.w = 1;
        // Apply an arbitrary reference-space rigid transform to head and hands.
        // Facing, placement and direct touch must remain invariant.
        const auto correction = glm::translate(M4(1), glm::vec3(0.2f, -0.1f, 0.3f))
            * glm::rotate(M4(1), 0.6f, glm::normalize(glm::vec3(1, 2, 3)));
        head = matrix_pose(correction * pose_matrix(head));
        pinch(false);
        for (auto& hand : hands)
            for (auto& joint : hand.joints)
                joint.pose = matrix_pose(correction * pose_matrix(joint.pose));
        update_menu(12);
        update_menu(12.3);
        const auto transformed_index = hands[0].joints[XR_HAND_JOINT_INDEX_TIP_EXT].pose.position;
        hands[0].joints[XR_HAND_JOINT_THUMB_TIP_EXT].pose.position = transformed_index;
        update_menu(12.4);
        CHECK(menu.is_open());
        tip({ 0, -HandMenuTuning::button_y, 0.04f });
        update_menu(12.5);
        tip({ 0, -HandMenuTuning::button_y, 0 });
        CHECK(update_menu(12.6) == HandMenu::Action::Close);
    }

    void test_menu_open_palm_and_either_hand()
    {
        for (int side : { 0, 1 }) {
            std::array<HandTracking::Hand, 2> hands;
            for (int i = 0; i < 2; ++i) {
                hands[i].active = true;
                fill_joints(hands[i].joints.data(), i);
            }
            XrPosef head { { 0, 0, 0, 1 }, { 0, 0, 0 } };
            // An open palm tilted about 65 degrees from the direction to the
            // headset must show the icon, without curling any fingertips.
            const auto tilt = glm::angleAxis(-0.85f, glm::vec3(1, 0, 0));
            hands[0].joints[0].pose.orientation = { tilt.x, tilt.y, tilt.z, tilt.w };
            auto& thumb = hands[0].joints[XR_HAND_JOINT_THUMB_TIP_EXT].pose.position;
            thumb = hands[0].joints[XR_HAND_JOINT_INDEX_TIP_EXT].pose.position;
            thumb.x += 0.10f;
            HandMenu menu;
            const auto update = [&](double now) { return menu.update(hands, head, true, now); };
            update(0);
            update(0.3);
            HandMesh icon;
            menu.append_mesh(icon);
            CHECK(!icon.vertices.empty() && !menu.is_open());
            const auto palm_position = hands[0].joints[0].pose.position;
            // The icon is beside the open hand, not buried over the glove.
            CHECK(icon.vertices[0].position.x < palm_position.x - 0.05f);
            thumb.x -= 0.09f;
            update(0.4);
            CHECK(menu.is_open());
            thumb.x += 0.09f;
            const auto tip = [&](int hand, glm::vec3 local) {
                const auto p = glm::vec3(menu.panel_pose() * glm::vec4(local, 1));
                hands[hand].joints[XR_HAND_JOINT_INDEX_TIP_EXT].pose.position = { p.x, p.y, p.z };
            };
            tip(side, { 0, HandMenuTuning::button_y, 0.04f });
            update(0.5);
            tip(side, { 0, HandMenuTuning::button_y, 0 });
            CHECK(update(0.6) == HandMenu::Action::None);
            CHECK(menu.is_open());
            for (double now : { 0.7, 1.0, 1.5 })
                CHECK(update(now) == HandMenu::Action::None && menu.is_open());

            // Same physical head/hands/panel, expressed in recentered coordinates.
            const auto captured = menu.panel_pose();
            const auto transform = glm::translate(M4(1), glm::vec3(-0.2f, 0.3f, 0.1f))
                * glm::rotate(M4(1), 0.7f, glm::vec3(0, 1, 0));
            menu.rebase_after_recenter(matrix_pose(transform));
            CHECK(menu.is_open());
            check_pose(matrix_pose(menu.panel_pose()), matrix_pose(transform * captured));
            head = matrix_pose(transform * pose_matrix(head));
            for (auto& hand : hands)
                for (auto& joint : hand.joints)
                    joint.pose = matrix_pose(transform * pose_matrix(joint.pose));
            for (double now : { 2.0, 2.5, 3.0 })
                CHECK(update(now) == HandMenu::Action::None && menu.is_open());
            tip(side, { 0, -HandMenuTuning::button_y, 0 });
            CHECK(update(3.1) == HandMenu::Action::None); // Slide after a reference change cannot press.
            tip(side, { 0, -HandMenuTuning::button_y, 0.04f });
            update(3.2);
            tip(side, { 0, -HandMenuTuning::button_y, 0 });
            CHECK(update(3.3) == HandMenu::Action::Close && !menu.is_open());
        }
        std::array<HandTracking::Hand, 2> hands;
        for (int i = 0; i < 2; ++i) {
            hands[i].active = true;
            fill_joints(hands[i].joints.data(), i);
        }
        XrPosef head { { 0, 0, 0, 1 }, { 0, 0, 0 } };
        const auto palm = glm::angleAxis(-1.5707963f, glm::vec3(1, 0, 0));
        hands[0].joints[0].pose.orientation = { palm.x, palm.y, palm.z, palm.w };
        auto& thumb = hands[0].joints[XR_HAND_JOINT_THUMB_TIP_EXT].pose.position;
        thumb = hands[0].joints[XR_HAND_JOINT_INDEX_TIP_EXT].pose.position;
        // Held pinch on tracking acquisition must not activate, but the palm
        // icon still appears: its visibility does not depend on pinch release.
        HandMenu menu;
        const auto update = [&](double now) { return menu.update(hands, head, true, now); };
        update(0);
        update(0.3);
        CHECK(!menu.is_open());
        HandMesh held_icon;
        menu.append_mesh(held_icon);
        CHECK(!held_icon.vertices.empty());
        thumb.x += 0.10f;
        update(0.4);
        thumb.x -= 0.10f;
        update(0.5);
        CHECK(menu.is_open());
        thumb.x += 0.10f;
        const auto both_tips = [&](float depth) {
            const auto p = glm::vec3(menu.panel_pose() * glm::vec4(0, HandMenuTuning::button_y, depth, 1));
            for (auto& hand : hands)
                hand.joints[XR_HAND_JOINT_INDEX_TIP_EXT].pose.position = { p.x, p.y, p.z };
        };
        both_tips(0.04f);
        update(0.6);
        both_tips(0);
        CHECK(update(0.7) == HandMenu::Action::None && menu.is_open());
        CHECK(update(1.7) == HandMenu::Action::None && menu.is_open());
        menu.rebase_after_recenter(XrPosef {}); // Invalid transform never leaves stale geometry.
        CHECK(!menu.is_open());
    }

    void test_help_hold()
    {
        HandMenuHold hold;
        CHECK(!hold.update(true, 1));
        for (int i = 1; i < 20; ++i)
            CHECK(!hold.update(true, 1 + i * 0.1));
        CHECK(hold.progress(2) > 0.49f && hold.progress(2) < 0.51f);
        CHECK(hold.update(true, 3));
        for (int i = 1; i < 20; ++i)
            CHECK(!hold.update(true, 3 + i * 0.1));
        CHECK(!hold.update(false, 6));
        CHECK(!hold.update(true, 7));
        CHECK(!hold.update(true, 7.1));
        CHECK(!hold.update(false, 7.2)); // Early lift cancels elapsed time.
        CHECK(hold.progress(7.2) == 0);
        CHECK(!hold.update(true, 8));
        CHECK(!hold.update(true, 12)); // A frame stall is not observed continuous contact.
        CHECK(hold.progress(12) == 0);
        CHECK(!hold.update(true, std::numeric_limits<double>::quiet_NaN()));
        CHECK(hold.progress(13) == 0);

        std::array<HandTracking::Hand, 2> hands;
        for (int i = 0; i < 2; ++i) {
            hands[i].active = true;
            fill_joints(hands[i].joints.data(), i);
        }
        XrPosef head { { 0, 0, 0, 1 }, { 0, 0, 0 } };
        const auto palm = glm::angleAxis(-1.5707963f, glm::vec3(1, 0, 0));
        hands[0].joints[0].pose.orientation = { palm.x, palm.y, palm.z, palm.w };
        auto& thumb = hands[0].joints[XR_HAND_JOINT_THUMB_TIP_EXT].pose.position;
        thumb = hands[0].joints[XR_HAND_JOINT_INDEX_TIP_EXT].pose.position;
        thumb.x += 0.10f;
        HandMenu menu;
        menu.update(hands, head, true, 0);
        menu.update(hands, head, true, 0.3);
        thumb.x -= 0.09f;
        menu.update(hands, head, true, 0.4);
        CHECK(menu.is_open());
        double now = 0.5;
        const auto update = [&](double dt = 0.1, bool eligible = true) {
            thumb = hands[0].joints[XR_HAND_JOINT_INDEX_TIP_EXT].pose.position;
            thumb.x += 0.10f;
            now += dt;
            return menu.update(hands, head, eligible, now);
        };
        const auto tip = [&](int hand, int button, float depth) {
            const auto p = glm::vec3(menu.panel_pose() * glm::vec4(0, HandMenuTuning::button_centers[button], depth, 1));
            hands[hand].joints[XR_HAND_JOINT_INDEX_TIP_EXT].pose.position = { p.x, p.y, p.z };
        };
        const auto wait = [&](int frames) {
            for (int i = 0; i < frames; ++i)
                CHECK(update() == HandMenu::Action::None);
        };
        const auto place_press = [&](int hand, int button) {
            tip(hand, button, 0.05f);
            CHECK(update() == HandMenu::Action::None);
            tip(hand, button, 0);
            CHECK(update() == HandMenu::Action::None);
        };
        place_press(1, 0);
        hands[1].joints[XR_HAND_JOINT_INDEX_TIP_EXT].pose.position = { 0.3f, 0.1f, -0.4f };
        place_press(0, 0);
        CHECK(menu.start_button().state == HandPlacedButton::State::Locked);
        CHECK(menu.take_locked_button() == 0);
        CHECK(menu.take_locked_button() == -1);
        const auto start_pose = menu.start_button().pose;
        place_press(1, 2);
        CHECK(menu.help_button().state == HandPlacedButton::State::Placing);
        place_press(1, 2); // Right hand cannot lock.
        CHECK(menu.help_button().state == HandPlacedButton::State::Placing);
        hands[1].joints[XR_HAND_JOINT_INDEX_TIP_EXT].pose.position = { -0.3f, 0.1f, -0.4f };
        place_press(0, 2);
        CHECK(menu.help_button().state == HandPlacedButton::State::Locked);
        CHECK(menu.take_locked_button() == 1);
        CHECK(menu.take_locked_button() == -1);
        CHECK(glm::distance(glm::vec3(menu.help_button().pose[3]), glm::vec3(-0.3f, 0.1f, -0.4f)) < 0.001f);
        CHECK(menu.start_button().pose == start_pose); // Independent placement.
        const auto cube_tip = [&](int hand, float distance) {
            const auto p = glm::vec3(menu.help_button().pose * glm::vec4(distance, 0, 0, 1));
            hands[hand].joints[XR_HAND_JOINT_INDEX_TIP_EXT].pose.position = { p.x, p.y, p.z };
        };
        const auto begin = [&](int hand) {
            cube_tip(hand, 0.1f);
            CHECK(update() == HandMenu::Action::None);
            cube_tip(hand, 0);
            CHECK(update() == HandMenu::Action::None);
        };
        wait(35); // Right index already inside at lock cannot activate.
        begin(1);
        wait(19);
        CHECK(update(0.11) == HandMenu::Action::CallForHelp && menu.is_open());
        wait(50); // Holding after firing never repeats.
        begin(1);
        wait(10);
        cube_tip(1, 0.04f); // Early withdrawal cancels elapsed time.
        CHECK(update() == HandMenu::Action::None);
        cube_tip(1, 0);
        wait(10);
        hands[1].active = false;
        CHECK(update() == HandMenu::Action::None && menu.is_open() && !menu.is_active());
        hands[1].active = true;
        wait(40); // Reacquiring inside cannot start a new countdown.
        CHECK(menu.help_button().state == HandPlacedButton::State::Locked);
        begin(0); // Left index can hold the invisible cube too.
        wait(19);
        CHECK(update(0.11) == HandMenu::Action::CallForHelp);
        cube_tip(0, 0.1f);
        update();
        // Close the retained panel partway through a right-hand hold.
        begin(1);
        wait(10);
        tip(0, 1, 0.05f);
        CHECK(update() == HandMenu::Action::None);
        tip(0, 1, 0);
        CHECK(update() == HandMenu::Action::Close);
        wait(7);
        CHECK(update(0.11) == HandMenu::Action::CallForHelp); // Closing only hides it.
        wait(35);
        CHECK(!menu.is_open());
        HandMesh hidden;
        menu.append_mesh(hidden);
        for (const auto& vertex : hidden.vertices)
            CHECK(glm::distance(vertex.position, glm::vec3(menu.help_button().pose[3])) > 0.06f); // Icon may remain; cube hides.
        const auto help_pose = menu.help_button().pose;
        const XrPosef shift { { 0, 0, 0, 1 }, { 0.2f, 0, 0 } };
        menu.rebase_after_recenter(shift);
        CHECK(glm::distance(glm::vec3(menu.help_button().pose[3]), glm::vec3(help_pose[3]) + glm::vec3(0.2f, 0, 0)) < 0.001f);
        CHECK(!menu.help_button().is_down());
        menu.reset(true);
        CHECK(!menu.has_placement());
    }

    void test_start_button()
    {
        using namespace HandMenuTuning;
        HandPlacedButton cube;
        cube.state = HandPlacedButton::State::Locked;
        cube.pose = glm::translate(M4(1), glm::vec3(0.2f, -0.3f, -0.5f))
            * glm::rotate(M4(1), 0.8f, glm::vec3(0, 1, 0));
        std::array<glm::vec3, 2> tips;
        const auto tip = [&](int hand, glm::vec3 local) {
            tips[hand] = glm::vec3(cube.pose * glm::vec4(local, 1));
        };
        tip(0, { 0.1f, 0, 0 });
        tip(1, { 0, 0, 0 });
        CHECK(!cube.update(tips)); // Already inside at lock: withdraw first.
        CHECK(!cube.update(tips));
        tip(1, { 0, 0, start_cube_release + 0.001f });
        CHECK(!cube.update(tips));
        tip(1, { start_cube_half + 0.001f, 0, 0 });
        CHECK(!cube.update(tips) && cube.hover);
        tip(1, { start_cube_half - 0.001f, 0, 0 });
        CHECK(cube.update(tips));
        for (int frame = 0; frame < 100; ++frame)
            CHECK(cube.update(tips) && cube.is_down()); // Holds, rather than a tap.
        tip(1, { start_cube_release - 0.001f, 0, 0 });
        CHECK(cube.update(tips)); // Small tracking jitter does not release.
        tip(1, { start_cube_release + 0.001f, 0, 0 });
        CHECK(!cube.update(tips) && !cube.is_down()); // Withdrawal is button up.
        tip(1, { 0, 0, 0 });
        CHECK(cube.update(tips)); // A new deliberate contact presses again.
        cube.clear_contact();
        CHECK(!cube.update(tips)); // Tracking recovery inside cannot press.
        tip(1, { 0.1f, 0, 0 });
        cube.update(tips);
        tip(1, { 0, 0, 0 });
        CHECK(cube.update(tips));
        tips[1].x = std::numeric_limits<float>::quiet_NaN();
        CHECK(!cube.update(tips) && !cube.is_down()); // Invalid input releases immediately.
        tip(1, { 0, 0, 0 });
        CHECK(!cube.update(tips));
        tip(0, { 0.1f, 0, 0 });
        tip(1, { 0.1f, 0, 0 });
        cube.update(tips);
        tip(0, { 0, 0, 0 });
        tip(1, { 0, 0, 0 });
        CHECK(cube.update(tips)); // Both fingers hold one virtual button.
        tip(0, { 0.1f, 0, 0 });
        CHECK(cube.update(tips)); // Still held by the right index.
        tip(1, { 0.1f, 0, 0 });
        CHECK(!cube.update(tips)); // Last fingertip leaving releases.
        tip(0, { 0, 0, 0 });
        CHECK(cube.update(tips)); // Left-only contact also holds.
        cube.state = HandPlacedButton::State::Placing;
        CHECK(!cube.update(tips) && !cube.is_down()); // Repositioning releases.

        IgnitionButton input;
        CHECK(!input.is_down(12, 100));
        input.set_down(true, 100);
        CHECK(!input.is_down(7, 110));
        CHECK(input.is_down(12, 120));
        CHECK(input.is_down(12, 121)); // Input polls do not consume the hold.
        for (uint64_t now = 200; now < 5000; now += 100) {
            input.set_down(true, now);
            CHECK(input.is_down(12, now + 99)); // Valid frames renew long holds.
        }
        input.set_down(false, 5000);
        CHECK(!input.is_down(12, 5000));
        input.set_down(true, 5100);
        input.release();
        CHECK(!input.is_down(12, 5101));
        input.set_down(true, 5200);
        CHECK(!input.is_down(12, 5200 + IgnitionButton::timeout_ms)); // Frame-stall failsafe.

        std::array<uint8_t, 2> argument { 0x6a, 12 };
        std::array<uint8_t, 15> call { 0xe8, 0xe6, 0x61, 0xff, 0xff, 0x85, 0xc0, 0x74, 0x04, 0x83, 0x4e, 0x04, 0x01, 0x6a, 0x07 };
        CHECK(resolve_ignition_call(argument, call, 0x4CC3E5) == 0x4C25D0);
        const int32_t redirected = 0x10001000 - (0x4CC3E5 + 5);
        memcpy(call.data() + 1, &redirected, sizeof(redirected));
        CHECK(resolve_ignition_call(argument, call, 0x4CC3E5) == 0x10001000);
        call[5] = 0x90; // Unknown result handling must still reject the hook.
        CHECK(!resolve_ignition_call(argument, call, 0x4CC3E5));
        call[5] = 0x85;
        argument[1] = 7;
        CHECK(!resolve_ignition_call(argument, call, 0x4CC3E5));
        CHECK(!resolve_ignition_call({}, call, 0x4CC3E5));
        CHECK(!resolve_ignition_call(argument, { call.data(), 4 }, 0x4CC3E5));
    }

    void test_start_placement()
    {
        using namespace HandMenuTuning;
        std::array<HandTracking::Hand, 2> hands;
        for (int i = 0; i < 2; ++i) {
            hands[i].active = true;
            fill_joints(hands[i].joints.data(), i);
        }
        XrPosef head { { 0, 0, 0, 1 }, { 0, 0, 0 } };
        const auto palm = glm::angleAxis(-1.5707963f, glm::vec3(1, 0, 0));
        hands[0].joints[0].pose.orientation = { palm.x, palm.y, palm.z, palm.w };
        auto& thumb = hands[0].joints[XR_HAND_JOINT_THUMB_TIP_EXT].pose.position;
        thumb = hands[0].joints[XR_HAND_JOINT_INDEX_TIP_EXT].pose.position;
        thumb.x += 0.10f;
        HandMenu menu;
        menu.update(hands, head, true, 0);
        menu.update(hands, head, true, 0.3);
        thumb.x -= 0.09f;
        menu.update(hands, head, true, 0.4);
        CHECK(menu.is_open());
        double now = 0.5;
        const auto update = [&](bool eligible = true) {
            thumb = hands[0].joints[XR_HAND_JOINT_INDEX_TIP_EXT].pose.position;
            thumb.x += 0.10f;
            now += 0.1;
            return menu.update(hands, head, eligible, now);
        };
        const auto tip = [&](int hand, glm::vec3 position) {
            hands[hand].joints[XR_HAND_JOINT_INDEX_TIP_EXT].pose.position = { position.x, position.y, position.z };
        };
        const auto panel_tip = [&](int hand, int button, float depth) {
            tip(hand, glm::vec3(menu.panel_pose() * glm::vec4(0, button_centers[button], depth, 1)));
        };
        const auto press = [&](int hand, int button) {
            panel_tip(hand, button, 0.05f);
            update();
            panel_tip(hand, button, 0);
            return update();
        };
        CHECK(press(1, 0) == HandMenu::Action::None);
        CHECK(menu.start_button().state == HandPlacedButton::State::Placing);
        const glm::vec3 target(0.22f, -0.16f, -0.42f);
        tip(1, target);
        CHECK(update() == HandMenu::Action::None);
        CHECK(glm::distance(glm::vec3(menu.start_button().pose[3]), target) < 0.00001f);
        CHECK(press(1, 0) == HandMenu::Action::None);
        CHECK(menu.start_button().state == HandPlacedButton::State::Placing); // Right cannot lock.
        tip(1, target);
        update();
        CHECK(press(0, 0) == HandMenu::Action::None);
        CHECK(menu.start_button().state == HandPlacedButton::State::Locked);
        for (int i = 0; i < 15; ++i)
            CHECK(update() == HandMenu::Action::None && !menu.start_button().is_down()); // No down at lock.
        tip(1, target + glm::vec3(0.1f, 0, 0));
        update();
        tip(1, target);
        CHECK(update() == HandMenu::Action::None && menu.start_button().is_down());
        for (int i = 0; i < 15; ++i)
            CHECK(update() == HandMenu::Action::None && menu.start_button().is_down());
        CHECK(press(0, 1) == HandMenu::Action::Close);
        CHECK(!menu.is_open() && menu.has_placement() && menu.start_button().is_down());
        HandMesh mesh;
        menu.append_mesh(mesh);
        CHECK(mesh.vertices.empty()); // Closing frame hides cube and markers; ignition remains down.
        tip(1, target + glm::vec3(0.1f, 0, 0));
        CHECK(update() == HandMenu::Action::None && !menu.start_button().is_down());
        tip(1, target);
        CHECK(update() == HandMenu::Action::None && menu.start_button().is_down()); // Invisible contact works.
        menu.append_mesh(mesh);
        for (const auto& vertex : mesh.vertices)
            CHECK(glm::distance(vertex.position, target) > 0.06f); // Local icon may remain, start cube is hidden.
        thumb = hands[0].joints[XR_HAND_JOINT_INDEX_TIP_EXT].pose.position;
        thumb.x += 0.01f;
        now += 0.7;
        menu.update(hands, head, true, now);
        CHECK(menu.is_open() && menu.start_button().is_down()); // Visibility does not interrupt a hold.
        menu.append_mesh(mesh);
        CHECK(!mesh.vertices.empty());
        CHECK(press(0, 1) == HandMenu::Action::Close);
        CHECK(update(false) == HandMenu::Action::None && !menu.start_button().is_down());
        mesh.clear();
        menu.append_mesh(mesh);
        CHECK(mesh.vertices.empty()); // Unsupported modes hide cube and cancel contact.
        CHECK(update() == HandMenu::Action::None);
        CHECK(menu.start_button().state == HandPlacedButton::State::Locked);
        CHECK(update() == HandMenu::Action::None && !menu.start_button().is_down()); // Reacquired inside.
        tip(1, target + glm::vec3(0.1f, 0, 0));
        update();
        hands[1].active = false;
        CHECK(update() == HandMenu::Action::None);
        hands[1].active = true;
        tip(1, target);
        CHECK(update() == HandMenu::Action::None && !menu.start_button().is_down()); // Loss clears arming.
        tip(1, target + glm::vec3(0.1f, 0, 0));
        update();
        tip(1, target);
        CHECK(update() == HandMenu::Action::None && menu.start_button().is_down());
        const auto captured = menu.start_button().pose;
        const auto transform = glm::translate(M4(1), glm::vec3(0.2f, 0.3f, 0.1f))
            * glm::rotate(M4(1), 0.4f, glm::vec3(0, 1, 0));
        menu.rebase_after_recenter(matrix_pose(transform));
        CHECK(!menu.start_button().is_down()); // External recenter releases a held cube.
        check_pose(matrix_pose(menu.start_button().pose), matrix_pose(transform * captured));
        head = matrix_pose(transform * pose_matrix(head));
        for (auto& hand : hands)
            for (auto& joint : hand.joints)
                joint.pose = matrix_pose(transform * pose_matrix(joint.pose));
        CHECK(update() == HandMenu::Action::None);
        tip(1, glm::vec3(menu.start_button().pose * glm::vec4(0.1f, 0, 0, 1)));
        update();
        tip(1, glm::vec3(menu.start_button().pose[3]));
        CHECK(update() == HandMenu::Action::None && menu.start_button().is_down());
        menu.reset(true);
        CHECK(!menu.has_placement() && !menu.start_button().is_down()); // Session end releases and discards.
    }

    void test_mesh()
    {
        HandMesh combined;
        for (const auto side : { XR_HAND_LEFT_EXT, XR_HAND_RIGHT_EXT }) {
            const auto& model = hand_model(side);
            CHECK(model.vertices.size() == 3028 && model.indices.size() == 14697);
            CHECK(model.inverse_bind.size() == XR_HAND_JOINT_COUNT_EXT);
            for (const auto& vertex : model.vertices) {
                float sum = 0;
                for (size_t i = 0; i < 4; ++i) {
                    CHECK(vertex.joints[i] < XR_HAND_JOINT_COUNT_EXT);
                    CHECK(std::isfinite(vertex.weights[i]) && vertex.weights[i] >= 0);
                    sum += vertex.weights[i];
                }
                CHECK(std::abs(sum - 1.0f) < 0.00001f);
            }
            for (const auto index : model.indices)
                CHECK(index < model.vertices.size());

            HandTracking::Hand hand;
            HandMesh mesh;
            append_hand_mesh(hand, side, mesh);
            CHECK(mesh.vertices.empty() && mesh.indices.empty());
            hand.active = true;
            // Put the runtime skeleton in the asset's bind pose. Skinning must
            // reconstruct the source mesh, including each hand's orientation.
            for (size_t i = 0; i < hand.joints.size(); ++i) {
                const auto bind = glm::inverse(glm::make_mat4(model.inverse_bind[i].data()));
                const auto q = glm::normalize(glm::quat_cast(bind));
                hand.joints[i].pose = { { q.x, q.y, q.z, q.w }, { bind[3].x, bind[3].y, bind[3].z } };
            }
            append_hand_mesh(hand, side, mesh);
            CHECK(mesh.vertices.size() == model.vertices.size() && mesh.indices.size() == model.indices.size());
            for (size_t i = 0; i < mesh.vertices.size(); ++i) {
                const auto& source = model.vertices[i];
                const auto& vertex = mesh.vertices[i];
                CHECK(glm::distance(vertex.position, glm::vec3(source.position[0], source.position[1], source.position[2])) < 0.00001f);
                CHECK(std::isfinite(vertex.uv.x) && std::isfinite(vertex.uv.y));
                CHECK(vertex.uv.x == source.uv[0] && vertex.uv.y == source.uv[1]);
                CHECK((vertex.color & 0xff000000) == 0xff000000);
            }

            const auto rest = hand;
            const auto rotation = glm::angleAxis(0.7f, glm::normalize(glm::vec3(1, 2, 3)));
            const glm::vec3 translation(-0.16f, -0.15f, -0.4f);
            for (auto& joint : hand.joints) {
                const auto p = rotation * glm::vec3(joint.pose.position.x, joint.pose.position.y, joint.pose.position.z) + translation;
                const auto& q = joint.pose.orientation;
                const auto r = rotation * glm::quat(q.w, q.x, q.y, q.z);
                joint.pose = { { r.x, r.y, r.z, r.w }, { p.x, p.y, p.z } };
            }
            HandMesh moved;
            append_hand_mesh(hand, side, moved);
            for (size_t i = 0; i < mesh.vertices.size(); ++i)
                CHECK(glm::distance(moved.vertices[i].position, rotation * mesh.vertices[i].position + translation) < 0.00001f);

            // Rotating one finger joint should deform its weighted vertices,
            // without changing vertices belonging entirely to other fingers.
            hand = rest;
            auto& distal = hand.joints[XR_HAND_JOINT_INDEX_DISTAL_EXT].pose.orientation;
            const auto bend = glm::angleAxis(0.8f, glm::vec3(1, 0, 0)) * glm::quat(distal.w, distal.x, distal.y, distal.z);
            distal = { bend.x, bend.y, bend.z, bend.w };
            HandMesh bent;
            append_hand_mesh(hand, side, bent);
            size_t changed = 0;
            for (size_t i = 0; i < mesh.vertices.size(); ++i) {
                bool influenced = false;
                for (size_t j = 0; j < 4; ++j)
                    influenced |= model.vertices[i].joints[j] == XR_HAND_JOINT_INDEX_DISTAL_EXT && model.vertices[i].weights[j] > 0;
                const auto difference = glm::distance(bent.vertices[i].position, mesh.vertices[i].position);
                if (!influenced)
                    CHECK(difference < 0.00001f);
                changed += difference > 0.0001f;
            }
            CHECK(changed > 10);

            const auto base = combined.vertices.size();
            append_hand_mesh(rest, side, combined);
            for (size_t i = 0; i < model.indices.size(); ++i)
                CHECK(combined.indices[combined.indices.size() - model.indices.size() + i] == base + model.indices[i]);
            hand.active = false;
            append_hand_mesh(hand, side, combined);
            CHECK(combined.vertices.size() == base + model.vertices.size());
        }
        CHECK(combined.vertices.size() == 6056 && combined.indices.size() == 29394);
        combined.clear();
        CHECK(combined.vertices.empty() && combined.indices.empty());
    }

    void test_texture()
    {
        const auto& data = hand_texture_data();
        CHECK(data.levels.size() == 10 && data.levels[0].width == 512);
        size_t blocks = 0;
        for (size_t level = 0; level < data.levels.size(); ++level) {
            const auto& mip = data.levels[level];
            CHECK(mip.width == std::max(1u, 512u >> level) && mip.height == mip.width);
            CHECK(mip.block_offset == blocks);
            const size_t sides = std::max(1u, (mip.width + 3) / 4);
            blocks += sides * sides;
        }
        CHECK(blocks == data.blocks.size());
        for (const auto block : data.blocks)
            CHECK(static_cast<uint16_t>(block) > static_cast<uint16_t>(block >> 16)); // Opaque four-color BC1.

        struct MockTexture {
            int fail_lock = -1;
            int fail_unlock = -1;
            bool short_pitch = false;
            int locks = 0;
            int unlocks = 0;
            std::vector<std::vector<unsigned char>> buffers;
            HRESULT LockRect(UINT level, D3DLOCKED_RECT* rect, const RECT*, DWORD)
            {
                ++locks;
                if (static_cast<int>(level) == fail_lock)
                    return D3DERR_DEVICELOST;
                const auto& mip = hand_texture_data().levels[level];
                const size_t row = std::max(1u, (mip.width + 3) / 4) * 8;
                const size_t rows = std::max(1u, (mip.height + 3) / 4);
                rect->Pitch = static_cast<int>(row) + (short_pitch ? -1 : 16);
                buffers.resize(level + 1);
                buffers[level].assign((row + 16) * rows, 0xa5);
                rect->pBits = buffers[level].data();
                return D3D_OK;
            }
            HRESULT UnlockRect(UINT level)
            {
                ++unlocks;
                return static_cast<int>(level) == fail_unlock ? D3DERR_DEVICELOST : D3D_OK;
            }
        } texture;
        CHECK(upload_hand_texture(&texture, data) == D3D_OK);
        CHECK(texture.locks == 10 && texture.unlocks == 10);
        for (size_t level = 0; level < data.levels.size(); ++level) {
            const auto& mip = data.levels[level];
            const size_t row_bytes = std::max(1u, (mip.width + 3) / 4) * 8;
            const size_t rows = std::max(1u, (mip.height + 3) / 4);
            const auto* source = reinterpret_cast<const unsigned char*>(data.blocks.data() + mip.block_offset);
            for (size_t row = 0; row < rows; ++row) {
                const auto* dest = texture.buffers[level].data() + row * (row_bytes + 16);
                CHECK(std::memcmp(source + row * row_bytes, dest, row_bytes) == 0);
                for (size_t byte = row_bytes; byte < row_bytes + 16; ++byte)
                    CHECK(dest[byte] == 0xa5);
            }
        }
        texture = {};
        texture.fail_lock = 2;
        CHECK(upload_hand_texture(&texture, data) == D3DERR_DEVICELOST);
        CHECK(texture.locks == 3 && texture.unlocks == 2);
        texture = {};
        texture.fail_unlock = 2;
        CHECK(upload_hand_texture(&texture, data) == D3DERR_DEVICELOST);
        CHECK(texture.locks == 3 && texture.unlocks == 3);
        texture = {};
        texture.short_pitch = true;
        CHECK(upload_hand_texture(&texture, data) == D3DERR_INVALIDCALL);
        CHECK(texture.locks == 1 && texture.unlocks == 1);
    }

    void test_config()
    {
        Config config {};
        CHECK(!config.openxr_hand_tracking);
        Config changed = config;
        changed.openxr_hand_tracking = true;
        CHECK(!(changed == config));
        config = changed;
        CHECK(config.openxr_hand_tracking && config == changed);
        const auto path = std::filesystem::temp_directory_path() / std::format("openRBRVR-hand-test-{}", GetCurrentProcessId());
        config.runtime = OPENXR;
        CHECK(config.write(path));
        const auto loaded = Config::from_toml(path);
        CHECK(loaded.openxr_hand_tracking && loaded.runtime == OPENXR);
        const auto saved = toml::parse_file(path.string());
        CHECK(!saved["OpenXR"]["handMenu"] && !saved["OpenXR"]["handMenuGesture"] && !saved["OpenXR"]["handMenuIcon"]);
        config.openxr_hand_tracking = false;
        CHECK(config.write(path));
        CHECK(!Config::from_toml(path).openxr_hand_tracking);
        {
            std::ofstream legacy(path);
            legacy << "runtime = 'openxr'\n[OpenXR]\nworldScale = 1000\n";
        }
        CHECK(!Config::from_toml(path).openxr_hand_tracking);
        // Obsolete menu controls cannot disable the menu/icon when hands are on,
        // or enable anything when hands are off. Saving drops all three keys.
        for (const bool hands_enabled : { false, true }) {
            {
                std::ofstream legacy(path);
                legacy << "runtime = 'openxr'\n[OpenXR]\nhandTracking = " << (hands_enabled ? "true" : "false")
                       << "\nhandMenu = false\nhandMenuIcon = false\nhandMenuGesture = 'runtime'\n";
            }
            auto legacy = Config::from_toml(path);
            CHECK(legacy.openxr_hand_tracking == hands_enabled && legacy.write(path));
            const auto migrated = toml::parse_file(path.string());
            CHECK(!migrated["OpenXR"]["handMenu"] && !migrated["OpenXR"]["handMenuIcon"] && !migrated["OpenXR"]["handMenuGesture"]);
        }
        std::filesystem::remove(path);
    }

    void test_hand_button_persistence()
    {
        const auto directory = std::filesystem::temp_directory_path() / std::format("openRBRVR-layout-test-{}", GetCurrentProcessId());
        std::filesystem::create_directories(directory);
        const auto car_a = directory / "carA_personal.ini", car_b = directory / "carB_personal.ini";
        const auto start = glm::translate(M4(1), glm::vec3(0.2f, -0.1f, -0.4f))
            * glm::rotate(M4(1), 0.7f, glm::vec3(0, 1, 0));
        const auto help = glm::translate(M4(1), glm::vec3(-0.2f, 0.1f, -0.4f));
        CHECK(!HandButtonLayout::parse(""));
        CHECK(!HandButtonLayout::parse("0 0 0 0 0 0 0"));
        CHECK(!HandButtonLayout::parse("0 0 0 0 0 0 1 extra"));
        CHECK(!HandButtonLayout::parse("0 0 0 0 0 0"));
        CHECK(!HandButtonLayout::parse("nan 0 0 0 0 0 1"));
        CHECK(!HandButtonLayout::parse("4 0 0 0 0 0 1"));
        CHECK(!HandButtonLayout::parse("0 0 0 0 0 0 2"));
        CHECK(HandButtonLayout::load(car_a).has_value());
        CHECK(!HandButtonLayout::load(car_a)->poses[0]);
        {
            std::ofstream initial(car_a);
            initial << "[Cam_internal]\nPos=0.3 1 -1\n[openRBRVR]\nseatPosition=0.4 1.1 -0.9\n[OtherPlugin]\nkeep=unchanged\n";
        }
        CHECK(HandButtonLayout::save(car_a, 0, start));
        CHECK(HandButtonLayout::save(car_a, 1, help));
        auto layout = HandButtonLayout::load(car_a);
        CHECK(layout && layout->poses[0] && layout->poses[1]);
        check_pose(matrix_pose(*layout->poses[0]), matrix_pose(start));
        check_pose(matrix_pose(*layout->poses[1]), matrix_pose(help));
        ini::IniFile preserved(car_a.string());
        CHECK(preserved["Cam_internal"]["Pos"].as<std::string>() == "0.3 1 -1");
        CHECK(preserved["openRBRVR"]["seatPosition"].as<std::string>() == "0.4 1.1 -0.9");
        CHECK(preserved["OtherPlugin"]["keep"].as<std::string>() == "unchanged");
        CHECK(HandButtonLayout::save(car_b, 0, help));
        auto other = HandButtonLayout::load(car_b);
        CHECK(other && other->poses[0] && !other->poses[1]);
        check_pose(matrix_pose(*other->poses[0]), matrix_pose(help));
        const auto moved = glm::translate(M4(1), glm::vec3(0.1f, 0.2f, -0.5f));
        CHECK(HandButtonLayout::save(car_a, 0, moved));
        layout = HandButtonLayout::load(car_a);
        check_pose(matrix_pose(*layout->poses[0]), matrix_pose(moved));
        check_pose(matrix_pose(*layout->poses[1]), matrix_pose(help));
        CHECK(!HandButtonLayout::save(directory / "missing" / "car.ini", 0, start));
        CHECK(!HandButtonLayout::save(car_a, 2, start));
        auto invalid = start;
        invalid[3].x = std::numeric_limits<float>::infinity();
        CHECK(!HandButtonLayout::save(car_a, 0, invalid));
        // Replacement failure preserves the complete original personal INI.
        const auto before_failure = HandButtonLayout::load(car_a);
        CHECK(SetFileAttributesW(car_a.c_str(), FILE_ATTRIBUTE_READONLY));
        CHECK(!HandButtonLayout::save(car_a, 0, start));
        CHECK(SetFileAttributesW(car_a.c_str(), FILE_ATTRIBUTE_NORMAL));
        const auto after_failure = HandButtonLayout::load(car_a);
        check_pose(matrix_pose(*after_failure->poses[0]), matrix_pose(*before_failure->poses[0]));
        CHECK(!std::filesystem::exists(car_a.string() + ".openRBRVR.tmp"));
        // A bad entry cannot break the other button or VR initialization.
        preserved["openRBRVR"][HandButtonLayout::keys[0]] = "bad saved pose";
        preserved.save(car_a.string());
        const auto partial = HandButtonLayout::load(car_a);
        CHECK(partial && !partial->poses[0] && partial->poses[1]);
        HandMenu menu;
        menu.restore_buttons(layout->poses);
        CHECK(!menu.is_open() && menu.take_locked_button() == -1);
        CHECK(menu.start_button().state == HandPlacedButton::State::Locked);
        CHECK(menu.help_button().state == HandPlacedButton::State::Locked);
        std::array<HandTracking::Hand, 2> hands;
        for (int i = 0; i < 2; ++i) {
            hands[i].active = true;
            fill_joints(hands[i].joints.data(), i);
        }
        const auto p0 = glm::vec3(moved[3]), p1 = glm::vec3(help[3]);
        hands[0].joints[XR_HAND_JOINT_INDEX_TIP_EXT].pose.position = { p0.x, p0.y, p0.z };
        hands[1].joints[XR_HAND_JOINT_INDEX_TIP_EXT].pose.position = { p1.x, p1.y, p1.z };
        const XrPosef head { { 0, 0, 0, 1 }, { 0, 0, 0 } };
        for (int frame = 0; frame < 50; ++frame) {
            CHECK(menu.update(hands, head, true, frame * 0.1) == HandMenu::Action::None);
            CHECK(!menu.start_button().is_down() && !menu.help_button().is_down());
        }
        // Loading another car discards the previous help placement and contacts.
        menu.restore_buttons(other->poses);
        CHECK(menu.help_button().state == HandPlacedButton::State::Unplaced);
        check_pose(matrix_pose(menu.start_button().pose), matrix_pose(help));
        // A recenter changes live coordinates without rewriting the saved pose.
        const XrPosef rebase { { 0, 0, 0, 1 }, { 0.2f, 0, 0 } };
        menu.rebase_after_recenter(rebase);
        CHECK(menu.take_locked_button() == -1);
        CHECK(glm::distance(glm::vec3(menu.start_button().pose[3]), glm::vec3(help[3]) + glm::vec3(0.2f, 0, 0)) < 0.001f);
        check_pose(matrix_pose(*HandButtonLayout::load(car_b)->poses[0]), matrix_pose(help));
        // Resolve model identity again when a car slot is reused by RSF.
        const auto previous_directory = std::filesystem::current_path();
        std::filesystem::create_directory(directory / "Cars");
        std::filesystem::current_path(directory);
        {
            std::ofstream cars("Cars/cars.ini");
            cars << "[Car00]\nIniFile=Cars/models/carA.ini\n";
        }
        CHECK(Config::resolve_personal_car_ini_path(0) == std::filesystem::path("Cars/models/carA_personal.ini"));
        {
            std::ofstream cars("Cars/cars.ini");
            cars << "[Car00]\nIniFile=Cars/models/carB.ini\n";
        }
        CHECK(Config::resolve_personal_car_ini_path(0) == std::filesystem::path("Cars/models/carB_personal.ini"));
        std::filesystem::current_path(previous_directory);
        std::filesystem::remove(directory / "Cars" / "cars.ini");
        std::filesystem::remove(directory / "Cars");
        std::filesystem::remove(car_a);
        std::filesystem::remove(car_b);
        std::filesystem::remove(directory);
    }

    void test_hook_publication()
    {
        using namespace HookTest;
        entries = removals = 0;
        {
            Hook<Fn> hook;
            installed = &hook;
            hook.install(original, replacement);
            CHECK(entries == 1 && hook.call(1) == 2);
        }
        CHECK(removals == 1);
        {
            Hook<Fn> hook;
            installed = &hook;
            enable_failure = true;
            bool failed = false;
            try {
                hook.install(original, replacement);
            } catch (const std::runtime_error&) {
                failed = true;
            }
            CHECK(failed && !hook.call && !hook.src && removals == 2);
            enable_failure = false;
            create_failure = true;
            failed = false;
            try {
                hook.install(original, replacement);
            } catch (const std::runtime_error&) {
                failed = true;
            }
            CHECK(failed && !hook.call && !hook.src && removals == 2);
            create_failure = false;
        }
        CHECK(removals == 2); // Failed enable cleaned up exactly once.
        installed = nullptr;
    }

    void test_scene_ownership()
    {
        struct Device {
            bool in_scene = false;
            int begins = 0;
            int ends = 0;
            HRESULT failure = D3D_OK;
            HRESULT BeginScene()
            {
                ++begins;
                if (FAILED(failure))
                    return failure;
                if (in_scene)
                    return D3DERR_INVALIDCALL;
                in_scene = true;
                return D3D_OK;
            }
            HRESULT EndScene()
            {
                CHECK(in_scene);
                ++ends;
                in_scene = false;
                return D3D_OK;
            }
        } device;

        {
            D3DSceneScope scene(&device);
            CHECK(scene.valid() && scene.owned() && device.in_scene);
            CHECK(scene.finish() == D3D_OK && !device.in_scene);
        }
        CHECK(device.begins == 1 && device.ends == 1);
        device.in_scene = true;
        {
            D3DSceneScope scene(&device);
            CHECK(scene.valid() && !scene.owned());
            CHECK(scene.finish() == D3D_OK && device.in_scene);
        }
        CHECK(device.in_scene && device.ends == 1); // Never close RBR's scene.
        device.in_scene = false;
        device.failure = D3DERR_DEVICELOST;
        {
            D3DSceneScope scene(&device);
            CHECK(!scene.valid() && !scene.owned());
        }
        CHECK(device.ends == 1);
        device.failure = D3D_OK;
        {
            D3DSceneScope scene(&device);
            CHECK(scene.valid());
        }
        CHECK(!device.in_scene && device.ends == 2); // Early-return cleanup.
    }
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetSystemProperties(XrInstance actual_instance, XrSystemId system, XrSystemProperties* properties)
{
    CHECK(actual_instance == instance && system == 1);
    ++mock.queries;
    if (mock.property_failure)
        return XR_ERROR_RUNTIME_FAILURE;
    auto& hand = *static_cast<XrSystemHandTrackingPropertiesEXT*>(properties->next);
    CHECK(hand.type == XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT);
    hand.supportsHandTracking = mock.supports;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance actual_instance, const char* name, PFN_xrVoidFunction* function)
{
    CHECK(actual_instance == instance);
    *function = nullptr;
    if (mock.missing_function)
        return XR_ERROR_FUNCTION_UNSUPPORTED;
    if (std::string_view(name) == "xrCreateHandTrackerEXT")
        *function = reinterpret_cast<PFN_xrVoidFunction>(create_tracker);
    if (std::string_view(name) == "xrDestroyHandTrackerEXT")
        *function = reinterpret_cast<PFN_xrVoidFunction>(destroy_tracker);
    if (std::string_view(name) == "xrLocateHandJointsEXT")
        *function = reinterpret_cast<PFN_xrVoidFunction>(locate_joints);
    if (std::string_view(name) == "xrCreateReferenceSpace")
        *function = reinterpret_cast<PFN_xrVoidFunction>(create_view_space);
    if (std::string_view(name) == "xrDestroySpace")
        *function = reinterpret_cast<PFN_xrVoidFunction>(destroy_view_space);
    if (std::string_view(name) == "xrLocateSpace")
        *function = reinterpret_cast<PFN_xrVoidFunction>(locate_view_space);
    CHECK(*function);
    return XR_SUCCESS;
}

int main()
{
    test_capabilities();
    test_lifecycle();
    test_visibility();
    test_motion_compensation();
    test_hand_reference_failures();
    test_menu_gestures();
    test_menu_touch();
    test_menu_integration();
    test_menu_open_palm_and_either_hand();
    test_help_hold();
    test_start_button();
    test_start_placement();
    test_mesh();
    test_texture();
    test_config();
    test_hand_button_persistence();
    test_scene_ownership();
    test_hook_publication();
    std::cout << "Hand tests passed: capabilities, lifecycle, visibility, automatic motion compensation, reference failures, local menu gesture, touch, placement/reset, start cube/held ignition input, two-second help hold, Valve skinning, texture uploads, configuration, per-car button persistence, hook publication/failures, D3D scene ownership\n";
}
