// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"
#include "ui/ozone/platform/dri/screen_manager.h"
#include "ui/ozone/platform/dri/test/mock_dri_wrapper.h"

namespace {

// Create a basic mode for a 6x4 screen.
const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

const uint32_t kPrimaryCrtc = 1;
const uint32_t kPrimaryConnector = 2;
const uint32_t kSecondaryCrtc = 3;
const uint32_t kSecondaryConnector = 4;

class MockScreenManager : public ui::ScreenManager {
 public:
  MockScreenManager(ui::DriWrapper* dri,
                    ui::ScanoutBufferGenerator* buffer_generator)
      : ScreenManager(dri, buffer_generator), dri_(dri) {}

  virtual void ForceInitializationOfPrimaryDisplay() OVERRIDE {}

 private:
  ui::DriWrapper* dri_;

  DISALLOW_COPY_AND_ASSIGN(MockScreenManager);
};

}  // namespace

class ScreenManagerTest : public testing::Test {
 public:
  ScreenManagerTest() {}
  virtual ~ScreenManagerTest() {}

  virtual void SetUp() OVERRIDE {
    dri_.reset(new ui::MockDriWrapper(3));
    buffer_generator_.reset(new ui::DriBufferGenerator(dri_.get()));
    screen_manager_.reset(new MockScreenManager(
        dri_.get(), buffer_generator_.get()));
  }
  virtual void TearDown() OVERRIDE {
    screen_manager_.reset();
    dri_.reset();
  }

 protected:
  scoped_ptr<ui::MockDriWrapper> dri_;
  scoped_ptr<ui::DriBufferGenerator> buffer_generator_;
  scoped_ptr<MockScreenManager> screen_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenManagerTest);
};

TEST_F(ScreenManagerTest, CheckWithNoControllers) {
  EXPECT_FALSE(screen_manager_->GetDisplayController(1));
}

TEST_F(ScreenManagerTest, CheckWithValidController) {
  screen_manager_->ConfigureDisplayController(
      kPrimaryCrtc, kPrimaryConnector, gfx::Point(), kDefaultMode);
  base::WeakPtr<ui::HardwareDisplayController> controller =
      screen_manager_->GetDisplayController(1);

  EXPECT_TRUE(controller);
  EXPECT_TRUE(controller->HasCrtc(kPrimaryCrtc));
}

TEST_F(ScreenManagerTest, CheckWithInvalidId) {
  screen_manager_->ConfigureDisplayController(
      kPrimaryCrtc, kPrimaryConnector, gfx::Point(), kDefaultMode);

  EXPECT_TRUE(screen_manager_->GetDisplayController(1));
  EXPECT_FALSE(screen_manager_->GetDisplayController(2));
}

TEST_F(ScreenManagerTest, CheckForSecondValidController) {
  screen_manager_->ConfigureDisplayController(
      kPrimaryCrtc, kPrimaryConnector, gfx::Point(), kDefaultMode);
  screen_manager_->ConfigureDisplayController(
      3, 4, gfx::Point(6, 0), kDefaultMode);

  EXPECT_TRUE(screen_manager_->GetDisplayController(1));
  EXPECT_TRUE(screen_manager_->GetDisplayController(2));
}

TEST_F(ScreenManagerTest, CheckControllerAfterItIsRemoved) {
  screen_manager_->ConfigureDisplayController(
      kPrimaryCrtc, kPrimaryConnector, gfx::Point(), kDefaultMode);
  base::WeakPtr<ui::HardwareDisplayController> controller =
      screen_manager_->GetDisplayController(1);

  EXPECT_TRUE(controller);
  screen_manager_->RemoveDisplayController(1);
  EXPECT_FALSE(controller);
}

TEST_F(ScreenManagerTest, CheckDuplicateConfiguration) {
  screen_manager_->ConfigureDisplayController(
      kPrimaryCrtc, kPrimaryConnector, gfx::Point(), kDefaultMode);
  screen_manager_->ConfigureDisplayController(
      kPrimaryCrtc, kPrimaryConnector, gfx::Point(), kDefaultMode);

  EXPECT_TRUE(screen_manager_->GetDisplayController(1));
  EXPECT_FALSE(screen_manager_->GetDisplayController(2));
}

TEST_F(ScreenManagerTest, CheckChangingMode) {
  screen_manager_->ConfigureDisplayController(
      kPrimaryCrtc, kPrimaryConnector, gfx::Point(), kDefaultMode);
  drmModeModeInfo new_mode = kDefaultMode;
  new_mode.vdisplay = 10;
  screen_manager_->ConfigureDisplayController(
      kPrimaryCrtc, kPrimaryConnector, gfx::Point(), new_mode);

  EXPECT_TRUE(screen_manager_->GetDisplayController(1));
  EXPECT_FALSE(screen_manager_->GetDisplayController(2));
  drmModeModeInfo mode = screen_manager_->GetDisplayController(1)->get_mode();
  EXPECT_EQ(new_mode.vdisplay, mode.vdisplay);
  EXPECT_EQ(new_mode.hdisplay, mode.hdisplay);
}

TEST_F(ScreenManagerTest, CheckForControllersInMirroredMode) {
  screen_manager_->ConfigureDisplayController(
      kPrimaryCrtc, kPrimaryConnector, gfx::Point(), kDefaultMode);
  screen_manager_->ConfigureDisplayController(
      kSecondaryCrtc, kSecondaryConnector, gfx::Point(), kDefaultMode);

  EXPECT_TRUE(screen_manager_->GetDisplayController(1));
  EXPECT_FALSE(screen_manager_->GetDisplayController(2));
}

TEST_F(ScreenManagerTest, CheckMirrorModeTransitions) {
  screen_manager_->ConfigureDisplayController(
      kPrimaryCrtc, kPrimaryConnector, gfx::Point(), kDefaultMode);
  screen_manager_->ConfigureDisplayController(
      kSecondaryCrtc, kSecondaryConnector, gfx::Point(6, 0), kDefaultMode);

  EXPECT_TRUE(screen_manager_->GetDisplayController(1));
  EXPECT_TRUE(screen_manager_->GetDisplayController(2));

  screen_manager_->ConfigureDisplayController(
      kPrimaryCrtc, kPrimaryConnector, gfx::Point(), kDefaultMode);
  screen_manager_->ConfigureDisplayController(
      kSecondaryCrtc, kSecondaryConnector, gfx::Point(), kDefaultMode);
  EXPECT_TRUE(screen_manager_->GetDisplayController(1));
  EXPECT_FALSE(screen_manager_->GetDisplayController(2));

  screen_manager_->ConfigureDisplayController(
      kPrimaryCrtc, kPrimaryConnector, gfx::Point(6, 0), kDefaultMode);
  screen_manager_->ConfigureDisplayController(
      kSecondaryCrtc, kSecondaryConnector, gfx::Point(), kDefaultMode);
  EXPECT_TRUE(screen_manager_->GetDisplayController(1));
  EXPECT_TRUE(screen_manager_->GetDisplayController(3));
}

TEST_F(ScreenManagerTest, MonitorGoneInMirrorMode) {
  screen_manager_->ConfigureDisplayController(
      kPrimaryCrtc, kPrimaryConnector, gfx::Point(), kDefaultMode);
  screen_manager_->ConfigureDisplayController(
      kSecondaryCrtc, kSecondaryConnector, gfx::Point(), kDefaultMode);

  screen_manager_->RemoveDisplayController(kSecondaryCrtc);
  EXPECT_TRUE(screen_manager_->ConfigureDisplayController(
      kPrimaryCrtc, kPrimaryConnector, gfx::Point(), kDefaultMode));
  EXPECT_TRUE(screen_manager_->GetDisplayController(1));
  EXPECT_FALSE(screen_manager_->GetDisplayController(2));
}
