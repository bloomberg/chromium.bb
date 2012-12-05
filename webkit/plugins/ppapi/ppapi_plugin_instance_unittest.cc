// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/safe_integer_conversions.h"
#include "webkit/plugins/ppapi/ppapi_unittest.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"

namespace webkit {
namespace ppapi {

class PpapiPluginInstanceTest : public PpapiUnittest {
 public:
  bool GetBitmapForOptimizedPluginPaint(const gfx::Rect& paint_bounds,
                                        TransportDIB** dib,
                                        gfx::Rect* location,
                                        gfx::Rect* clip,
                                        float* scale_factor) {
    return !!instance()->GetBitmapForOptimizedPluginPaint(
        paint_bounds, dib, location, clip, scale_factor);
  }
};

#if !defined(USE_AURA) && (defined(OS_WIN) || defined(OS_LINUX))
// Windows and Linux don't support scaled optimized plugin paints ATM.
#define MAYBE_GetBitmap2xScale DISABLED_GetBitmap2xScale
#else
#define MAYBE_GetBitmap2xScale GetBitmap2xScale
#endif

// Test that GetBitmapForOptimizedPluginPaint doesn't return a bitmap rect
// that's bigger than the actual backing store bitmap.
TEST_F(PpapiPluginInstanceTest, MAYBE_GetBitmap2xScale) {
  PP_Size size;
  size.width = 3;
  size.height = 3;

  scoped_refptr<PPB_ImageData_Impl> image_data = new PPB_ImageData_Impl(
      instance()->pp_instance(), PPB_ImageData_Impl::PLATFORM);
  image_data->Init(PPB_ImageData_Impl::GetNativeImageDataFormat(),
                   size.width, size.height, true);
  ASSERT_TRUE(image_data->Map() != NULL);

  instance()->set_always_on_top(true);
  SetViewSize(size.width, size.height);

  float scale = 2.0;
  gfx::Rect bounds(0, 0, 1, 1);
  gfx::Rect location;
  gfx::Rect clip;
  float bitmap_scale = 0;
  TransportDIB* dib = NULL;
  EXPECT_TRUE(GetBitmapForOptimizedPluginPaint(
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

}  // namespace ppapi
}  // namespace webkit
