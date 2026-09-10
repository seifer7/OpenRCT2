/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <gtest/gtest.h>
#include <openrct2-ui/view3d/TerrainGeometry.h>
#include <openrct2/world/Map.h>

using namespace OpenRCT2;
using namespace OpenRCT2::Ui::View3D;

TEST(Map3DGeometry, TerrainMatchesGameHeightForEveryValidSlope)
{
    constexpr uint8_t slopes[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 23, 27, 29, 30 };
    for (const auto slope : slopes)
    {
        for (const auto base : { 16, 160, 1024 })
        {
            const auto vertices = TerrainVertices(base, slope);
            for (int32_t y = 0; y < kCoordsXYStep; ++y)
            {
                for (int32_t x = 0; x < kCoordsXYStep; ++x)
                {
                    bool found = false;
                    for (size_t side = 0; side < 4; ++side)
                    {
                        const auto a = vertices[side];
                        const auto b = vertices[(side + 1) % 4];
                        const auto c = vertices[4];
                        const auto edge = [](Vector p, Vector q, float px, float py) {
                            return (q.x - p.x) * (py - p.y) - (q.y - p.y) * (px - p.x);
                        };
                        const auto area = edge(a, b, c.x, c.y);
                        const auto u = edge(b, c, static_cast<float>(x), static_cast<float>(y)) / area;
                        const auto v = edge(c, a, static_cast<float>(x), static_cast<float>(y)) / area;
                        const auto w = 1 - u - v;
                        if (u >= 0 && v >= 0 && w >= 0)
                        {
                            const auto height = u * a.z + v * b.z + w * c.z;
                            // Game height sampling rounds half-unit slopes to integer world coordinates.
                            EXPECT_NEAR(height, TileElementHeight(CoordsXYZ{ x, y, base }, slope), 0.5f)
                                << "slope=" << static_cast<int>(slope) << " x=" << x << " y=" << y;
                            found = true;
                            break;
                        }
                    }
                    ASSERT_TRUE(found);
                }
            }
        }
    }
}

TEST(Map3DGeometry, CornersUseWorldXYOrientation)
{
    constexpr uint8_t slopes[] = { kTileSlopeSCornerUp, kTileSlopeECornerUp, kTileSlopeNCornerUp, kTileSlopeWCornerUp };
    for (size_t corner = 0; corner < 4; ++corner)
    {
        const auto vertices = TerrainVertices(32, slopes[corner]);
        for (size_t i = 0; i < 4; ++i)
            EXPECT_FLOAT_EQ(vertices[i].z, i == corner ? 48.0f : 32.0f);
    }
}

TEST(Map3DGeometry, ValleysHaveLowCentersAndSteepSlopesReachDoubleHeight)
{
    EXPECT_FLOAT_EQ(TerrainVertices(32, kTileSlopeWEValley)[4].z, 32);
    EXPECT_FLOAT_EQ(TerrainVertices(32, kTileSlopeNSValley)[4].z, 32);
    const auto vertices = TerrainVertices(32, kTileSlopeWCornerDown | kTileSlopeDiagonalFlag);
    EXPECT_FLOAT_EQ(vertices[1].z, 64);
    EXPECT_FLOAT_EQ(vertices[4].z, 48);
}

TEST(Map3DGeometry, NearClipRejectsTrianglesBehindCamera)
{
    std::array<Vector, 4> output;
    EXPECT_EQ(ClipNear({ Vector{ 0, 0, -2 }, Vector{ 1, 0, 1 }, Vector{ 0, 1, 2 } }, output, 4), 0u);
}

TEST(Map3DGeometry, NearClipPreservesVisibleTriangles)
{
    const std::array<Vector, 3> input = { Vector{ 0, 0, 4 }, Vector{ 8, 0, 8 }, Vector{ 0, 8, 8 } };
    std::array<Vector, 4> output;
    ASSERT_EQ(ClipNear(input, output, 4), 3u);
    for (size_t i = 0; i < input.size(); ++i)
    {
        EXPECT_FLOAT_EQ(output[i].x, input[i].x);
        EXPECT_FLOAT_EQ(output[i].y, input[i].y);
        EXPECT_FLOAT_EQ(output[i].z, input[i].z);
    }
}

TEST(Map3DGeometry, NearClipSplitsCrossingTriangles)
{
    std::array<Vector, 4> output;
    const auto count = ClipNear({ Vector{ 0, 0, 0 }, Vector{ 8, 0, 8 }, Vector{ 0, 8, 8 } }, output, 4);
    ASSERT_EQ(count, 4u);
    EXPECT_FLOAT_EQ(output[0].x, 0);
    EXPECT_FLOAT_EQ(output[0].y, 4);
    EXPECT_FLOAT_EQ(output[1].x, 4);
    EXPECT_FLOAT_EQ(output[1].y, 0);
    for (size_t i = 0; i < count; ++i)
        EXPECT_GE(output[i].z, 4);
    EXPECT_EQ(ClipNear({ Vector{ 0, 0, 8 }, Vector{ 8, 0, 0 }, Vector{ 0, 8, 0 } }, output, 4), 3u);
}

TEST(Map3DGeometry, PerspectiveProjectsCenterAndForeshortensDistance)
{
    const auto center = Project({ 0, 0, 10 }, 100, 800, 600);
    EXPECT_FLOAT_EQ(center.x, 400);
    EXPECT_FLOAT_EQ(center.y, 300);
    EXPECT_FLOAT_EQ(center.z, 0.1f);
    const auto nearPoint = Project({ 10, 10, 10 }, 100, 800, 600);
    const auto farPoint = Project({ 10, 10, 20 }, 100, 800, 600);
    EXPECT_FLOAT_EQ(nearPoint.x - center.x, 2 * (farPoint.x - center.x));
    EXPECT_FLOAT_EQ(center.y - nearPoint.y, 2 * (center.y - farPoint.y));
    EXPECT_GT(nearPoint.z, farPoint.z);
}
