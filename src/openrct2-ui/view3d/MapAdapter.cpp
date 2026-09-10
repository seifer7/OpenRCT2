/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "MapAdapter.h"

#include "Renderer.h"
#include "TerrainGeometry.h"

#include <openrct2/GameState.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/TileElementsView.h>
#include <openrct2/world/tile_element/SurfaceElement.h>
#include <openrct2/world/tile_element/TileElement.h>

namespace OpenRCT2::Ui::View3D
{
    float GetMapExtent()
    {
        const auto size = getGameState().mapSize;
        return static_cast<float>(std::max(size.x, size.y) * kCoordsXYStep);
    }

    MapCameraStart GetMapCameraStart()
    {
        const auto size = getGameState().mapSize;
        const float x = size.x * kCoordsXYStep * 0.5f;
        const float y = size.y * kCoordsXYStep * 0.5f;
        const auto z = static_cast<float>(TileElementHeight({ static_cast<int32_t>(x), static_cast<int32_t>(y) }));
        return { { x, y, z }, GetMapExtent() };
    }

    void DrawMap(Renderer& renderer)
    {
        const auto size = getGameState().mapSize;
        for (int32_t y = 0; y < size.y; ++y)
        {
            for (int32_t x = 0; x < size.x; ++x)
            {
                const auto* surface = MapGetSurfaceElementAt(TileCoordsXY{ x, y });
                if (surface == nullptr)
                    continue;
                const Vector origin{ static_cast<float>(x * kCoordsXYStep), static_cast<float>(y * kCoordsXYStep), 0 };
                auto vertices = TerrainVertices(surface->getBaseZ(), surface->getSlope());
                for (auto& vertex : vertices)
                    vertex = vertex + origin;
                for (size_t i = 0; i < 4; ++i)
                {
                    const auto a = vertices[i];
                    const auto b = vertices[(i + 1) % 4];
                    renderer.triangle(a, b, vertices[4], 144);
                    // Each tile is a solid column, including discontinuous edges and the map boundary.
                    renderer.quad(a, { a.x, a.y, 0 }, { b.x, b.y, 0 }, b, 22);
                }
                if (surface->getWaterHeight() != 0)
                {
                    const auto z = static_cast<float>(surface->getWaterHeight());
                    renderer.quad(
                        { origin.x, origin.y, z }, { origin.x + 32, origin.y, z }, { origin.x + 32, origin.y + 32, z },
                        { origin.x, origin.y + 32, z }, 130);
                }
                for (const auto* element : TileElementsView<TileElement>(TileCoordsXY{ x, y }))
                {
                    const auto type = element->getType();
                    if (type == TileElementType::surface || element->isGhost() || element->isInvisible())
                        continue;
                    uint8_t colour = 202;
                    float inset = 4;
                    switch (type)
                    {
                        case TileElementType::track:
                            colour = 166;
                            break;
                        case TileElementType::path:
                            colour = 10;
                            inset = 1;
                            break;
                        case TileElementType::smallScenery:
                            colour = 94;
                            inset = 9;
                            break;
                        case TileElementType::largeScenery:
                            colour = 154;
                            break;
                        case TileElementType::wall:
                            colour = 34;
                            inset = 10;
                            break;
                        case TileElementType::entrance:
                            colour = 46;
                            break;
                        default:
                            break;
                    }
                    const float base = static_cast<float>(element->getBaseZ());
                    const float top = static_cast<float>(std::max(element->getBaseZ() + 4, element->getClearanceZ()));
                    renderer.box(
                        { origin.x + inset, origin.y + inset, base }, { origin.x + 32 - inset, origin.y + 32 - inset, top },
                        colour);
                }
            }
        }
    }
} // namespace OpenRCT2::Ui::View3D
