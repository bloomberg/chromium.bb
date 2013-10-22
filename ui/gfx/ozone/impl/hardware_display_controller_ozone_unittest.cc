// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/ozone/impl/drm_skbitmap_ozone.h"
#include "ui/gfx/ozone/impl/drm_wrapper_ozone.h"
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

const uint32_t kDPMSPropertyId = 1;

// The real DrmWrapper makes actual DRM calls which we can't use in unit tests.
class MockDrmWrapperOzone : public gfx::DrmWrapperOzone {
 public:
  MockDrmWrapperOzone(int fd) : DrmWrapperOzone(""),
                                get_crtc_call_count_(0),
                                free_crtc_call_count_(0),
                                restore_crtc_call_count_(0),
                                add_framebuffer_call_count_(0),
                                remove_framebuffer_call_count_(0),
                                set_crtc_expectation_(true),
                                add_framebuffer_expectation_(true),
                                page_flip_expectation_(true) {
    fd_ = fd;
  }

  virtual ~MockDrmWrapperOzone() { fd_ = -1; }

  virtual drmModeCrtc* GetCrtc(uint32_t crtc_id) OVERRIDE {
    get_crtc_call_count_++;
    return new drmModeCrtc;
  }

  virtual void FreeCrtc(drmModeCrtc* crtc) OVERRIDE {
    free_crtc_call_count_++;
    delete crtc;
  }

  virtual bool SetCrtc(uint32_t crtc_id,
                       uint32_t framebuffer,
                       uint32_t* connectors,
                       drmModeModeInfo* mode) OVERRIDE {
    return set_crtc_expectation_;
  }

  virtual bool SetCrtc(drmModeCrtc* crtc, uint32_t* connectors) OVERRIDE {
    restore_crtc_call_count_++;
    return true;
  }

  virtual bool AddFramebuffer(const drmModeModeInfo& mode,
                              uint8_t depth,
                              uint8_t bpp,
                              uint32_t stride,
                              uint32_t handle,
                              uint32_t* framebuffer) OVERRIDE {
    add_framebuffer_call_count_++;
    return add_framebuffer_expectation_;
  }

  virtual bool RemoveFramebuffer(uint32_t framebuffer) OVERRIDE {
    remove_framebuffer_call_count_++;
    return true;
  }

  virtual bool PageFlip(uint32_t crtc_id,
                        uint32_t framebuffer,
                        void* data) OVERRIDE {
    return page_flip_expectation_;
  }

  virtual bool ConnectorSetProperty(uint32_t connector_id,
                                    uint32_t property_id,
                                    uint64_t value) OVERRIDE { return true; }

  int get_get_crtc_call_count() const {
    return get_crtc_call_count_;
  }

  int get_free_crtc_call_count() const {
    return free_crtc_call_count_;
  }

  int get_restore_crtc_call_count() const {
    return restore_crtc_call_count_;
  }

  int get_add_framebuffer_call_count() const {
    return add_framebuffer_call_count_;
  }

  int get_remove_framebuffer_call_count() const {
    return remove_framebuffer_call_count_;
  }

  void set_set_crtc_expectation(bool state) {
    set_crtc_expectation_ = state;
  }

  void set_add_framebuffer_expectation(bool state) {
    add_framebuffer_expectation_ = state;
  }

  void set_page_flip_expectation(bool state) {
    page_flip_expectation_ = state;
  }

 private:
  int get_crtc_call_count_;
  int free_crtc_call_count_;
  int restore_crtc_call_count_;
  int add_framebuffer_call_count_;
  int remove_framebuffer_call_count_;

  bool set_crtc_expectation_;
  bool add_framebuffer_expectation_;
  bool page_flip_expectation_;

  DISALLOW_COPY_AND_ASSIGN(MockDrmWrapperOzone);
};

class MockDrmSkBitmapOzone : public gfx::DrmSkBitmapOzone {
 public:
  MockDrmSkBitmapOzone(int fd) : DrmSkBitmapOzone(fd) {}
  virtual ~MockDrmSkBitmapOzone() {}

  virtual bool Initialize() OVERRIDE {
    return allocPixels();
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(MockDrmSkBitmapOzone);
};

class MockSoftwareSurfaceOzone : public gfx::SoftwareSurfaceOzone {
 public:
  MockSoftwareSurfaceOzone(gfx::HardwareDisplayControllerOzone* controller)
      : SoftwareSurfaceOzone(controller) {}
  virtual ~MockSoftwareSurfaceOzone() {}

 private:
  virtual gfx::DrmSkBitmapOzone* CreateBuffer() OVERRIDE {
    return new MockDrmSkBitmapOzone(kFd);
  }
  DISALLOW_COPY_AND_ASSIGN(MockSoftwareSurfaceOzone);
};

}  // namespace

class HardwareDisplayControllerOzoneTest : public testing::Test {
 public:
  HardwareDisplayControllerOzoneTest() {}
  virtual ~HardwareDisplayControllerOzoneTest() {}

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;
 protected:
  scoped_ptr<gfx::HardwareDisplayControllerOzone> controller_;
  scoped_ptr<MockDrmWrapperOzone> drm_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayControllerOzoneTest);
};

void HardwareDisplayControllerOzoneTest::SetUp() {
  controller_.reset(new gfx::HardwareDisplayControllerOzone());
  drm_.reset(new MockDrmWrapperOzone(kFd));
}

void HardwareDisplayControllerOzoneTest::TearDown() {
  controller_.reset();
  drm_.reset();
}

TEST_F(HardwareDisplayControllerOzoneTest, CheckInitialState) {
  EXPECT_EQ(gfx::HardwareDisplayControllerOzone::UNASSOCIATED,
            controller_->get_state());
}

TEST_F(HardwareDisplayControllerOzoneTest,
       CheckStateAfterControllerIsInitialized) {
  controller_->SetControllerInfo(
      drm_.get(), kConnectorId, kCrtcId, kDPMSPropertyId, kDefaultMode);

  EXPECT_EQ(1, drm_->get_get_crtc_call_count());
  EXPECT_EQ(gfx::HardwareDisplayControllerOzone::UNINITIALIZED,
            controller_->get_state());
}

TEST_F(HardwareDisplayControllerOzoneTest, CheckStateAfterSurfaceIsBound) {
  controller_->SetControllerInfo(
      drm_.get(), kConnectorId, kCrtcId, kDPMSPropertyId, kDefaultMode);
  scoped_ptr<gfx::SoftwareSurfaceOzone> surface(
      new MockSoftwareSurfaceOzone(controller_.get()));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_TRUE(controller_->BindSurfaceToController(surface.Pass()));

  EXPECT_EQ(2, drm_->get_add_framebuffer_call_count());
  EXPECT_EQ(gfx::HardwareDisplayControllerOzone::SURFACE_INITIALIZED,
            controller_->get_state());
}

TEST_F(HardwareDisplayControllerOzoneTest, CheckStateIfBindingFails) {
  drm_->set_add_framebuffer_expectation(false);

  controller_->SetControllerInfo(
      drm_.get(), kConnectorId, kCrtcId, kDPMSPropertyId, kDefaultMode);
  scoped_ptr<gfx::SoftwareSurfaceOzone> surface(
      new MockSoftwareSurfaceOzone(controller_.get()));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_FALSE(controller_->BindSurfaceToController(surface.Pass()));

  EXPECT_EQ(1, drm_->get_add_framebuffer_call_count());
  EXPECT_EQ(gfx::HardwareDisplayControllerOzone::FAILED,
            controller_->get_state());
}

TEST_F(HardwareDisplayControllerOzoneTest, CheckStateAfterPageFlip) {
  controller_->SetControllerInfo(
      drm_.get(), kConnectorId, kCrtcId, kDPMSPropertyId, kDefaultMode);
  scoped_ptr<gfx::SoftwareSurfaceOzone> surface(
      new MockSoftwareSurfaceOzone(controller_.get()));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_TRUE(controller_->BindSurfaceToController(surface.Pass()));

  controller_->SchedulePageFlip();

  EXPECT_EQ(gfx::HardwareDisplayControllerOzone::INITIALIZED,
            controller_->get_state());
}

TEST_F(HardwareDisplayControllerOzoneTest, CheckStateIfModesetFails) {
  drm_->set_set_crtc_expectation(false);

  controller_->SetControllerInfo(
      drm_.get(), kConnectorId, kCrtcId, kDPMSPropertyId, kDefaultMode);
  scoped_ptr<gfx::SoftwareSurfaceOzone> surface(
      new MockSoftwareSurfaceOzone(controller_.get()));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_TRUE(controller_->BindSurfaceToController(surface.Pass()));

  controller_->SchedulePageFlip();

  EXPECT_EQ(gfx::HardwareDisplayControllerOzone::FAILED,
            controller_->get_state());
}

TEST_F(HardwareDisplayControllerOzoneTest, CheckStateIfPageFlipFails) {
  drm_->set_page_flip_expectation(false);

  controller_->SetControllerInfo(
      drm_.get(), kConnectorId, kCrtcId, kDPMSPropertyId, kDefaultMode);
  scoped_ptr<gfx::SoftwareSurfaceOzone> surface(
      new MockSoftwareSurfaceOzone(controller_.get()));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_TRUE(controller_->BindSurfaceToController(surface.Pass()));

  controller_->SchedulePageFlip();

  EXPECT_EQ(gfx::HardwareDisplayControllerOzone::FAILED,
            controller_->get_state());
}

TEST_F(HardwareDisplayControllerOzoneTest, CheckProperDestruction) {
  controller_->SetControllerInfo(
      drm_.get(), kConnectorId, kCrtcId, kDPMSPropertyId, kDefaultMode);
  scoped_ptr<gfx::SoftwareSurfaceOzone> surface(
      new MockSoftwareSurfaceOzone(controller_.get()));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_TRUE(controller_->BindSurfaceToController(surface.Pass()));

  EXPECT_EQ(gfx::HardwareDisplayControllerOzone::SURFACE_INITIALIZED,
            controller_->get_state());

  controller_.reset();

  EXPECT_EQ(2, drm_->get_remove_framebuffer_call_count());
  EXPECT_EQ(1, drm_->get_restore_crtc_call_count());
  EXPECT_EQ(1, drm_->get_free_crtc_call_count());
}
