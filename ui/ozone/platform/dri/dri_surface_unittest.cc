// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_surface.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"
#include "ui/ozone/platform/dri/test/mock_dri_wrapper.h"

namespace {

// Create a basic mode for a 6x4 screen.
const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

}  // namespace

class DriSurfaceTest : public testing::Test {
 public:
  DriSurfaceTest() {}

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  scoped_ptr<ui::MockDriWrapper> drm_;
  scoped_ptr<ui::HardwareDisplayController> controller_;
  scoped_ptr<ui::DriSurface> surface_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriSurfaceTest);
};

void DriSurfaceTest::SetUp() {
  drm_.reset(new ui::MockDriWrapper(3));
  controller_.reset(new ui::HardwareDisplayController(drm_.get(), 1, 1));

  surface_.reset(new ui::DriSurface(
      drm_.get(), gfx::Size(kDefaultMode.hdisplay, kDefaultMode.vdisplay)));
}

void DriSurfaceTest::TearDown() {
  surface_.reset();
  controller_.reset();
  drm_.reset();
}

TEST_F(DriSurfaceTest, FailInitialization) {
  drm_->set_create_dumb_buffer_expectation(false);
  EXPECT_FALSE(surface_->Initialize());
}

TEST_F(DriSurfaceTest, SuccessfulInitialization) {
  EXPECT_TRUE(surface_->Initialize());
}

TEST_F(DriSurfaceTest, CheckFBIDOnSwap) {
  EXPECT_TRUE(surface_->Initialize());

  // Check that the framebuffer ID is correct.
  EXPECT_EQ(2u, surface_->GetFramebufferId());

  surface_->SwapBuffers();

  EXPECT_EQ(1u, surface_->GetFramebufferId());
}

TEST_F(DriSurfaceTest, CheckPixelPointerOnSwap) {
  EXPECT_TRUE(surface_->Initialize());

  void* bitmap_pixels1 = surface_->GetDrawableForWidget()->getDevice()
      ->accessBitmap(false).getPixels();

  surface_->SwapBuffers();

  void* bitmap_pixels2 = surface_->GetDrawableForWidget()->getDevice()
      ->accessBitmap(false).getPixels();

  // Check that once the buffers have been swapped the drawable's underlying
  // pixels have been changed.
  EXPECT_NE(bitmap_pixels1, bitmap_pixels2);
}
