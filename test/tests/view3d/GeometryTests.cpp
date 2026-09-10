/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <algorithm>
#include <gtest/gtest.h>
#include <openrct2-ui/view3d/TerrainGeometry.h>
#include <openrct2-ui/view3d/TileTexture.h>
#include <openrct2/drawing/PaletteMap.h>
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

TEST(Map3DTextures, UnprojectsDiamondWithoutMirroring)
{
    FlatTileSprite sprite{};
    sprite[32] = static_cast<Drawing::PaletteIndex>(10);
    sprite[15 * 64 + 1] = static_cast<Drawing::PaletteIndex>(20);
    sprite[31 * 64 + 32] = static_cast<Drawing::PaletteIndex>(30);
    sprite[15 * 64 + 63] = static_cast<Drawing::PaletteIndex>(40);
    const auto pixels = ExtractTileTexture(sprite);
    EXPECT_EQ(pixels[0], static_cast<Drawing::PaletteIndex>(10));
    EXPECT_EQ(pixels[31], static_cast<Drawing::PaletteIndex>(20));
    EXPECT_EQ(pixels[31 * 32 + 31], static_cast<Drawing::PaletteIndex>(30));
    EXPECT_EQ(pixels[31 * 32], static_cast<Drawing::PaletteIndex>(40));
}

TEST(Map3DTextures, PreservesPaletteAndTransparentPathPixels)
{
    FlatTileSprite sprite{};
    sprite[16 * 64 + 32] = static_cast<Drawing::PaletteIndex>(154);
    const auto pixels = ExtractTileTexture(sprite);
    EXPECT_EQ(pixels[16 * 32 + 16], static_cast<Drawing::PaletteIndex>(154));
    EXPECT_EQ(pixels[0], Drawing::PaletteIndex::transparent);
    EXPECT_EQ(pixels[31 * 32 + 31], Drawing::PaletteIndex::transparent);
    sprite.fill(static_cast<Drawing::PaletteIndex>(246));
    const auto solid = ExtractTileTexture(sprite);
    EXPECT_TRUE(std::all_of(solid.begin(), solid.end(), [](auto pixel) {
        return pixel == static_cast<Drawing::PaletteIndex>(246);
    }));
}

TEST(Map3DTextures, NativeDiamondHasNoTransparentTileSeams)
{
    FlatTileSprite sprite{};
    for (int32_t y = 0; y < 32; ++y)
    {
        const auto halfWidth = 2 * std::min(y + 1, 32 - y);
        for (int32_t x = 32 - halfWidth; x < 32 + halfWidth; ++x)
            sprite[y * 64 + x] = static_cast<Drawing::PaletteIndex>(144);
    }
    const auto pixels = ExtractTileTexture(sprite);
    EXPECT_TRUE(std::all_of(pixels.begin(), pixels.end(), [](auto pixel) {
        return pixel == static_cast<Drawing::PaletteIndex>(144);
    }));
}

TEST(Map3DTextures, ConnectedPathsReachBothEdgesWithoutFillingTheVerges)
{
    for (const bool alongX : { false, true })
    {
        FlatTileSprite sprite{};
        for (int32_t y = 0; y < 32; ++y)
        {
            for (int32_t x = 0; x < 64; ++x)
            {
                const float worldX = y + 0.5f - (x - 31.5f) * 0.5f;
                const float worldY = y + 0.5f + (x - 31.5f) * 0.5f;
                const auto along = alongX ? worldX : worldY;
                const auto across = alongX ? worldY : worldX;
                if (along >= 0 && along < 32 && across >= 4 && across < 28)
                    sprite[y * 64 + x] = static_cast<Drawing::PaletteIndex>(154);
            }
        }
        const auto pixels = ExtractTileTexture(sprite);
        for (int32_t i = 8; i < 24; ++i)
        {
            EXPECT_EQ(pixels[alongX ? i * 32 : i], static_cast<Drawing::PaletteIndex>(154));
            EXPECT_EQ(pixels[alongX ? i * 32 + 31 : 31 * 32 + i], static_cast<Drawing::PaletteIndex>(154));
            EXPECT_EQ(pixels[alongX ? i : i * 32], Drawing::PaletteIndex::transparent);
            EXPECT_EQ(pixels[alongX ? 31 * 32 + i : i * 32 + 31], Drawing::PaletteIndex::transparent);
        }
    }
}

TEST(Map3DTextures, CoordinatesFollowWorldXYNotHeightOrCamera)
{
    const Vector origin{ 320, 640, 0 };
    const auto near = TileTextureCoordinates({ 320, 640, 160 }, origin);
    const auto far = TileTextureCoordinates({ 352, 672, 192 }, origin);
    const auto center = TileTextureCoordinates({ 336, 656, 176 }, origin);
    EXPECT_FLOAT_EQ(near.x, 0);
    EXPECT_FLOAT_EQ(near.y, 0);
    EXPECT_FLOAT_EQ(far.x, 1);
    EXPECT_FLOAT_EQ(far.y, 1);
    EXPECT_FLOAT_EQ(center.x, 0.5f);
    EXPECT_FLOAT_EQ(center.y, 0.5f);
}

TEST(Map3DTextures, PathSpriteSelectionPreservesEdgesAndFilledCorners)
{
    for (uint8_t edges = 0; edges < 16; ++edges)
        EXPECT_EQ(FlatPathImageOffset(edges), edges);
    EXPECT_EQ(FlatPathImageOffset(0x13), 20);
    EXPECT_EQ(FlatPathImageOffset(0x26), 21);
    EXPECT_EQ(FlatPathImageOffset(0x4C), 29);
    EXPECT_EQ(FlatPathImageOffset(0x89), 25);
    EXPECT_EQ(FlatPathImageOffset(0xFF), 50);
}

TEST(Map3DTextures, PathRampsHaveCorrectDirectionAndHeight)
{
    constexpr bool raised[4][4] = { { true, false, false, true }, { false, false, true, true },
                                    { false, true, true, false }, { true, true, false, false } };
    for (uint8_t direction = 0; direction < 4; ++direction)
    {
        const auto vertices = PathVertices(32, true, direction);
        for (size_t corner = 0; corner < 4; ++corner)
            EXPECT_FLOAT_EQ(vertices[corner].z, raised[direction][corner] ? 48.0f : 32.0f);
        EXPECT_FLOAT_EQ(vertices[4].z, 40);
        const auto flat = PathVertices(32, false, direction);
        for (auto vertex : flat)
            EXPECT_FLOAT_EQ(vertex.z, 32);
    }
}

TEST(Map3DTextures, MissingMaterialDoesNotAllocateATexture)
{
    TileTextureCache cache;
    EXPECT_EQ(cache.getLayer(ImageId()), -1);
    EXPECT_TRUE(cache.entries().empty());
}

TEST(Map3DWater, BlendTableMatchesGamePaletteAndRetainsTerrainDetail)
{
    std::array<Drawing::PaletteIndex, 6 * 256> maps{};
    for (size_t row = 0; row < 6; ++row)
    {
        for (size_t colour = 0; colour < 256; ++colour)
            maps[row * 256 + colour] = static_cast<Drawing::PaletteIndex>((colour + (row + 1) * 17) % 256);
    }
    G1Element source{};
    source.offset = reinterpret_cast<uint8_t*>(maps.data());
    source.width = 256;
    source.height = 6;
    const Drawing::PaletteMap gameMap(maps.data(), 6, 256);
    const auto texture = BuildWaterBlendPalette(&source);
    ASSERT_EQ(texture.size(), 7u * 256);
    for (size_t colour = 0; colour < 256; ++colour)
    {
        EXPECT_EQ(texture[colour], static_cast<Drawing::PaletteIndex>(colour));
        for (size_t row = 1; row <= 6; ++row)
            EXPECT_EQ(
                texture[row * 256 + colour],
                gameMap.Blend(static_cast<Drawing::PaletteIndex>(row), static_cast<Drawing::PaletteIndex>(colour)));
    }
    EXPECT_NE(texture[256 + 144], texture[256 + 150]);
}

TEST(Map3DWater, MissingOrInvalidBlendTablePreservesUnderlyingScene)
{
    G1Element malformed{};
    uint8_t pixel = 0;
    malformed.offset = &pixel;
    malformed.width = 1;
    malformed.height = 1;
    for (const auto* source : { static_cast<const G1Element*>(nullptr), static_cast<const G1Element*>(&malformed) })
    {
        const auto texture = BuildWaterBlendPalette(source);
        ASSERT_EQ(texture.size(), 256u);
        for (size_t colour = 0; colour < 256; ++colour)
            EXPECT_EQ(texture[colour], static_cast<Drawing::PaletteIndex>(colour));
    }
}

TEST(Map3DWater, NativeMaskLevelsAreNotConvertedToOpaqueColours)
{
    FlatTileSprite sprite{};
    sprite[32] = static_cast<Drawing::PaletteIndex>(1);
    sprite[15 * 64 + 1] = static_cast<Drawing::PaletteIndex>(2);
    sprite[31 * 64 + 32] = static_cast<Drawing::PaletteIndex>(6);
    const auto texture = ExtractTileTexture(sprite);
    EXPECT_EQ(texture[0], static_cast<Drawing::PaletteIndex>(1));
    EXPECT_EQ(texture[31], static_cast<Drawing::PaletteIndex>(2));
    EXPECT_EQ(texture[31 * 32 + 31], static_cast<Drawing::PaletteIndex>(6));
    EXPECT_EQ(texture[31 * 32], Drawing::PaletteIndex::transparent);
}

TEST(Map3DWater, SurfaceIsFlatAtWaterHeightAndMeetsAdjacentTiles)
{
    const Vector origin{ 320, 640, 16 };
    const auto vertices = WaterVertices(origin, 96);
    const auto next = WaterVertices({ 352, 640, 128 }, 96);
    for (const auto vertex : vertices)
        EXPECT_FLOAT_EQ(vertex.z, 96);
    EXPECT_FLOAT_EQ(vertices[1].x, next[0].x);
    EXPECT_FLOAT_EQ(vertices[1].y, next[0].y);
    EXPECT_FLOAT_EQ(vertices[2].x, next[3].x);
    EXPECT_FLOAT_EQ(vertices[2].y, next[3].y);
    const auto uv = TileTextureCoordinates(vertices[2], origin);
    EXPECT_FLOAT_EQ(uv.x, 1);
    EXPECT_FLOAT_EQ(uv.y, 1);
}
