// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/safe_integer_conversions.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppapi_unittest.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"

namespace webkit {
namespace ppapi {

namespace {

// A mock graphics object to bind. We only have to implement enough so that
// GetBitmapForOptimizedPluginPaint runs.
//
// If this is eventually all moved to content, we can simplify this. The
// point here is that we need to just have a bound graphics 2d resource host
// with the right properties.
class MockPlatformGraphics2D : public PluginDelegate::PlatformGraphics2D {
 public:
  // The image data pointer must outlive this class.
  MockPlatformGraphics2D(PPB_ImageData_Impl* image_data, float scale)
      : image_data_(image_data),
        scale_(scale),
        bound_instance_(NULL) {
  }
  virtual ~MockPlatformGraphics2D() {
    if (bound_instance_)
      bound_instance_->SetBoundGraphics2DForTest(NULL);
  }

  virtual bool ReadImageData(PP_Resource image,
                             const PP_Point* top_left) OVERRIDE {
    return false;
  }
  virtual bool BindToInstance(PluginInstance* new_instance) OVERRIDE {
    bound_instance_ = new_instance;
    return true;
  }
  virtual void Paint(WebKit::WebCanvas* canvas,
                     const gfx::Rect& plugin_rect,
                     const gfx::Rect& paint_rect) OVERRIDE {}
  virtual void ViewWillInitiatePaint() OVERRIDE {}
  virtual void ViewInitiatedPaint() OVERRIDE {}
  virtual void ViewFlushedPaint() OVERRIDE {}

  virtual bool IsAlwaysOpaque() const OVERRIDE { return true; }
  virtual void SetScale(float scale) OVERRIDE {}
  virtual float GetScale() const OVERRIDE { return scale_; }
  virtual PPB_ImageData_Impl* ImageData() OVERRIDE {
    return image_data_;
  }

 private:
  PPB_ImageData_Impl* image_data_;
  float scale_;
  PluginInstance* bound_instance_;

  DISALLOW_COPY_AND_ASSIGN(MockPlatformGraphics2D);
};

}  // namespace

// This class is forward-declared as a friend so can't be in the anonymous
// namespace.
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


// Test that GetBitmapForOptimizedPluginPaint doesn't return a bitmap rect
// that's bigger than the actual backing store bitmap.
TEST_F(PpapiPluginInstanceTest, GetBitmap2xScale) {
  PP_Size size;
  size.width = 3;
  size.height = 3;

  scoped_refptr<PPB_ImageData_Impl> image_data = new PPB_ImageData_Impl(
      instance()->pp_instance(), PPB_ImageData_Impl::PLATFORM);
  image_data->Init(PPB_ImageData_Impl::GetNativeImageDataFormat(),
                   size.width, size.height, true);
  ASSERT_TRUE(image_data->Map() != NULL);

  float scale = 2.0;
  MockPlatformGraphics2D mock_graphics_2d(image_data.get(), 1.0 / scale);
  instance()->SetBoundGraphics2DForTest(&mock_graphics_2d);

  instance()->set_always_on_top(true);
  SetViewSize(size.width, size.height);

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
