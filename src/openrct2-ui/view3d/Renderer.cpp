/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Renderer.h"

namespace OpenRCT2::Ui::View3D
{
    void Renderer::beginFrame(int32_t width, int32_t height, const Camera& camera)
    {
        _width = width;
        _height = height;
        _camera = camera;
        _focalLength = static_cast<float>(height) * 0.8660254f;
        _pixels.resize(static_cast<size_t>(width) * height);
        _depth.resize(_pixels.size());
        std::fill(_pixels.begin(), _pixels.end(), 138);
        std::fill(_depth.begin(), _depth.end(), 0.0f);
    }

    void Renderer::box(Vector lower, Vector upper, uint8_t colour)
    {
        const std::array<Vector, 4> bottom = { lower, Vector{ upper.x, lower.y, lower.z }, Vector{ upper.x, upper.y, lower.z },
                                               Vector{ lower.x, upper.y, lower.z } };
        auto top = bottom;
        for (auto& vertex : top)
            vertex.z = upper.z;
        quad(top[0], top[1], top[2], top[3], colour);
        quad(bottom[3], bottom[2], bottom[1], bottom[0], colour);
        for (size_t i = 0; i < 4; ++i)
        {
            const auto j = (i + 1) % 4;
            quad(bottom[i], bottom[j], top[j], top[i], colour);
        }
    }

    void Renderer::quad(Vector a, Vector b, Vector c, Vector d, uint8_t colour)
    {
        triangle(a, b, c, colour);
        triangle(a, c, d, colour);
    }

    void Renderer::triangle(Vector a, Vector b, Vector c, uint8_t colour)
    {
        const auto normal = (b - a).cross(c - a);
        const auto length = std::sqrt(normal.dot(normal));
        if (length < 0.001f)
            return;
        const auto light = std::abs(normal.dot({ -0.3f, -0.4f, 0.8660254f })) / length;
        colour += static_cast<uint8_t>(3 + light * 6);
        std::array<Vector, 4> clipped;
        const auto count = ClipNear({ _camera.toCamera(a), _camera.toCamera(b), _camera.toCamera(c) }, clipped, 4.0f);
        if (count < 3)
            return;
        for (size_t i = 0; i < count; ++i)
            clipped[i] = Project(clipped[i], _focalLength, static_cast<float>(_width), static_cast<float>(_height));
        for (size_t i = 1; i + 1 < count; ++i)
            rasterize(clipped[0], clipped[i], clipped[i + 1], colour);
    }

    void Renderer::rasterize(Vector a, Vector b, Vector c, uint8_t colour)
    {
        const auto edge = [](Vector p, Vector q, float x, float y) {
            return (q.x - p.x) * (y - p.y) - (q.y - p.y) * (x - p.x);
        };
        const float area = edge(a, b, c.x, c.y);
        if (std::abs(area) < 0.001f)
            return;
        const auto minX = std::max(0.0f, std::floor(std::min({ a.x, b.x, c.x })));
        const auto minY = std::max(0.0f, std::floor(std::min({ a.y, b.y, c.y })));
        const auto maxX = std::min(static_cast<float>(_width - 1), std::ceil(std::max({ a.x, b.x, c.x })));
        const auto maxY = std::min(static_cast<float>(_height - 1), std::ceil(std::max({ a.y, b.y, c.y })));
        if (minX > maxX || minY > maxY)
            return;
        for (auto y = static_cast<int32_t>(minY); y <= static_cast<int32_t>(maxY); ++y)
        {
            for (auto x = static_cast<int32_t>(minX); x <= static_cast<int32_t>(maxX); ++x)
            {
                const auto u = edge(b, c, x + 0.5f, y + 0.5f) / area;
                const auto v = edge(c, a, x + 0.5f, y + 0.5f) / area;
                const auto w = 1.0f - u - v;
                if (u < 0 || v < 0 || w < 0)
                    continue;
                const auto inverseZ = u * a.z + v * b.z + w * c.z;
                const auto index = static_cast<size_t>(y) * _width + x;
                if (inverseZ > _depth[index])
                {
                    _depth[index] = inverseZ;
                    _pixels[index] = colour;
                }
            }
        }
    }
} // namespace OpenRCT2::Ui::View3D
