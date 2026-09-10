/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "Camera.h"

#include <span>
#include <vector>

namespace OpenRCT2::Ui::View3D
{
    class Renderer
    {
    public:
        virtual ~Renderer() = default;
        virtual void beginFrame(int32_t width, int32_t height, const Camera& camera);
        virtual void triangle(Vector a, Vector b, Vector c, uint8_t colour);
        void quad(Vector a, Vector b, Vector c, Vector d, uint8_t colour);
        void box(Vector lower, Vector upper, uint8_t colour);
        std::span<const uint8_t> pixels() const
        {
            return _pixels;
        }

    private:
        Camera _camera;
        float _focalLength{};
        int32_t _width{}, _height{};
        std::vector<uint8_t> _pixels;
        std::vector<float> _depth;

        void rasterize(Vector a, Vector b, Vector c, uint8_t colour);
    };
} // namespace OpenRCT2::Ui::View3D
