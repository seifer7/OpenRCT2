/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "Geometry.h"

#include <bit>
#include <openrct2/world/Location.hpp>
#include <openrct2/world/tile_element/Slope.h>

namespace OpenRCT2::Ui::View3D
{
    // World XY, not the rotated isometric screen directions. The last vertex is the tile center.
    inline std::array<Vector, 5> TerrainVertices(int32_t baseZ, uint8_t slope)
    {
        const auto corners = GetSlopeCornerHeights(baseZ, slope);
        const auto raised = slope & kTileSlopeRaisedCornersMask;
        const auto count = std::popcount(static_cast<unsigned>(raised));
        float center = static_cast<float>(baseZ);
        if (count >= 3)
            center += kLandHeightStep;
        else if (count == 2 && raised != kTileSlopeWEValley && raised != kTileSlopeNSValley)
            center += kLandHeightStep / 2;

        return { Vector{ 0, 0, static_cast<float>(corners.south) },
                 Vector{ kCoordsXYStep, 0, static_cast<float>(corners.east) },
                 Vector{ kCoordsXYStep, kCoordsXYStep, static_cast<float>(corners.north) },
                 Vector{ 0, kCoordsXYStep, static_cast<float>(corners.west) },
                 Vector{ kCoordsXYStep / 2, kCoordsXYStep / 2, center } };
    }
} // namespace OpenRCT2::Ui::View3D
