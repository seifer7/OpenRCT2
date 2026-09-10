/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

namespace OpenRCT2::Drawing
{
    struct RenderTarget;
}

namespace OpenRCT2::Ui::View3D
{
    class OpenGLRenderer;

    class Presentation
    {
    public:
        void drawOverlay(Drawing::RenderTarget& rt);
        void present(OpenGLRenderer& renderer);
    };
} // namespace OpenRCT2::Ui::View3D
