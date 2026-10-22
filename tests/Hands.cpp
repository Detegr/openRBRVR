#include "Config.hpp"
#include "D3DSceneScope.hpp"
#include "HandMesh.hpp"
#include "HandTexture.hpp"
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
        if (locations->next) {
            auto& source = *static_cast<XrHandTrackingDataSourceStateEXT*>(locations->next);
            CHECK(source.type == XR_TYPE_HAND_TRACKING_DATA_SOURCE_STATE_EXT);
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
        const auto path = std::filesystem::temp_directory_path() / std::format("openRBRVR-hand-test-{}.toml", GetCurrentProcessId());
        config.runtime = OPENXR;
        CHECK(config.write(path));
        auto loaded = Config::from_toml(path);
        CHECK(loaded.openxr_hand_tracking && loaded.runtime == OPENXR);
        config.openxr_hand_tracking = false;
        CHECK(config.write(path));
        CHECK(!Config::from_toml(path).openxr_hand_tracking);
        {
            std::ofstream legacy(path);
            legacy << "runtime = 'openxr'\n[OpenXR]\nworldScale = 1000\n";
        }
        CHECK(!Config::from_toml(path).openxr_hand_tracking);
        std::filesystem::remove(path);
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
    test_mesh();
    test_texture();
    test_config();
    test_scene_ownership();
    std::cout << "Hand tests passed: capabilities, lifecycle, visibility, automatic motion compensation, reference failures, Valve skinning, texture uploads, configuration, D3D scene ownership\n";
}
