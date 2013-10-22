// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/gfx/ozone/impl/drm_skbitmap_ozone.h"
#include "ui/gfx/ozone/impl/hardware_display_controller_ozone.h"
#include "ui/gfx/ozone/impl/software_surface_ozone.h"

namespace {

// Create a basic mode for a 6x4 screen.
const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

// Mock file descriptor ID.
const int kFd = 3;

// Mock connector ID.
const uint32_t kConnectorId = 1;

// Mock CRTC ID.
const uint32_t kCrtcId = 1;

// Mock DPMS property ID.
const uint32_t kDPMSPropertyId = 1;

class MockDrmWrapperOzone : public gfx::DrmWrapperOzone {
 public:
  MockDrmWrapperOzone() : DrmWrapperOzone(""), id_(1) { fd_ = kFd; }
  virtual ~MockDrmWrapperOzone() { fd_ = -1; }

  virtual drmModeCrtc* GetCrtc(uint32_t crtc_id) OVERRIDE { return NULL; }
  virtual void FreeCrtc(drmModeCrtc* crtc) OVERRIDE {}
  virtual bool SetCrtc(uint32_t crtc_id,
                       uint32_t framebuffer,
                       uint32_t* connectors,
                       drmModeModeInfo* mode) OVERRIDE { return true; }
  virtual bool SetCrtc(drmModeCrtc* crtc, uint32_t* connectors) OVERRIDE {
    return true;
  }
  virtual bool AddFramebuffer(const drmModeModeInfo& mode,
                              uint8_t depth,
                              uint8_t bpp,
                              uint32_t stride,
                              uint32_t handle,
                              uint32_t* framebuffer) OVERRIDE {
    *framebuffer = id_++;
    return true;
  }
  virtual bool RemoveFramebuffer(uint32_t framebuffer) OVERRIDE { return true; }
  virtual bool PageFlip(uint32_t crtc_id,
                        uint32_t framebuffer,
                        void* data) OVERRIDE {
    return true;
  }
  virtual bool ConnectorSetProperty(uint32_t connector_id,
                                    uint32_t property_id,
                                    uint64_t value) OVERRIDE { return true; }

 private:
  int id_;
  DISALLOW_COPY_AND_ASSIGN(MockDrmWrapperOzone);
};

class MockDrmSkBitmapOzone : public gfx::DrmSkBitmapOzone {
 public:
  MockDrmSkBitmapOzone(int fd,
                       bool initialize_expectation)
      : DrmSkBitmapOzone(fd),
        initialize_expectation_(initialize_expectation) {}
  virtual ~MockDrmSkBitmapOzone() {}

  virtual bool Initialize() OVERRIDE {
    if (!initialize_expectation_)
      return false;

    allocPixels();
    // Clear the bitmap to black.
    eraseColor(SK_ColorBLACK);

    return true;
  }
 private:
  bool initialize_expectation_;

  DISALLOW_COPY_AND_ASSIGN(MockDrmSkBitmapOzone);
};

class MockSoftwareSurfaceOzone : public gfx::SoftwareSurfaceOzone {
 public:
  MockSoftwareSurfaceOzone(gfx::HardwareDisplayControllerOzone* controller)
      : SoftwareSurfaceOzone(controller),
        initialize_expectation_(true) {}
  virtual ~MockSoftwareSurfaceOzone() {}

  void set_initialize_expectation(bool state) {
    initialize_expectation_ = state;
  }

 private:
  virtual gfx::DrmSkBitmapOzone* CreateBuffer() OVERRIDE {
    return new MockDrmSkBitmapOzone(kFd, initialize_expectation_);
  }

  bool initialize_expectation_;

  DISALLOW_COPY_AND_ASSIGN(MockSoftwareSurfaceOzone);
};

}  // namespace

class SoftwareSurfaceOzoneTest : public testing::Test {
 public:
  SoftwareSurfaceOzoneTest() {}

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  scoped_ptr<MockDrmWrapperOzone> drm_;
  scoped_ptr<gfx::HardwareDisplayControllerOzone> controller_;
  scoped_ptr<MockSoftwareSurfaceOzone> surface_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SoftwareSurfaceOzoneTest);
};

void SoftwareSurfaceOzoneTest::SetUp() {
  drm_.reset(new MockDrmWrapperOzone());
  controller_.reset(new gfx::HardwareDisplayControllerOzone());
  controller_->SetControllerInfo(
      drm_.get(), kConnectorId, kCrtcId, kDPMSPropertyId, kDefaultMode);

  surface_.reset(new MockSoftwareSurfaceOzone(controller_.get()));
}

void SoftwareSurfaceOzoneTest::TearDown() {
  surface_.reset();
  controller_.reset();
  drm_.reset();
}

TEST_F(SoftwareSurfaceOzoneTest, FailInitialization) {
  surface_->set_initialize_expectation(false);
  EXPECT_FALSE(surface_->Initialize());
}

TEST_F(SoftwareSurfaceOzoneTest, SuccessfulInitialization) {
  EXPECT_TRUE(surface_->Initialize());
}

TEST_F(SoftwareSurfaceOzoneTest, CheckFBIDOnSwap) {
  EXPECT_TRUE(surface_->Initialize());
  controller_->BindSurfaceToController(
      surface_.PassAs<gfx::SoftwareSurfaceOzone>());

  // Check that the framebuffer ID is correct.
  EXPECT_EQ(2u, controller_->get_surface()->GetFramebufferId());

  controller_->get_surface()->SwapBuffers();

  EXPECT_EQ(1u, controller_->get_surface()->GetFramebufferId());
}

TEST_F(SoftwareSurfaceOzoneTest, CheckPixelPointerOnSwap) {
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

TEST_F(SoftwareSurfaceOzoneTest, CheckCorrectBufferSync) {
  EXPECT_TRUE(surface_->Initialize());

  SkCanvas* canvas = surface_->GetDrawableForWidget();
  SkRect clip;
  // Modify part of the canvas.
  clip.set(0, 0,
           canvas->getDeviceSize().width() / 2,
           canvas->getDeviceSize().height() / 2);
  canvas->clipRect(clip, SkRegion::kReplace_Op);

  canvas->drawColor(SK_ColorWHITE);

  surface_->SwapBuffers();

  // Verify that the modified contents have been copied over on swap (make sure
  // the 2 buffers have the same content).
  for (int i = 0; i < canvas->getDeviceSize().height(); ++i) {
    for (int j = 0; j < canvas->getDeviceSize().width(); ++j) {
      if (i < clip.height() && j < clip.width())
        EXPECT_EQ(SK_ColorWHITE,
                  canvas->getDevice()->accessBitmap(false).getColor(j, i));
      else
        EXPECT_EQ(SK_ColorBLACK,
                  canvas->getDevice()->accessBitmap(false).getColor(j, i));
    }
  }
}
