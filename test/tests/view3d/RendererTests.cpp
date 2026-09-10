/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <gtest/gtest.h>
#include <openrct2-ui/view3d/Renderer.h>

using namespace OpenRCT2::Ui::View3D;

namespace
{
    Vector FromDefaultCamera(float x, float y, float z)
    {
        constexpr float diagonal = 0.707106781f;
        return { x, (y + z) * diagonal, (y - z) * diagonal };
    }

    void DrawTriangle(Renderer& renderer, float depth, uint8_t colour)
    {
        renderer.triangle(
            FromDefaultCamera(-depth / 2, -depth / 2, depth), FromDefaultCamera(depth / 2, -depth / 2, depth),
            FromDefaultCamera(0, depth / 2, depth), colour);
    }
} // namespace

TEST(View3DRenderer, DepthBufferIsIndependentOfDrawOrder)
{
    Renderer nearFirst;
    Renderer farFirst;
    nearFirst.beginFrame(64, 64, Camera{});
    farFirst.beginFrame(64, 64, Camera{});
    DrawTriangle(nearFirst, 20, 46);
    DrawTriangle(nearFirst, 40, 94);
    DrawTriangle(farFirst, 40, 94);
    DrawTriangle(farFirst, 20, 46);
    EXPECT_TRUE(std::equal(nearFirst.pixels().begin(), nearFirst.pixels().end(), farFirst.pixels().begin()));
    EXPECT_NE(nearFirst.pixels()[32 * 64 + 32], 138);
}

TEST(View3DRenderer, TrianglesBehindCameraLeaveBackgroundUntouched)
{
    Renderer renderer;
    renderer.beginFrame(64, 64, Camera{});
    DrawTriangle(renderer, -20, 46);
    EXPECT_TRUE(std::all_of(renderer.pixels().begin(), renderer.pixels().end(), [](uint8_t pixel) { return pixel == 138; }));
}

TEST(View3DRenderer, ResizeClearsColourAndDepthBuffers)
{
    Renderer renderer;
    renderer.beginFrame(64, 64, Camera{});
    DrawTriangle(renderer, 20, 46);
    renderer.beginFrame(32, 48, Camera{});
    ASSERT_EQ(renderer.pixels().size(), 32u * 48u);
    EXPECT_TRUE(std::all_of(renderer.pixels().begin(), renderer.pixels().end(), [](uint8_t pixel) { return pixel == 138; }));
    DrawTriangle(renderer, 40, 94);
    EXPECT_NE(renderer.pixels()[24 * 32 + 16], 138);
}
