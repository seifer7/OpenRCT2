/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <gtest/gtest.h>
#include <openrct2-ui/view3d/Camera.h>

using namespace OpenRCT2::Ui::View3D;

TEST(View3DCamera, InitialViewTargetsMapCenter)
{
    Camera camera;
    const Vector center{ 320, 640, 48 };
    camera.initialise(center, 1280, 800, 600);
    const auto target = camera.toCamera(center);
    EXPECT_NEAR(target.x, 0, 0.001f);
    EXPECT_NEAR(target.y, 0, 0.001f);
    EXPECT_GT(target.z, 0);
}

TEST(View3DCamera, DiagonalMovementIsNormalised)
{
    Camera straight;
    Camera diagonal;
    straight.move(0.05f, { 1, 0, false }, 1024);
    diagonal.move(0.05f, { 1, 1, false }, 1024);
    EXPECT_NEAR(straight.position().dot(straight.position()), diagonal.position().dot(diagonal.position()), 0.001f);
}

TEST(View3DCamera, FastMovementAndTimeClampArePreserved)
{
    Camera normal;
    Camera fast;
    Camera delayed;
    normal.move(0.1f, { 1, 0, false }, 1024);
    fast.move(0.1f, { 1, 0, true }, 1024);
    delayed.move(10.0f, { 1, 0, false }, 1024);
    EXPECT_FLOAT_EQ(fast.position().y, normal.position().y * 4);
    EXPECT_FLOAT_EQ(fast.position().z, normal.position().z * 4);
    EXPECT_FLOAT_EQ(delayed.position().y, normal.position().y);
    EXPECT_FLOAT_EQ(delayed.position().z, normal.position().z);
}

TEST(View3DCamera, MouseRightTurnsMovementRightAndPitchRemainsFinite)
{
    Camera camera;
    camera.look(100, 0);
    camera.move(0.1f, { 1, 0, false }, 1024);
    EXPECT_GT(camera.position().x, 0);
    camera.look(0, -1000000);
    const auto before = camera.position();
    camera.move(0.1f, { 1, 0, false }, 1024);
    EXPECT_GT(camera.position().z, before.z);
    const auto point = camera.toCamera({ 0, 0, 0 });
    EXPECT_TRUE(std::isfinite(point.x));
    EXPECT_TRUE(std::isfinite(point.y));
    EXPECT_TRUE(std::isfinite(point.z));
}
