/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Presentation.h"

#include "OpenGLRenderer.h"

#include <openrct2/drawing/Text.h>

namespace OpenRCT2::Ui::View3D
{
    void Presentation::drawOverlay(Drawing::RenderTarget& rt)
    {
        drawText(
            rt, { 12, 12 }, "3D View | WASD: fly | Right-drag: look | Shift: faster | Esc: return",
            { Drawing::Colour::white });
    }

    void Presentation::present(OpenGLRenderer& renderer)
    {
        renderer.render();
    }
} // namespace OpenRCT2::Ui::View3D
