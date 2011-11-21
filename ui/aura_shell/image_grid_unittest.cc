// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura_shell/image_grid.h"
#include "ui/aura_shell/test/aura_shell_test_base.h"
#include "ui/gfx/image/image.h"

using aura_shell::internal::ImageGrid;

namespace aura_shell {
namespace test {

namespace {

// Creates a gfx::Image with the requested dimensions.
gfx::Image* CreateImage(const gfx::Size& size) {
  SkBitmap* bitmap = new SkBitmap();
  bitmap->setConfig(SkBitmap::kARGB_8888_Config, size.width(), size.height());
  return new gfx::Image(bitmap);
}

}  // namespace

typedef aura_shell::test::AuraShellTestBase ImageGridTest;

// Test that an ImageGrid's layers are transformed correctly when SetSize() is
// called.
TEST_F(ImageGridTest, Basic) {
  // Size of the images around the grid's border.
  const int kBorder = 2;

  scoped_ptr<gfx::Image> image_1x1(CreateImage(gfx::Size(1, 1)));
  scoped_ptr<gfx::Image> image_1xB(CreateImage(gfx::Size(1, kBorder)));
  scoped_ptr<gfx::Image> image_Bx1(CreateImage(gfx::Size(kBorder, 1)));
  scoped_ptr<gfx::Image> image_BxB(CreateImage(gfx::Size(kBorder, kBorder)));

  ImageGrid grid;
  grid.Init(image_BxB.get(), image_1xB.get(), image_BxB.get(),
            image_Bx1.get(), image_1x1.get(), image_Bx1.get(),
            image_BxB.get(), image_1xB.get(), image_BxB.get());

  ImageGrid::TestAPI test_api(&grid);
  ASSERT_TRUE(test_api.top_left_layer() != NULL);
  ASSERT_TRUE(test_api.top_layer() != NULL);
  ASSERT_TRUE(test_api.top_right_layer() != NULL);
  ASSERT_TRUE(test_api.left_layer() != NULL);
  ASSERT_TRUE(test_api.center_layer() != NULL);
  ASSERT_TRUE(test_api.right_layer() != NULL);
  ASSERT_TRUE(test_api.bottom_left_layer() != NULL);
  ASSERT_TRUE(test_api.bottom_layer() != NULL);
  ASSERT_TRUE(test_api.bottom_right_layer() != NULL);

  const gfx::Size size(20, 30);
  grid.SetSize(size);

  // The top-left layer should be flush with the top-left corner and unscaled.
  EXPECT_EQ(gfx::Rect(0, 0, kBorder, kBorder).ToString(),
            test_api.GetTransformedLayerBounds(
                *test_api.top_left_layer()).ToString());

  // The top layer should be flush with the top edge and stretched horizontally
  // between the two top corners.
  EXPECT_EQ(gfx::Rect(
                kBorder, 0, size.width() - 2 * kBorder, kBorder).ToString(),
            test_api.GetTransformedLayerBounds(
                *test_api.top_layer()).ToString());

  // The top-right layer should be flush with the top-right corner and unscaled.
  EXPECT_EQ(gfx::Rect(size.width() - kBorder, 0, kBorder, kBorder).ToString(),
            test_api.GetTransformedLayerBounds(
                *test_api.top_right_layer()).ToString());

  // The left layer should be flush with the left edge and stretched vertically
  // between the two left corners.
  EXPECT_EQ(gfx::Rect(
                0, kBorder, kBorder, size.height() - 2 * kBorder).ToString(),
            test_api.GetTransformedLayerBounds(
                *test_api.left_layer()).ToString());

  // The center layer should fill the space in the middle of the grid.
  EXPECT_EQ(gfx::Rect(
                kBorder, kBorder, size.width() - 2 * kBorder,
                size.height() - 2 * kBorder).ToString(),
            test_api.GetTransformedLayerBounds(
                *test_api.center_layer()).ToString());

  // The right layer should be flush with the right edge and stretched
  // vertically between the two right corners.
  EXPECT_EQ(gfx::Rect(
                size.width() - kBorder, kBorder,
                kBorder, size.height() - 2 * kBorder).ToString(),
            test_api.GetTransformedLayerBounds(
                *test_api.right_layer()).ToString());

  // The bottom-left layer should be flush with the bottom-left corner and
  // unscaled.
  EXPECT_EQ(gfx::Rect(0, size.height() - kBorder, kBorder, kBorder).ToString(),
            test_api.GetTransformedLayerBounds(
                *test_api.bottom_left_layer()).ToString());

  // The bottom layer should be flush with the bottom edge and stretched
  // horizontally between the two bottom corners.
  EXPECT_EQ(gfx::Rect(
                kBorder, size.height() - kBorder,
                size.width() - 2 * kBorder, kBorder).ToString(),
            test_api.GetTransformedLayerBounds(
                *test_api.bottom_layer()).ToString());

  // The bottom-right layer should be flush with the bottom-right corner and
  // unscaled.
  EXPECT_EQ(gfx::Rect(
                size.width() - kBorder, size.height() - kBorder,
                kBorder, kBorder).ToString(),
            test_api.GetTransformedLayerBounds(
                *test_api.bottom_right_layer()).ToString());
}

// Check that we don't crash if only a single image is supplied.
TEST_F(ImageGridTest, SingleImage) {
  const int kBorder = 1;
  scoped_ptr<gfx::Image> image(CreateImage(gfx::Size(kBorder, kBorder)));

  ImageGrid grid;
  grid.Init(NULL, image.get(), NULL,
            NULL, NULL, NULL,
            NULL, NULL, NULL);

  ImageGrid::TestAPI test_api(&grid);
  EXPECT_TRUE(test_api.top_left_layer() == NULL);
  ASSERT_TRUE(test_api.top_layer() != NULL);
  EXPECT_TRUE(test_api.top_right_layer() == NULL);
  EXPECT_TRUE(test_api.left_layer() == NULL);
  EXPECT_TRUE(test_api.center_layer() == NULL);
  EXPECT_TRUE(test_api.right_layer() == NULL);
  EXPECT_TRUE(test_api.bottom_left_layer() == NULL);
  EXPECT_TRUE(test_api.bottom_layer() == NULL);
  EXPECT_TRUE(test_api.bottom_right_layer() == NULL);

  const gfx::Size kSize(10, 10);
  grid.SetSize(kSize);

  // The top layer should be scaled horizontally across the entire width, but it
  // shouldn't be scaled vertically.
  EXPECT_EQ(gfx::Rect(0, 0, kSize.width(), kBorder).ToString(),
            test_api.GetTransformedLayerBounds(
                *test_api.top_layer()).ToString());
}

// Test that side (top, left, right, bottom) layers that are narrower than their
// adjacent corner layers stay pinned to the outside edges instead of getting
// moved inwards or scaled.  This exercises the scenario used for shadows.
TEST_F(ImageGridTest, SmallerSides) {
  const int kCorner = 2;
  const int kEdge = 1;

  scoped_ptr<gfx::Image> top_left_image(
      CreateImage(gfx::Size(kCorner, kCorner)));
  scoped_ptr<gfx::Image> top_image(CreateImage(gfx::Size(kEdge, kEdge)));
  scoped_ptr<gfx::Image> top_right_image(
      CreateImage(gfx::Size(kCorner, kCorner)));
  scoped_ptr<gfx::Image> left_image(CreateImage(gfx::Size(kEdge, kEdge)));
  scoped_ptr<gfx::Image> right_image(CreateImage(gfx::Size(kEdge, kEdge)));

  ImageGrid grid;
  grid.Init(top_left_image.get(), top_image.get(), top_right_image.get(),
            left_image.get(), NULL, right_image.get(),
            NULL, NULL, NULL);
  ImageGrid::TestAPI test_api(&grid);

  const gfx::Size kSize(20, 30);
  grid.SetSize(kSize);

  // The top layer should be flush with the top edge and stretched horizontally
  // between the two top corners.
  EXPECT_EQ(gfx::Rect(
                kCorner, 0, kSize.width() - 2 * kCorner, kEdge).ToString(),
            test_api.GetTransformedLayerBounds(
                *test_api.top_layer()).ToString());


  // The left layer should be flush with the left edge and stretched vertically
  // between the top left corner and the bottom.
  EXPECT_EQ(gfx::Rect(
                0, kCorner, kEdge, kSize.height() - kCorner).ToString(),
            test_api.GetTransformedLayerBounds(
                *test_api.left_layer()).ToString());

  // The right layer should be flush with the right edge and stretched
  // vertically between the top right corner and the bottom.
  EXPECT_EQ(gfx::Rect(
                kSize.width() - kEdge, kCorner,
                kEdge, kSize.height() - kCorner).ToString(),
            test_api.GetTransformedLayerBounds(
                *test_api.right_layer()).ToString());
}

}  // namespace test
}  // namespace aura_shell
