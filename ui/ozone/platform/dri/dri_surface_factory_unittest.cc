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
#include "ui/ozone/platform/dri/hardware_display_controller.h"
#include "ui/ozone/platform/dri/screen_manager.h"
#include "ui/ozone/platform/dri/test/mock_dri_wrapper.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace {

// Mode of size 6x4.
const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

class MockScreenManager : public ui::ScreenManager {
 public:
  MockScreenManager(ui::DriWrapper* dri,
                    ui::ScanoutBufferGenerator* buffer_generator)
      : ScreenManager(dri, buffer_generator),
        dri_(dri) {}
  virtual ~MockScreenManager() {}

  // Normally we'd use DRM to figure out the controller configuration. But we
  // can't use DRM in unit tests, so we just create a fake configuration.
  virtual void ForceInitializationOfPrimaryDisplay() OVERRIDE {
    ConfigureDisplayController(1, 2, kDefaultMode);
  }

 private:
  ui::DriWrapper* dri_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(MockScreenManager);
};

}  // namespace

class DriSurfaceFactoryTest : public testing::Test {
 public:
  DriSurfaceFactoryTest() {}

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;
 protected:
  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<ui::MockDriWrapper> dri_;
  scoped_ptr<ui::DriBufferGenerator> buffer_generator_;
  scoped_ptr<MockScreenManager> screen_manager_;
  scoped_ptr<ui::DriSurfaceFactory> factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriSurfaceFactoryTest);
};

void DriSurfaceFactoryTest::SetUp() {
  message_loop_.reset(new base::MessageLoopForUI);
  dri_.reset(new ui::MockDriWrapper(3));
  buffer_generator_.reset(new ui::DriBufferGenerator(dri_.get()));
  screen_manager_.reset(new MockScreenManager(dri_.get(),
                                              buffer_generator_.get()));
  factory_.reset(new ui::DriSurfaceFactory(dri_.get(), screen_manager_.get()));
}

void DriSurfaceFactoryTest::TearDown() {
  factory_.reset();
  message_loop_.reset();
}

TEST_F(DriSurfaceFactoryTest, FailInitialization) {
  dri_->fail_init();
  EXPECT_EQ(ui::DriSurfaceFactory::FAILED, factory_->InitializeHardware());
}

TEST_F(DriSurfaceFactoryTest, SuccessfulInitialization) {
  EXPECT_EQ(ui::DriSurfaceFactory::INITIALIZED,
            factory_->InitializeHardware());
}

TEST_F(DriSurfaceFactoryTest, SuccessfulWidgetRealization) {
  EXPECT_EQ(ui::DriSurfaceFactory::INITIALIZED,
            factory_->InitializeHardware());

  gfx::AcceleratedWidget w = factory_->GetAcceleratedWidget();
  EXPECT_EQ(ui::DriSurfaceFactory::kDefaultWidgetHandle, w);

  EXPECT_TRUE(factory_->CreateCanvasForWidget(w));
}

TEST_F(DriSurfaceFactoryTest, SetCursorImage) {
  EXPECT_EQ(ui::DriSurfaceFactory::INITIALIZED,
            factory_->InitializeHardware());

  gfx::AcceleratedWidget w = factory_->GetAcceleratedWidget();
  EXPECT_EQ(ui::DriSurfaceFactory::kDefaultWidgetHandle, w);

  scoped_ptr<ui::SurfaceOzoneCanvas> surf = factory_->CreateCanvasForWidget(w);
  EXPECT_TRUE(surf);

  SkBitmap image;
  SkImageInfo info = SkImageInfo::Make(
      6, 4, kN32_SkColorType, kPremul_SkAlphaType);
  image.allocPixels(info);
  image.eraseColor(SK_ColorWHITE);

  factory_->SetHardwareCursor(w, image, gfx::Point(4, 2));

  SkBitmap cursor;
  // Buffers 0 and 1 are the cursor buffers.
  cursor.setInfo(dri_->buffers()[1]->getCanvas()->imageInfo());
  EXPECT_TRUE(dri_->buffers()[1]->getCanvas()->readPixels(&cursor, 0, 0));

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
