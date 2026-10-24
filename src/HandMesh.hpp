#pragma once

#include "HandTracking.hpp"
#include "Util.hpp"
#include <span>
#include <vector>

struct HandVertex {
    glm::vec3 position;
    D3DCOLOR color;
    glm::vec2 uv;
};
static_assert(sizeof(HandVertex) == 6 * sizeof(float)); // XYZ | DIFFUSE | TEX1

struct HandMesh {
    std::vector<HandVertex> vertices;
    std::vector<uint16_t> indices;
    void clear()
    {
        vertices.clear();
        indices.clear();
    }
};

// Offline-converted Valve model data. Joint indices use XrHandJointEXT order;
// inverse binds are column-major glTF matrices in OpenXR metres and axes.
struct HandModelVertex {
    float position[3];
    float normal[3];
    float uv[2];
    uint8_t joints[4];
    float weights[4];
};
struct HandModel {
    std::span<const HandModelVertex> vertices;
    std::span<const uint16_t> indices;
    std::span<const std::array<float, 16>, XR_HAND_JOINT_COUNT_EXT> inverse_bind;
};
const HandModel& hand_model(XrHandEXT side);

struct HandTextureLevel {
    uint32_t width;
    uint32_t height;
    size_t block_offset;
};
struct HandTextureData {
    std::span<const HandTextureLevel> levels;
    std::span<const uint64_t> blocks; // Opaque BC1 / D3DFMT_DXT1, little-endian.
};
const HandTextureData& hand_texture_data();

// Appends opaque, shaded triangles in the same right-handed, metre-based
// reference space as the OpenXR views. No game camera or wheel anchoring.
void append_hand_mesh(const HandTracking::Hand& hand, XrHandEXT side, HandMesh& mesh);
