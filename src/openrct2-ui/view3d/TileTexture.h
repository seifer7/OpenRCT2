/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "TerrainGeometry.h"

#include <algorithm>
#include <bit>
#include <openrct2/drawing/Drawing.Sprite.h>
#include <openrct2/drawing/G1Element.h>
#include <openrct2/drawing/ImageId.hpp>
#include <openrct2/drawing/PaletteIndex.h>
#include <openrct2/drawing/RenderTarget.h>
#include <span>
#include <unordered_map>
#include <vector>

namespace OpenRCT2::Ui::View3D
{
    constexpr int32_t kTileTextureSize = 32;
    using TilePixels = std::array<Drawing::PaletteIndex, kTileTextureSize * kTileTextureSize>;
    using FlatTileSprite = std::array<Drawing::PaletteIndex, 64 * 32>;

    // Native diamond scanlines cover two horizontal pixels per world texel. Sprite origin is (32, 0).
    inline TilePixels ExtractTileTexture(const FlatTileSprite& sprite)
    {
        TilePixels pixels{};
        for (int32_t y = 0; y < kTileTextureSize; ++y)
        {
            for (int32_t x = 0; x < kTileTextureSize; ++x)
            {
                const auto index = ((x + y) / 2) * 64 + 32 + y - x;
                const auto colour = sprite[index];
                pixels[y * kTileTextureSize + x] = colour != Drawing::PaletteIndex::transparent ? colour : sprite[index - 1];
            }
        }
        return pixels;
    }

    inline Vector TileTextureCoordinates(Vector point, Vector origin)
    {
        return { (point.x - origin.x) / kCoordsXYStep, (point.y - origin.y) / kCoordsXYStep, 0 };
    }

    inline std::array<Vector, 4> WaterVertices(Vector origin, float height)
    {
        return { Vector{ origin.x, origin.y, height }, Vector{ origin.x + kCoordsXYStep, origin.y, height },
                 Vector{ origin.x + kCoordsXYStep, origin.y + kCoordsXYStep, height },
                 Vector{ origin.x, origin.y + kCoordsXYStep, height } };
    }

    // Water-mask pixels select blend-map rows; unlike ordinary sprites they are not colour indices.
    // Row zero preserves the destination. The remaining rows match PaletteMap::Blend(src, dst).
    inline std::vector<Drawing::PaletteIndex> BuildWaterBlendPalette(const G1Element* source)
    {
        constexpr int32_t width = 256;
        const auto rows = source != nullptr && source->offset != nullptr && source->width == width
                && source->height > 0 && source->height < 256
            ? source->height
            : 0;
        std::vector<Drawing::PaletteIndex> pixels(static_cast<size_t>(rows + 1) * width);
        for (int32_t i = 0; i < width; ++i)
            pixels[i] = static_cast<Drawing::PaletteIndex>(i);
        if (rows != 0)
            std::copy_n(reinterpret_cast<const Drawing::PaletteIndex*>(source->offset), rows * width, pixels.data() + width);
        return pixels;
    }

    inline std::array<Vector, 5> PathVertices(int32_t baseZ, bool sloped, uint8_t direction)
    {
        constexpr uint8_t slopes[] = { kTileSlopeSWSideUp, kTileSlopeNWSideUp, kTileSlopeNESideUp, kTileSlopeSESideUp };
        return TerrainVertices(baseZ, sloped ? slopes[direction & 3] : kTileSlopeFlat);
    }

    inline uint8_t FlatPathImageOffset(uint8_t edgesAndCorners)
    {
        // Matches Paint.Path.cpp's rotation-zero surface sprite layout, including filled corners.
        constexpr uint8_t offsets[] = {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 20, 4, 5, 6, 22, 8, 9, 10, 26, 12, 13, 14, 36,
            0, 1, 2, 3, 4, 5, 21, 23, 8, 9, 10, 11, 12, 13, 33, 37, 0, 1, 2, 3, 4, 5, 6, 24, 8, 9, 10, 11, 12, 13, 14, 38,
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 29, 30, 34, 39, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 40,
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 35, 41, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 42,
            0, 1, 2, 3, 4, 5, 6, 7, 8, 25, 10, 27, 12, 31, 14, 43, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 28, 12, 13, 14, 44,
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 45, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 46,
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 32, 14, 47, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 48,
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 49, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 50,
        };
        return offsets[edgesAndCorners];
    }

    class TileTextureCache
    {
    public:
        struct Entry
        {
            G1Element source;
            TilePixels pixels;
            bool dirty = true;
        };

        int32_t getLayer(const ImageId& image)
        {
            const auto* source = image.HasValue() ? GfxGetG1Element(image) : nullptr;
            if (source == nullptr || source->offset == nullptr || source->width <= 0 || source->height <= 0
                || source->flags.has(G1Flag::isPalette))
                return -1;

            const auto key = std::bit_cast<uint64_t>(image);
            const auto found = _indices.find(key);
            if (found != _indices.end())
            {
                auto& entry = _entries[found->second];
                if (entry.source.offset != source->offset || entry.source.width != source->width
                    || entry.source.height != source->height || entry.source.xOffset != source->xOffset
                    || entry.source.yOffset != source->yOffset || entry.source.flags != source->flags)
                    entry = { *source, decode(image), true };
                return found->second;
            }

            const auto layer = static_cast<int32_t>(_entries.size());
            _entries.push_back({ *source, decode(image), true });
            _indices.emplace(key, layer);
            return layer;
        }

        std::span<Entry> entries()
        {
            return _entries;
        }

    private:
        std::unordered_map<uint64_t, int32_t> _indices;
        std::vector<Entry> _entries;

        static TilePixels decode(const ImageId& image)
        {
            FlatTileSprite sprite{};
            Drawing::RenderTarget rt{};
            rt.bits = sprite.data();
            rt.width = 64;
            rt.height = 32;
            GfxDrawSpriteSoftware(rt, image, { 32, 0 });
            return ExtractTileTexture(sprite);
        }
    };
} // namespace OpenRCT2::Ui::View3D
