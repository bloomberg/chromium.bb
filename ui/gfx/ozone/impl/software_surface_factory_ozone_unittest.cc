// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/ozone/impl/drm_skbitmap_ozone.h"
#include "ui/gfx/ozone/impl/drm_wrapper_ozone.h"
#include "ui/gfx/ozone/impl/hardware_display_controller_ozone.h"
#include "ui/gfx/ozone/impl/software_surface_factory_ozone.h"
#include "ui/gfx/ozone/impl/software_surface_ozone.h"
#include "ui/gfx/ozone/surface_factory_ozone.h"

namespace {

const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

// Mock file descriptor ID.
const int kFd = 3;

// Mock connector ID.
const uint32_t kConnectorId = 1;

// Mock CRTC ID.
const uint32_t kCrtcId = 1;

const uint32_t kDPMSPropertyId = 1;

const gfx::AcceleratedWidget kDefaultWidgetHandle = 1;

// The real DrmWrapper makes actual DRM calls which we can't use in unit tests.
class MockDrmWrapperOzone : public gfx::DrmWrapperOzone {
 public:
  MockDrmWrapperOzone(int fd) : DrmWrapperOzone(""),
                                add_framebuffer_expectation_(true),
                                page_flip_expectation_(true) {
    fd_ = fd;
  }

  virtual ~MockDrmWrapperOzone() { fd_ = -1; }

  virtual drmModeCrtc* GetCrtc(uint32_t crtc_id) OVERRIDE {
    return new drmModeCrtc;
  }

  virtual void FreeCrtc(drmModeCrtc* crtc) OVERRIDE {
    delete crtc;
  }

  virtual bool SetCrtc(uint32_t crtc_id,
                       uint32_t framebuffer,
                       uint32_t* connectors,
                       drmModeModeInfo* mode) OVERRIDE {
    return true;
  }

  virtual bool SetCrtc(drmModeCrtc* crtc, uint32_t* connectors) OVERRIDE {
    return true;
  }

  virtual bool AddFramebuffer(const drmModeModeInfo& mode,
                              uint8_t depth,
                              uint8_t bpp,
                              uint32_t stride,
                              uint32_t handle,
                              uint32_t* framebuffer) OVERRIDE {
    return add_framebuffer_expectation_;
  }

  virtual bool RemoveFramebuffer(uint32_t framebuffer) OVERRIDE { return true; }

  virtual bool PageFlip(uint32_t crtc_id,
                        uint32_t framebuffer,
                        void* data) OVERRIDE {
    static_cast<gfx::HardwareDisplayControllerOzone*>(data)->get_surface()
        ->SwapBuffers();
    return page_flip_expectation_;
  }

  virtual bool ConnectorSetProperty(uint32_t connector_id,
                                    uint32_t property_id,
                                    uint64_t value) OVERRIDE { return true; }

  void set_add_framebuffer_expectation(bool state) {
    add_framebuffer_expectation_ = state;
  }

  void set_page_flip_expectation(bool state) {
    page_flip_expectation_ = state;
  }

 private:
  bool add_framebuffer_expectation_;
  bool page_flip_expectation_;

  DISALLOW_COPY_AND_ASSIGN(MockDrmWrapperOzone);
};

class MockDrmSkBitmapOzone : public gfx::DrmSkBitmapOzone {
 public:
  MockDrmSkBitmapOzone() : DrmSkBitmapOzone(kFd) {}
  virtual ~MockDrmSkBitmapOzone() {}

  virtual bool Initialize() OVERRIDE {
    allocPixels();
    eraseColor(SK_ColorBLACK);
    return true;
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
    return new MockDrmSkBitmapOzone();
  }

  DISALLOW_COPY_AND_ASSIGN(MockSoftwareSurfaceOzone);
};

// SSFO would normally allocate DRM resources. We can't rely on having a DRM
// backend to allocate and display our buffers. Thus, we replace these
// resources with stubs. For DRM calls, we simply use stubs that do nothing and
// for buffers we use the default SkBitmap allocator.
class MockSoftwareSurfaceFactoryOzone
    : public gfx::SoftwareSurfaceFactoryOzone {
 public:
  MockSoftwareSurfaceFactoryOzone()
      : SoftwareSurfaceFactoryOzone(),
        mock_drm_(NULL),
        drm_wrapper_expectation_(true),
        initialize_controller_expectation_(true) {}
  virtual ~MockSoftwareSurfaceFactoryOzone() {};

  void set_drm_wrapper_expectation(bool state) {
    drm_wrapper_expectation_ = state;
  }

  void set_initialize_controller_expectation(bool state) {
    initialize_controller_expectation_ = state;
  }

  MockDrmWrapperOzone* get_drm() const {
    return mock_drm_;
  }

 private:
  virtual gfx::SoftwareSurfaceOzone* CreateSurface(
      gfx::HardwareDisplayControllerOzone* controller) OVERRIDE {
    return new MockSoftwareSurfaceOzone(controller);
  }

  virtual gfx::DrmWrapperOzone* CreateWrapper() OVERRIDE {
    if (drm_wrapper_expectation_)
      mock_drm_ = new MockDrmWrapperOzone(kFd);
    else
      mock_drm_ = new MockDrmWrapperOzone(-1);

    return mock_drm_;
  }

  // Normally we'd use DRM to figure out the controller configuration. But we
  // can't use DRM in unit tests, so we just create a fake configuration.
  virtual bool InitializeControllerForPrimaryDisplay(
      gfx::DrmWrapperOzone* drm,
      gfx::HardwareDisplayControllerOzone* controller) OVERRIDE {
    if (initialize_controller_expectation_) {
      controller->SetControllerInfo(drm,
                                    kConnectorId,
                                    kCrtcId,
                                    kDPMSPropertyId,
                                    kDefaultMode);
      return true;
    } else {
      return false;
    }
  }

  virtual void WaitForPageFlipEvent(int fd) OVERRIDE {}

  MockDrmWrapperOzone* mock_drm_;
  bool drm_wrapper_expectation_;
  bool initialize_controller_expectation_;

  DISALLOW_COPY_AND_ASSIGN(MockSoftwareSurfaceFactoryOzone);
};

}  // namespace

class SoftwareSurfaceFactoryOzoneTest : public testing::Test {
 public:
  SoftwareSurfaceFactoryOzoneTest() {}

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;
 protected:
  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<MockSoftwareSurfaceFactoryOzone> factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SoftwareSurfaceFactoryOzoneTest);
};

void SoftwareSurfaceFactoryOzoneTest::SetUp() {
  message_loop_.reset(new base::MessageLoop(base::MessageLoop::TYPE_UI));
  factory_.reset(new MockSoftwareSurfaceFactoryOzone());
}

void SoftwareSurfaceFactoryOzoneTest::TearDown() {
  factory_.reset();
  message_loop_.reset();
}

TEST_F(SoftwareSurfaceFactoryOzoneTest, FailInitialization) {
  factory_->set_drm_wrapper_expectation(false);

  EXPECT_EQ(gfx::SurfaceFactoryOzone::FAILED, factory_->InitializeHardware());
}

TEST_F(SoftwareSurfaceFactoryOzoneTest, SuccessfulInitialization) {
  EXPECT_EQ(gfx::SurfaceFactoryOzone::INITIALIZED,
            factory_->InitializeHardware());
}

TEST_F(SoftwareSurfaceFactoryOzoneTest, FailSurfaceInitialization) {
  factory_->set_initialize_controller_expectation(false);

  EXPECT_EQ(gfx::SurfaceFactoryOzone::INITIALIZED,
            factory_->InitializeHardware());

  gfx::AcceleratedWidget w = factory_->GetAcceleratedWidget();
  EXPECT_EQ(kDefaultWidgetHandle, w);

  EXPECT_EQ(gfx::kNullAcceleratedWidget, factory_->RealizeAcceleratedWidget(w));
}

TEST_F(SoftwareSurfaceFactoryOzoneTest, FailBindingSurfaceToController) {
  EXPECT_EQ(gfx::SurfaceFactoryOzone::INITIALIZED,
            factory_->InitializeHardware());

  factory_->get_drm()->set_add_framebuffer_expectation(false);

  gfx::AcceleratedWidget w = factory_->GetAcceleratedWidget();
  EXPECT_EQ(kDefaultWidgetHandle, w);

  EXPECT_EQ(gfx::kNullAcceleratedWidget, factory_->RealizeAcceleratedWidget(w));
}

TEST_F(SoftwareSurfaceFactoryOzoneTest, SuccessfulWidgetRealization) {
  EXPECT_EQ(gfx::SurfaceFactoryOzone::INITIALIZED,
            factory_->InitializeHardware());

  gfx::AcceleratedWidget w = factory_->GetAcceleratedWidget();
  EXPECT_EQ(kDefaultWidgetHandle, w);

  EXPECT_NE(gfx::kNullAcceleratedWidget, factory_->RealizeAcceleratedWidget(w));
}

TEST_F(SoftwareSurfaceFactoryOzoneTest, FailSchedulePageFlip) {
  EXPECT_EQ(gfx::SurfaceFactoryOzone::INITIALIZED,
            factory_->InitializeHardware());

  factory_->get_drm()->set_page_flip_expectation(false);

  gfx::AcceleratedWidget w = factory_->GetAcceleratedWidget();
  EXPECT_EQ(kDefaultWidgetHandle, w);

  EXPECT_NE(gfx::kNullAcceleratedWidget, factory_->RealizeAcceleratedWidget(w));

  EXPECT_FALSE(factory_->SchedulePageFlip(w));
}

TEST_F(SoftwareSurfaceFactoryOzoneTest, SuccessfulSchedulePageFlip) {
  EXPECT_EQ(gfx::SurfaceFactoryOzone::INITIALIZED,
            factory_->InitializeHardware());

  gfx::AcceleratedWidget w = factory_->GetAcceleratedWidget();
  EXPECT_EQ(kDefaultWidgetHandle, w);

  EXPECT_NE(gfx::kNullAcceleratedWidget, factory_->RealizeAcceleratedWidget(w));

  EXPECT_TRUE(factory_->SchedulePageFlip(w));
}
