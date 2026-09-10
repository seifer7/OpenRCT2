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

namespace OpenRCT2::Ui::View3D
{
    class Renderer;

    struct MapCameraStart
    {
        Vector center;
        float extent;
    };

    MapCameraStart GetMapCameraStart();
    float GetMapExtent();
    void DrawMap(Renderer& renderer, bool gridlines = false);
} // namespace OpenRCT2::Ui::View3D
