#include "HandMesh.hpp"
#include <bit>

namespace {
#include "../assets/valve_hands/left.inc"
#include "../assets/valve_hands/right.inc"
#include "../assets/valve_hands/texture.inc"

    const HandModel models[] {
        { left_vertices, left_indices, left_inverse_bind },
        { right_vertices, right_indices, right_inverse_bind },
    };
    const HandTextureData texture { texture_levels, texture_blocks };
}

const HandModel& hand_model(XrHandEXT side)
{
    return models[side == XR_HAND_RIGHT_EXT ? 1 : 0];
}

const HandTextureData& hand_texture_data()
{
    return texture;
}
