// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_surface.h"
#include "ui/ozone/platform/dri/dri_surface_factory.h"
#include "ui/ozone/platform/dri/dri_window_delegate.h"
#include "ui/ozone/platform/dri/dri_window_delegate_manager.h"
#include "ui/ozone/platform/dri/drm_device_manager.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"
#include "ui/ozone/platform/dri/screen_manager.h"
#include "ui/ozone/platform/dri/test/mock_drm_device.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace {

// Mode of size 6x4.
const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

const gfx::AcceleratedWidget kDefaultWidgetHandle = 1;
const uint32_t kDefaultCrtc = 1;
const uint32_t kDefaultConnector = 2;
const int kDefaultCursorSize = 64;

std::vector<skia::RefPtr<SkSurface>> GetCursorBuffers(
    const scoped_refptr<ui::MockDrmDevice> drm) {
  std::vector<skia::RefPtr<SkSurface>> cursor_buffers;
  for (const skia::RefPtr<SkSurface>& cursor_buffer : drm->buffers()) {
    if (cursor_buffer->width() == kDefaultCursorSize &&
        cursor_buffer->height() == kDefaultCursorSize) {
      cursor_buffers.push_back(cursor_buffer);
    }
  }

  return cursor_buffers;
}

}  // namespace

class DriWindowDelegateTest : public testing::Test {
 public:
  DriWindowDelegateTest() {}

  void SetUp() override;
  void TearDown() override;

 protected:
  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_refptr<ui::MockDrmDevice> drm_;
  scoped_ptr<ui::DriBufferGenerator> buffer_generator_;
  scoped_ptr<ui::ScreenManager> screen_manager_;
  scoped_ptr<ui::DrmDeviceManager> drm_device_manager_;
  scoped_ptr<ui::DriWindowDelegateManager> window_delegate_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriWindowDelegateTest);
};

void DriWindowDelegateTest::SetUp() {
  message_loop_.reset(new base::MessageLoopForUI);
  drm_ = new ui::MockDrmDevice();
  buffer_generator_.reset(new ui::DriBufferGenerator());
  screen_manager_.reset(new ui::ScreenManager(buffer_generator_.get()));
  screen_manager_->AddDisplayController(drm_, kDefaultCrtc, kDefaultConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kDefaultCrtc, kDefaultConnector, gfx::Point(), kDefaultMode);

  drm_device_manager_.reset(new ui::DrmDeviceManager(drm_));
  window_delegate_manager_.reset(new ui::DriWindowDelegateManager());

  scoped_ptr<ui::DriWindowDelegate> window_delegate(new ui::DriWindowDelegate(
      kDefaultWidgetHandle, drm_device_manager_.get(), screen_manager_.get()));
  window_delegate->Initialize();
  window_delegate->OnBoundsChanged(
      gfx::Rect(gfx::Size(kDefaultMode.hdisplay, kDefaultMode.vdisplay)));
  window_delegate_manager_->AddWindowDelegate(kDefaultWidgetHandle,
                                              window_delegate.Pass());
}

void DriWindowDelegateTest::TearDown() {
  scoped_ptr<ui::DriWindowDelegate> delegate =
      window_delegate_manager_->RemoveWindowDelegate(kDefaultWidgetHandle);
  delegate->Shutdown();
  message_loop_.reset();
}

TEST_F(DriWindowDelegateTest, SetCursorImage) {
  SkBitmap image;
  SkImageInfo info =
      SkImageInfo::Make(6, 4, kN32_SkColorType, kPremul_SkAlphaType);
  image.allocPixels(info);
  image.eraseColor(SK_ColorWHITE);

  std::vector<SkBitmap> cursor_bitmaps;
  cursor_bitmaps.push_back(image);
  window_delegate_manager_->GetWindowDelegate(kDefaultWidgetHandle)
      ->SetCursor(cursor_bitmaps, gfx::Point(4, 2), 0);

  SkBitmap cursor;
  std::vector<skia::RefPtr<SkSurface>> cursor_buffers = GetCursorBuffers(drm_);
  EXPECT_EQ(2u, cursor_buffers.size());

  // Buffers 1 is the cursor backbuffer we just drew in.
  cursor.setInfo(cursor_buffers[1]->getCanvas()->imageInfo());
  EXPECT_TRUE(cursor_buffers[1]->getCanvas()->readPixels(&cursor, 0, 0));

  // Check that the frontbuffer is displaying the right image as set above.
  for (int i = 0; i < cursor.height(); ++i) {
    for (int j = 0; j < cursor.width(); ++j) {
      if (j < info.width() && i < info.height())
        EXPECT_EQ(SK_ColorWHITE, cursor.getColor(j, i));
      else
        EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT),
                  cursor.getColor(j, i));
    }
  }
}

TEST_F(DriWindowDelegateTest, CheckCursorSurfaceAfterChangingDevice) {
  // Add another device.
  scoped_refptr<ui::MockDrmDevice> drm = new ui::MockDrmDevice();
  screen_manager_->AddDisplayController(drm, kDefaultCrtc, kDefaultConnector);
  screen_manager_->ConfigureDisplayController(
      drm, kDefaultCrtc, kDefaultConnector,
      gfx::Point(0, kDefaultMode.vdisplay), kDefaultMode);

  // Move window to the display on the new device.
  window_delegate_manager_->GetWindowDelegate(kDefaultWidgetHandle)
      ->OnBoundsChanged(gfx::Rect(0, kDefaultMode.vdisplay,
                                  kDefaultMode.hdisplay,
                                  kDefaultMode.vdisplay));

  EXPECT_EQ(2u, GetCursorBuffers(drm).size());
}
