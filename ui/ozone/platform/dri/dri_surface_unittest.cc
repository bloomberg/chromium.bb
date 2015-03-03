// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/ozone/platform/dri/crtc_controller.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_surface.h"
#include "ui/ozone/platform/dri/dri_window_delegate.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"
#include "ui/ozone/platform/dri/test/mock_drm_device.h"

namespace {

// Create a basic mode for a 6x4 screen.
const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

const uint32_t kDefaultCrtc = 1;
const uint32_t kDefaultConnector = 2;
const size_t kPlanesPerCrtc = 1;

class MockDriWindowDelegate : public ui::DriWindowDelegate {
 public:
  MockDriWindowDelegate(ui::DrmDevice* drm) {
    controller_.reset(new ui::HardwareDisplayController(make_scoped_ptr(
        new ui::CrtcController(drm, kDefaultCrtc, kDefaultConnector))));
    scoped_refptr<ui::DriBuffer> buffer(new ui::DriBuffer(drm));
    SkImageInfo info = SkImageInfo::MakeN32Premul(kDefaultMode.hdisplay,
                                                  kDefaultMode.vdisplay);
    EXPECT_TRUE(buffer->Initialize(info, true));
    EXPECT_TRUE(controller_->Modeset(ui::OverlayPlane(buffer), kDefaultMode));
  }
  ~MockDriWindowDelegate() override {}

  // DriWindowDelegate:
  void Initialize() override {}
  void Shutdown() override {}
  gfx::AcceleratedWidget GetAcceleratedWidget() override { return 1; }
  ui::HardwareDisplayController* GetController() override {
    return controller_.get();
  }
  void OnBoundsChanged(const gfx::Rect& bounds) override {}
  void SetCursor(const std::vector<SkBitmap>& bitmaps,
                 const gfx::Point& location,
                 int frame_delay_ms) override {}
  void SetCursorWithoutAnimations(const std::vector<SkBitmap>& bitmaps,
                                  const gfx::Point& location) override {}
  void MoveCursor(const gfx::Point& location) override {}

 private:
  scoped_ptr<ui::HardwareDisplayController> controller_;

  DISALLOW_COPY_AND_ASSIGN(MockDriWindowDelegate);
};

}  // namespace

class DriSurfaceTest : public testing::Test {
 public:
  DriSurfaceTest() {}

  void SetUp() override;
  void TearDown() override;

 protected:
  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_refptr<ui::MockDrmDevice> drm_;
  scoped_ptr<MockDriWindowDelegate> window_delegate_;
  scoped_ptr<ui::DriSurface> surface_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriSurfaceTest);
};

void DriSurfaceTest::SetUp() {
  message_loop_.reset(new base::MessageLoopForUI);
  std::vector<uint32_t> crtcs;
  crtcs.push_back(kDefaultCrtc);
  drm_ = new ui::MockDrmDevice(true, crtcs, kPlanesPerCrtc);
  window_delegate_.reset(new MockDriWindowDelegate(drm_.get()));
  surface_.reset(new ui::DriSurface(window_delegate_.get()));
  surface_->ResizeCanvas(gfx::Size(kDefaultMode.hdisplay,
                                   kDefaultMode.vdisplay));
}

void DriSurfaceTest::TearDown() {
  surface_.reset();
  window_delegate_.reset();
  drm_ = nullptr;
  message_loop_.reset();
}

TEST_F(DriSurfaceTest, CheckFBIDOnSwap) {
  surface_->PresentCanvas(gfx::Rect());
  // Framebuffer ID 1 is allocated in SetUp for the buffer used to modeset.
  EXPECT_EQ(3u, drm_->current_framebuffer());
  surface_->PresentCanvas(gfx::Rect());
  EXPECT_EQ(2u, drm_->current_framebuffer());
}

TEST_F(DriSurfaceTest, CheckSurfaceContents) {
  SkPaint paint;
  paint.setColor(SK_ColorWHITE);
  SkRect rect = SkRect::MakeWH(kDefaultMode.hdisplay / 2,
                               kDefaultMode.vdisplay / 2);
  surface_->GetSurface()->getCanvas()->drawRect(rect, paint);
  surface_->PresentCanvas(
      gfx::Rect(0, 0, kDefaultMode.hdisplay / 2, kDefaultMode.vdisplay / 2));

  SkBitmap image;
  // Buffer 0 is the buffer used in SetUp for modesetting and buffer 1 is the
  // frontbuffer.
  // Buffer 2 is the backbuffer we just painted in, so we want to make sure its
  // contents are correct.
  image.setInfo(drm_->buffers()[2]->getCanvas()->imageInfo());
  EXPECT_TRUE(drm_->buffers()[2]->getCanvas()->readPixels(&image, 0, 0));

  EXPECT_EQ(kDefaultMode.hdisplay, image.width());
  EXPECT_EQ(kDefaultMode.vdisplay, image.height());

  // Make sure the updates are correctly propagated to the native surface.
  for (int i = 0; i < image.height(); ++i) {
    for (int j = 0; j < image.width(); ++j) {
      if (j < kDefaultMode.hdisplay / 2 && i < kDefaultMode.vdisplay / 2)
        EXPECT_EQ(SK_ColorWHITE, image.getColor(j, i));
      else
        EXPECT_EQ(SK_ColorBLACK, image.getColor(j, i));
    }
  }
}
