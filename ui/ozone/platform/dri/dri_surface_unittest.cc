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
#include "ui/ozone/platform/dri/drm_device_manager.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"
#include "ui/ozone/platform/dri/screen_manager.h"
#include "ui/ozone/platform/dri/test/mock_drm_device.h"

namespace {

// Create a basic mode for a 6x4 screen.
const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

const gfx::AcceleratedWidget kDefaultWidgetHandle = 1;
const uint32_t kDefaultCrtc = 1;
const uint32_t kDefaultConnector = 2;
const size_t kPlanesPerCrtc = 1;
const uint32_t kDefaultCursorSize = 64;

}  // namespace

class DriSurfaceTest : public testing::Test {
 public:
  DriSurfaceTest() {}

  void SetUp() override;
  void TearDown() override;

 protected:
  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_refptr<ui::MockDrmDevice> drm_;
  scoped_ptr<ui::DriBufferGenerator> buffer_generator_;
  scoped_ptr<ui::ScreenManager> screen_manager_;
  scoped_ptr<ui::DrmDeviceManager> drm_device_manager_;
  scoped_ptr<ui::DriWindowDelegate> window_delegate_;
  scoped_ptr<ui::DriSurface> surface_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriSurfaceTest);
};

void DriSurfaceTest::SetUp() {
  message_loop_.reset(new base::MessageLoopForUI);
  std::vector<uint32_t> crtcs;
  crtcs.push_back(kDefaultCrtc);
  drm_ = new ui::MockDrmDevice(true, crtcs, kPlanesPerCrtc);
  buffer_generator_.reset(new ui::DriBufferGenerator());
  screen_manager_.reset(new ui::ScreenManager(buffer_generator_.get()));
  screen_manager_->AddDisplayController(drm_, kDefaultCrtc, kDefaultConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kDefaultCrtc, kDefaultConnector, gfx::Point(), kDefaultMode);

  drm_device_manager_.reset(new ui::DrmDeviceManager(drm_));
  window_delegate_.reset(new ui::DriWindowDelegate(
      kDefaultWidgetHandle, drm_device_manager_.get(), screen_manager_.get()));
  window_delegate_->Initialize();
  window_delegate_->OnBoundsChanged(
      gfx::Rect(gfx::Size(kDefaultMode.hdisplay, kDefaultMode.vdisplay)));

  surface_.reset(new ui::DriSurface(window_delegate_.get()));
  surface_->ResizeCanvas(gfx::Size(kDefaultMode.hdisplay,
                                   kDefaultMode.vdisplay));
}

void DriSurfaceTest::TearDown() {
  surface_.reset();
  window_delegate_->Shutdown();
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
  std::vector<skia::RefPtr<SkSurface>> framebuffers;
  for (const auto& buffer : drm_->buffers()) {
    // Skip cursor buffers.
    if (buffer->width() == kDefaultCursorSize &&
        buffer->height() == kDefaultCursorSize)
      continue;

    framebuffers.push_back(buffer);
  }

  // Buffer 0 is the modesetting buffer, buffer 1 is the frontbuffer and buffer
  // 2 is the backbuffer.
  EXPECT_EQ(3u, framebuffers.size());

  image.setInfo(framebuffers[2]->getCanvas()->imageInfo());
  EXPECT_TRUE(framebuffers[2]->getCanvas()->readPixels(&image, 0, 0));

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
