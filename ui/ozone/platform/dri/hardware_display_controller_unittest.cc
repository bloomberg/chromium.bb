// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/ozone/platform/dri/crtc_state.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"
#include "ui/ozone/platform/dri/test/mock_dri_wrapper.h"
#include "ui/ozone/public/native_pixmap.h"

namespace {

// Create a basic mode for a 6x4 screen.
const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

const uint32_t kPrimaryCrtc = 1;
const uint32_t kPrimaryConnector = 2;
const uint32_t kSecondaryCrtc = 3;
const uint32_t kSecondaryConnector = 4;

const gfx::Size kDefaultModeSize(kDefaultMode.hdisplay, kDefaultMode.vdisplay);
const gfx::SizeF kDefaultModeSizeF(1.0, 1.0);

class MockScanoutBuffer : public ui::ScanoutBuffer {
 public:
  MockScanoutBuffer(const gfx::Size& size) : size_(size) {}

  // ScanoutBuffer:
  virtual uint32_t GetFramebufferId() const OVERRIDE {return 0; }
  virtual uint32_t GetHandle() const OVERRIDE { return 0; }
  virtual gfx::Size GetSize() const OVERRIDE { return size_; }

 private:
  virtual ~MockScanoutBuffer() {}

  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(MockScanoutBuffer);
};

}  // namespace

class HardwareDisplayControllerTest : public testing::Test {
 public:
  HardwareDisplayControllerTest() {}
  virtual ~HardwareDisplayControllerTest() {}

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;
 protected:
  scoped_ptr<ui::HardwareDisplayController> controller_;
  scoped_ptr<ui::MockDriWrapper> drm_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayControllerTest);
};

void HardwareDisplayControllerTest::SetUp() {
  drm_.reset(new ui::MockDriWrapper(3));
  controller_.reset(new ui::HardwareDisplayController(
      drm_.get(),
      scoped_ptr<ui::CrtcState>(
          new ui::CrtcState(drm_.get(), kPrimaryCrtc, kPrimaryConnector))));
}

void HardwareDisplayControllerTest::TearDown() {
  controller_.reset();
  drm_.reset();
}

TEST_F(HardwareDisplayControllerTest, CheckModesettingResult) {
  ui::OverlayPlane plane(scoped_refptr<ui::ScanoutBuffer>(
      new MockScanoutBuffer(kDefaultModeSize)));

  EXPECT_TRUE(controller_->Modeset(plane, kDefaultMode));
  EXPECT_FALSE(plane.buffer->HasOneRef());
}

TEST_F(HardwareDisplayControllerTest, CheckStateAfterPageFlip) {
  ui::OverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
      new MockScanoutBuffer(kDefaultModeSize)));

  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));

  ui::OverlayPlane plane2(scoped_refptr<ui::ScanoutBuffer>(
      new MockScanoutBuffer(kDefaultModeSize)));
  controller_->QueueOverlayPlane(plane2);
  EXPECT_TRUE(controller_->SchedulePageFlip());
  controller_->WaitForPageFlipEvent();
  EXPECT_TRUE(plane1.buffer->HasOneRef());
  EXPECT_FALSE(plane2.buffer->HasOneRef());

  EXPECT_EQ(1, drm_->get_page_flip_call_count());
  EXPECT_EQ(0, drm_->get_overlay_flip_call_count());
}

TEST_F(HardwareDisplayControllerTest, CheckStateIfModesetFails) {
  drm_->set_set_crtc_expectation(false);

  ui::OverlayPlane plane(scoped_refptr<ui::ScanoutBuffer>(new MockScanoutBuffer(
      kDefaultModeSize)));

  EXPECT_FALSE(controller_->Modeset(plane, kDefaultMode));
}

TEST_F(HardwareDisplayControllerTest, CheckStateIfPageFlipFails) {
  drm_->set_page_flip_expectation(false);

  ui::OverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
      new MockScanoutBuffer(kDefaultModeSize)));

  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));

  ui::OverlayPlane plane2(scoped_refptr<ui::ScanoutBuffer>(
      new MockScanoutBuffer(kDefaultModeSize)));
  controller_->QueueOverlayPlane(plane2);
  EXPECT_FALSE(controller_->SchedulePageFlip());
  EXPECT_FALSE(plane1.buffer->HasOneRef());
  EXPECT_FALSE(plane2.buffer->HasOneRef());

  controller_->WaitForPageFlipEvent();
  EXPECT_FALSE(plane1.buffer->HasOneRef());
  EXPECT_TRUE(plane2.buffer->HasOneRef());
}

TEST_F(HardwareDisplayControllerTest, VerifyNoDRMCallsWhenDisabled) {
  ui::OverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
      new MockScanoutBuffer(kDefaultModeSize)));

  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  controller_->Disable();
  ui::OverlayPlane plane2(scoped_refptr<ui::ScanoutBuffer>(
      new MockScanoutBuffer(kDefaultModeSize)));
  controller_->QueueOverlayPlane(plane2);
  EXPECT_TRUE(controller_->SchedulePageFlip());
  controller_->WaitForPageFlipEvent();
  EXPECT_EQ(0, drm_->get_page_flip_call_count());

  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  controller_->QueueOverlayPlane(plane2);
  EXPECT_TRUE(controller_->SchedulePageFlip());
  controller_->WaitForPageFlipEvent();
  EXPECT_EQ(1, drm_->get_page_flip_call_count());
}

TEST_F(HardwareDisplayControllerTest, CheckOverlayPresent) {
  ui::OverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
      new MockScanoutBuffer(kDefaultModeSize)));
  ui::OverlayPlane plane2(
      scoped_refptr<ui::ScanoutBuffer>(new MockScanoutBuffer(kDefaultModeSize)),
      1,
      gfx::OVERLAY_TRANSFORM_NONE,
      gfx::Rect(kDefaultModeSize),
      gfx::RectF(kDefaultModeSizeF));
  plane2.overlay_plane = 1;  // Force association with a plane.

  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));

  controller_->QueueOverlayPlane(plane1);
  controller_->QueueOverlayPlane(plane2);

  EXPECT_TRUE(controller_->SchedulePageFlip());
  controller_->WaitForPageFlipEvent();
  EXPECT_EQ(1, drm_->get_page_flip_call_count());
  EXPECT_EQ(1, drm_->get_overlay_flip_call_count());
}

TEST_F(HardwareDisplayControllerTest, PageflipMirroredControllers) {
  controller_->AddCrtc(
      scoped_ptr<ui::CrtcState>(
          new ui::CrtcState(drm_.get(), kSecondaryCrtc, kSecondaryConnector)));

  ui::OverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
      new MockScanoutBuffer(kDefaultModeSize)));
  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  EXPECT_EQ(2, drm_->get_set_crtc_call_count());

  ui::OverlayPlane plane2(scoped_refptr<ui::ScanoutBuffer>(
      new MockScanoutBuffer(kDefaultModeSize)));
  controller_->QueueOverlayPlane(plane2);
  EXPECT_TRUE(controller_->SchedulePageFlip());
  EXPECT_EQ(2, drm_->get_page_flip_call_count());
}
