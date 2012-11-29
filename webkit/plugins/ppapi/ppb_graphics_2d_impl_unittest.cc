// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/safe_integer_conversions.h"
#include "webkit/plugins/ppapi/ppapi_unittest.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_graphics_2d_impl.h"

namespace webkit {
namespace ppapi {

typedef PpapiUnittest PpapiGraphics2DImplTest;

TEST_F(PpapiGraphics2DImplTest, ConvertToLogicalPixels) {
  static const struct {
    int x1;
    int y1;
    int w1;
    int h1;
    int x2;
    int y2;
    int w2;
    int h2;
    int dx1;
    int dy1;
    int dx2;
    int dy2;
    float scale;
    bool result;
  } tests[] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1.0, true },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2.0, true },
    { 0, 0, 4, 4, 0, 0, 2, 2, 0, 0, 0, 0, 0.5, true },
    { 1, 1, 4, 4, 0, 0, 3, 3, 0, 0, 0, 0, 0.5, false },
    { 53, 75, 100, 100, 53, 75, 100, 100, 0, 0, 0, 0, 1.0, true },
    { 53, 75, 100, 100, 106, 150, 200, 200, 0, 0, 0, 0, 2.0, true },
    { 53, 75, 100, 100, 26, 37, 51, 51, 0, 0, 0, 0, 0.5, false },
    { 53, 74, 100, 100, 26, 37, 51, 50, 0, 0, 0, 0, 0.5, false },
    { -1, -1, 100, 100, -1, -1, 51, 51, 0, 0, 0, 0, 0.5, false },
    { -2, -2, 100, 100, -1, -1, 50, 50, 4, -4, 2, -2, 0.5, true },
    { -101, -100, 50, 50, -51, -50, 26, 25, 0, 0, 0, 0, 0.5, false },
    { 10, 10, 20, 20, 5, 5, 10, 10, 0, 0, 0, 0, 0.5, true },
      // Cannot scroll due to partial coverage on sides
    { 11, 10, 20, 20, 5, 5, 11, 10, 0, 0, 0, 0, 0.5, false },
      // Can scroll since backing store is actually smaller/scaling up
    { 11, 20, 100, 100, 22, 40, 200, 200, 7, 3, 14, 6, 2.0, true },
      // Can scroll due to delta and bounds being aligned
    { 10, 10, 20, 20, 5, 5, 10, 10, 6, 4, 3, 2, 0.5, true },
      // Cannot scroll due to dx
    { 10, 10, 20, 20, 5, 5, 10, 10, 5, 4, 2, 2, 0.5, false },
      // Cannot scroll due to dy
    { 10, 10, 20, 20, 5, 5, 10, 10, 6, 3, 3, 1, 0.5, false },
      // Cannot scroll due to top
    { 10, 11, 20, 20, 5, 5, 10, 11, 6, 4, 3, 2, 0.5, false },
      // Cannot scroll due to left
    { 7, 10, 20, 20, 3, 5, 11, 10, 6, 4, 3, 2, 0.5, false },
      // Cannot scroll due to width
    { 10, 10, 21, 20, 5, 5, 11, 10, 6, 4, 3, 2, 0.5, false },
      // Cannot scroll due to height
    { 10, 10, 20, 51, 5, 5, 10, 26, 6, 4, 3, 2, 0.5, false },
      // Check negative scroll deltas
    { 10, 10, 20, 20, 5, 5, 10, 10, -6, -4, -3, -2, 0.5, true },
    { 10, 10, 20, 20, 5, 5, 10, 10, -6, -3, -3, -1, 0.5, false },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    gfx::Rect r1(tests[i].x1, tests[i].y1, tests[i].w1, tests[i].h1);
    gfx::Rect r2(tests[i].x2, tests[i].y2, tests[i].w2, tests[i].h2);
    gfx::Rect orig = r1;
    gfx::Point delta(tests[i].dx1, tests[i].dy1);
    bool res = webkit::ppapi::PPB_Graphics2D_Impl::ConvertToLogicalPixels(
        tests[i].scale, &r1, &delta);
    EXPECT_EQ(r2.ToString(), r1.ToString());
    EXPECT_EQ(res, tests[i].result);
    if (res) {
      EXPECT_EQ(delta, gfx::Point(tests[i].dx2, tests[i].dy2));
    }
    // Reverse the scale and ensure all the original pixels are still inside
    // the result.
    webkit::ppapi::PPB_Graphics2D_Impl::ConvertToLogicalPixels(
      1.0f / tests[i].scale, &r1, NULL);
    EXPECT_TRUE(r1.Contains(orig));
  }
}

/*
Disabled because this doesn't run with the new proxy design.
TODO(brettw) Rewrite this test to use the new design.

#if !defined(USE_AURA) && (defined(OS_WIN) || defined(OS_LINUX))
// Windows and Linux don't support scaled optimized plugin paints ATM.
#define MAYBE_GetBitmap2xScale DISABLED_GetBitmap2xScale
#else
#define MAYBE_GetBitmap2xScale GetBitmap2xScale
#endif

// Test that GetBitmapForOptimizedPluginPaint doesn't return a bitmap rect
// that's bigger than the actual backing store bitmap.
TEST_F(PpapiGraphics2DImplTest, GetBitmap2xScale) {
  PP_Size size;
  size.width = 3;
  size.height = 3;
  PP_Resource resource =
      PPB_Graphics2D_Impl::Create(instance()->pp_instance(), size, PP_TRUE);
  EXPECT_TRUE(resource);
  EXPECT_TRUE(instance()->BindGraphics(instance()->pp_instance(), resource));
  instance()->set_always_on_top(true);
  float scale = 2.0;
  SetViewSize(size.width, size.height, 1.0 / scale);

  gfx::Rect bounds(0, 0, 1, 1);
  gfx::Rect location;
  gfx::Rect clip;
  float bitmap_scale = 0;
  TransportDIB* dib = NULL;
  EXPECT_TRUE(instance()->GetBitmapForOptimizedPluginPaint(
      bounds, &dib, &location, &clip, &bitmap_scale));

  EXPECT_EQ(0, location.x());
  EXPECT_EQ(0, location.y());
  EXPECT_EQ(gfx::ToFlooredInt(size.width / scale), location.width());
  EXPECT_EQ(gfx::ToFlooredInt(size.height / scale), location.height());
  EXPECT_EQ(0, clip.x());
  EXPECT_EQ(0, clip.y());
  EXPECT_EQ(size.width, clip.width());
  EXPECT_EQ(size.height, clip.height());
  EXPECT_EQ(scale, bitmap_scale);
}
*/

}  // namespace ppapi
}  // namespace webkit
