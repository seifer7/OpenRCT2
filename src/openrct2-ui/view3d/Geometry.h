/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <array>
#include <cstddef>

namespace OpenRCT2::Ui::View3D
{
    struct Vector
    {
        float x{}, y{}, z{};

        Vector operator+(Vector b) const
        {
            return { x + b.x, y + b.y, z + b.z };
        }
        Vector operator-(Vector b) const
        {
            return { x - b.x, y - b.y, z - b.z };
        }
        Vector operator*(float scale) const
        {
            return { x * scale, y * scale, z * scale };
        }
        float dot(Vector b) const
        {
            return x * b.x + y * b.y + z * b.z;
        }
        Vector cross(Vector b) const
        {
            return { y * b.z - z * b.y, z * b.x - x * b.z, x * b.y - y * b.x };
        }
    };

    inline size_t ClipNear(const std::array<Vector, 3>& triangle, std::array<Vector, 4>& output, float nearZ)
    {
        size_t count = 0;
        auto previous = triangle.back();
        for (const auto current : triangle)
        {
            if ((current.z >= nearZ) != (previous.z >= nearZ))
            {
                const auto t = (nearZ - previous.z) / (current.z - previous.z);
                output[count++] = previous + (current - previous) * t;
                output[count - 1].z = nearZ;
            }
            if (current.z >= nearZ)
                output[count++] = current;
            previous = current;
        }
        return count;
    }

    inline Vector Project(Vector point, float focalLength, float width, float height)
    {
        const auto inverseZ = 1.0f / point.z;
        return { width * 0.5f + point.x * focalLength * inverseZ, height * 0.5f - point.y * focalLength * inverseZ, inverseZ };
    }
} // namespace OpenRCT2::Ui::View3D
