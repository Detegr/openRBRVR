#pragma once

#include "HandMesh.hpp"
#include <algorithm>
#include <cstring>

// DXT1 locks contain rows of 4x4 blocks, including the final 2x2 and 1x1 mips.
// Templated so pitch handling and failure cleanup can be tested without a GPU.
template <typename Texture>
HRESULT upload_hand_texture(Texture* texture, const HandTextureData& data)
{
    for (size_t level = 0; level < data.levels.size(); ++level) {
        const auto& mip = data.levels[level];
        const size_t row_bytes = std::max(1u, (mip.width + 3) / 4) * sizeof(uint64_t);
        const size_t rows = std::max(1u, (mip.height + 3) / 4);
        D3DLOCKED_RECT lock {};
        if (const auto result = texture->LockRect(static_cast<UINT>(level), &lock, nullptr, 0); FAILED(result)) {
            return result;
        }
        if (!lock.pBits || lock.Pitch < static_cast<int>(row_bytes)) {
            texture->UnlockRect(static_cast<UINT>(level));
            return D3DERR_INVALIDCALL;
        }
        const auto* source = reinterpret_cast<const unsigned char*>(data.blocks.data() + mip.block_offset);
        auto* destination = static_cast<unsigned char*>(lock.pBits);
        for (size_t row = 0; row < rows; ++row) {
            std::memcpy(destination + row * lock.Pitch, source + row * row_bytes, row_bytes);
        }
        if (const auto result = texture->UnlockRect(static_cast<UINT>(level)); FAILED(result)) {
            return result;
        }
    }
    return D3D_OK;
}
