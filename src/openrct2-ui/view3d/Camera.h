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

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace OpenRCT2::Ui::View3D
{
    struct CameraInput
    {
        float forward{};
        float strafe{};
        bool fast{};
    };

    class Camera
    {
    public:
        Camera()
        {
            updateBasis();
        }

        void initialise(Vector center, float extent, int32_t width, int32_t height)
        {
            const auto distance = extent * std::max(1.5f, 1.5f * height / width) + 256.0f;
            _position = { center.x, center.y - distance, center.z + distance };
        }

        void look(int32_t dx, int32_t dy)
        {
            _yaw = std::remainder(_yaw - dx * 0.003f, 6.283185307f);
            _pitch = std::clamp(_pitch - dy * 0.003f, -1.55f, 1.55f);
            updateBasis();
        }

        void move(float dt, const CameraInput& input, float extent)
        {
            const auto motion = _forward * input.forward + _right * input.strafe;
            const auto length = std::sqrt(motion.dot(motion));
            if (length > 0)
            {
                const auto speed = std::max(128.0f, extent * 0.25f) * (input.fast ? 4.0f : 1.0f);
                _position = _position + motion * (speed * std::min(0.1f, dt) / length);
            }
        }

        Vector toCamera(Vector point) const
        {
            const auto delta = point - _position;
            return { delta.dot(_right), delta.dot(_up), delta.dot(_forward) };
        }

        Vector position() const
        {
            return _position;
        }

    private:
        Vector _position{}, _forward{}, _right{}, _up{};
        float _yaw = 1.570796327f;
        float _pitch = -0.785398163f;

        void updateBasis()
        {
            _forward = { std::cos(_yaw) * std::cos(_pitch), std::sin(_yaw) * std::cos(_pitch), std::sin(_pitch) };
            _right = { std::sin(_yaw), -std::cos(_yaw), 0 };
            _up = _right.cross(_forward);
        }
    };
} // namespace OpenRCT2::Ui::View3D
