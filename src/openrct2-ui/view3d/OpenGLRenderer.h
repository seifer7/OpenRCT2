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
    class TileTextureCache;

    class OpenGLRenderer final : public Renderer
    {
    public:
        OpenGLRenderer();
        ~OpenGLRenderer() override;
        OpenGLRenderer(const OpenGLRenderer&) = delete;
        OpenGLRenderer& operator=(const OpenGLRenderer&) = delete;

        void beginFrame(int32_t width, int32_t height, const Camera& camera) override;
        void triangle(Vector a, Vector b, Vector c, uint8_t colour) override;
        void texturedTriangle(
            Vector a, Vector b, Vector c, Vector origin, const ImageId& image, uint8_t colour, bool cutout = false) override;
        void waterQuad(
            Vector a, Vector b, Vector c, Vector d, Vector origin, const ImageId& mask, const ImageId& overlay) override;
        void render();

    private:
        struct Vertex
        {
            Vector position;
            float colour;
            Vector texture{ 0, 0, -1 };
            float cutout{};
        };
        struct Resources;
        std::unique_ptr<Resources> _resources;
        std::unique_ptr<TileTextureCache> _textures;
        std::vector<Vertex> _vertices;
        std::vector<Vertex> _waterVertices;
        int32_t _waterOverlay = -1;
        Camera _camera;
        int32_t _width{}, _height{};
    };
} // namespace OpenRCT2::Ui::View3D
