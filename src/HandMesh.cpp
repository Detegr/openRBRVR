#include "HandMesh.hpp"
#include <algorithm>
#include <cmath>

void append_hand_mesh(const HandTracking::Hand& hand, XrHandEXT side, HandMesh& mesh)
{
    if (!hand.active) {
        return;
    }
    const auto& model = hand_model(side);
    // Both embedded hands fit in one 16-bit indexed draw. Avoid wrapping if a
    // caller accidentally appends the same hand repeatedly.
    if (mesh.vertices.size() + model.vertices.size() > UINT16_MAX) {
        return;
    }
    std::array<M4, XR_HAND_JOINT_COUNT_EXT> skin;
    for (size_t i = 0; i < skin.size(); ++i) {
        const auto& pose = hand.joints[i].pose;
        const auto& q = pose.orientation;
        auto transform = glm::mat4_cast(glm::normalize(glm::quat(q.w, q.x, q.y, q.z)));
        transform[3] = { pose.position.x, pose.position.y, pose.position.z, 1.0f };
        // Located joints are already absolute in the recentered reference space.
        // Do not multiply by the wrist again or accumulate parent transforms.
        skin[i] = transform * glm::make_mat4(model.inverse_bind[i].data());
    }

    const auto base = static_cast<uint16_t>(mesh.vertices.size());
    mesh.vertices.reserve(mesh.vertices.size() + model.vertices.size());
    mesh.indices.reserve(mesh.indices.size() + model.indices.size());
    const auto light_direction = glm::normalize(glm::vec3(-0.3f, 0.8f, 0.5f));
    for (const auto& source : model.vertices) {
        const glm::vec4 bind_position(source.position[0], source.position[1], source.position[2], 1.0f);
        const glm::vec4 bind_normal(source.normal[0], source.normal[1], source.normal[2], 0.0f);
        glm::vec4 position(0.0f);
        glm::vec4 normal(0.0f);
        for (size_t i = 0; i < 4; ++i) {
            if (source.weights[i] > 0.0f) {
                const auto& transform = skin[source.joints[i]];
                position += source.weights[i] * (transform * bind_position);
                normal += source.weights[i] * (transform * bind_normal);
            }
        }
        const float length = glm::dot(glm::vec3(normal), glm::vec3(normal));
        const auto unit_normal = length > 0.000001f ? glm::vec3(normal) / std::sqrt(length) : glm::vec3(0, 1, 0);
        const float light = 0.70f + 0.30f * std::max(0.0f, glm::dot(unit_normal, light_direction));
        const auto shade = static_cast<int>(255 * light);
        mesh.vertices.push_back({ glm::vec3(position), D3DCOLOR_XRGB(shade, shade, shade), { source.uv[0], source.uv[1] } });
    }
    for (const auto index : model.indices) {
        mesh.indices.push_back(base + index);
    }
}
