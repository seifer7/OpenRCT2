/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "Renderer.h"

#include <memory>

namespace OpenRCT2::Ui::View3D
{
    class OpenGLRenderer final : public Renderer
    {
    public:
        OpenGLRenderer();
        ~OpenGLRenderer() override;
        OpenGLRenderer(const OpenGLRenderer&) = delete;
        OpenGLRenderer& operator=(const OpenGLRenderer&) = delete;

        void beginFrame(int32_t width, int32_t height, const Camera& camera) override;
        void triangle(Vector a, Vector b, Vector c, uint8_t colour) override;
        void render();

    private:
        struct Vertex
        {
            Vector position;
            float colour;
        };
        struct Resources;
        std::unique_ptr<Resources> _resources;
        std::vector<Vertex> _vertices;
        Camera _camera;
        int32_t _width{}, _height{};
    };
} // namespace OpenRCT2::Ui::View3D
