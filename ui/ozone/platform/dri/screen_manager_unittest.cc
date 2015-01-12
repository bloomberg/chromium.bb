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

  void ForceInitializationOfPrimaryDisplay() override {}

 private:
  ui::DriWrapper* dri_;

  DISALLOW_COPY_AND_ASSIGN(MockScreenManager);
};

class TestDisplayChangeObserver : public ui::DisplayChangeObserver {
 public:
  TestDisplayChangeObserver()
      : num_displays_changed_(0), num_displays_removed_(0) {}

  ~TestDisplayChangeObserver() override {}

  int num_displays_changed() const { return num_displays_changed_; }
  int num_displays_removed() const { return num_displays_removed_; }

  // TestDisplayChangeObserver:
  void OnDisplayChanged(ui::HardwareDisplayController* controller) override {
    num_displays_changed_++;
  }

  void OnDisplayRemoved(ui::HardwareDisplayController* controller) override {
    num_displays_removed_++;
  }

 private:
  int num_displays_changed_;
  int num_displays_removed_;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayChangeObserver);
};

}  // namespace

class ScreenManagerTest : public testing::Test {
 public:
  ScreenManagerTest() {}
  ~ScreenManagerTest() override {}

  gfx::Rect GetPrimaryBounds() const {
    return gfx::Rect(0, 0, kDefaultMode.hdisplay, kDefaultMode.vdisplay);
  }

  // Secondary is in extended mode, right-of primary.
  gfx::Rect GetSecondaryBounds() const {
    return gfx::Rect(
        kDefaultMode.hdisplay, 0, kDefaultMode.hdisplay, kDefaultMode.vdisplay);
  }

  void SetUp() override {
    dri_.reset(new ui::MockDriWrapper(3));
    buffer_generator_.reset(new ui::DriBufferGenerator(dri_.get()));
    screen_manager_.reset(new MockScreenManager(
        dri_.get(), buffer_generator_.get()));
    screen_manager_->AddObserver(&observer_);
  }
  void TearDown() override {
    screen_manager_->RemoveObserver(&observer_);
    screen_manager_.reset();
    dri_.reset();
  }

 protected:
  scoped_ptr<ui::MockDriWrapper> dri_;
  scoped_ptr<ui::DriBufferGenerator> buffer_generator_;
  scoped_ptr<MockScreenManager> screen_manager_;

  TestDisplayChangeObserver observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenManagerTest);
};

TEST_F(ScreenManagerTest, CheckWithNoControllers) {
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
}

TEST_F(ScreenManagerTest, CheckWithValidController) {
  screen_manager_->AddDisplayController(dri_.get(), kPrimaryCrtc,
                                        kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(kPrimaryCrtc,
                                              kPrimaryConnector,
                                              GetPrimaryBounds().origin(),
                                              kDefaultMode);
  ui::HardwareDisplayController* controller =
      screen_manager_->GetDisplayController(GetPrimaryBounds());

  EXPECT_TRUE(controller);
  EXPECT_TRUE(controller->HasCrtc(kPrimaryCrtc));
}

TEST_F(ScreenManagerTest, CheckWithInvalidBounds) {
  screen_manager_->AddDisplayController(dri_.get(), kPrimaryCrtc,
                                        kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(kPrimaryCrtc,
                                              kPrimaryConnector,
                                              GetPrimaryBounds().origin(),
                                              kDefaultMode);

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

TEST_F(ScreenManagerTest, CheckForSecondValidController) {
  screen_manager_->AddDisplayController(dri_.get(), kPrimaryCrtc,
                                        kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(kPrimaryCrtc,
                                              kPrimaryConnector,
                                              GetPrimaryBounds().origin(),
                                              kDefaultMode);
  screen_manager_->AddDisplayController(dri_.get(), kSecondaryCrtc,
                                        kSecondaryConnector);
  screen_manager_->ConfigureDisplayController(
      kSecondaryCrtc, kSecondaryConnector, GetSecondaryBounds().origin(),
      kDefaultMode);

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_TRUE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

TEST_F(ScreenManagerTest, CheckControllerAfterItIsRemoved) {
  screen_manager_->AddDisplayController(dri_.get(), kPrimaryCrtc,
                                        kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(kPrimaryCrtc,
                                              kPrimaryConnector,
                                              GetPrimaryBounds().origin(),
                                              kDefaultMode);
  EXPECT_EQ(1, observer_.num_displays_changed());
  EXPECT_EQ(0, observer_.num_displays_removed());
  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));

  screen_manager_->RemoveDisplayController(kPrimaryCrtc);
  EXPECT_EQ(1, observer_.num_displays_changed());
  EXPECT_EQ(1, observer_.num_displays_removed());
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
}

TEST_F(ScreenManagerTest, CheckDuplicateConfiguration) {
  screen_manager_->AddDisplayController(dri_.get(), kPrimaryCrtc,
                                        kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(kPrimaryCrtc,
                                              kPrimaryConnector,
                                              GetPrimaryBounds().origin(),
                                              kDefaultMode);
  uint32_t framebuffer = dri_->current_framebuffer();

  screen_manager_->ConfigureDisplayController(kPrimaryCrtc,
                                              kPrimaryConnector,
                                              GetPrimaryBounds().origin(),
                                              kDefaultMode);

  // Should reuse existing framebuffer.
  EXPECT_EQ(framebuffer, dri_->current_framebuffer());

  EXPECT_EQ(2, observer_.num_displays_changed());
  EXPECT_EQ(0, observer_.num_displays_removed());

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

TEST_F(ScreenManagerTest, CheckChangingMode) {
  screen_manager_->AddDisplayController(dri_.get(), kPrimaryCrtc,
                                        kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(kPrimaryCrtc,
                                              kPrimaryConnector,
                                              GetPrimaryBounds().origin(),
                                              kDefaultMode);
  drmModeModeInfo new_mode = kDefaultMode;
  new_mode.vdisplay = 10;
  screen_manager_->ConfigureDisplayController(
      kPrimaryCrtc, kPrimaryConnector, GetPrimaryBounds().origin(), new_mode);

  EXPECT_EQ(2, observer_.num_displays_changed());
  EXPECT_EQ(0, observer_.num_displays_removed());

  gfx::Rect new_bounds(0, 0, new_mode.hdisplay, new_mode.vdisplay);
  EXPECT_TRUE(screen_manager_->GetDisplayController(new_bounds));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
  drmModeModeInfo mode =
      screen_manager_->GetDisplayController(new_bounds)->get_mode();
  EXPECT_EQ(new_mode.vdisplay, mode.vdisplay);
  EXPECT_EQ(new_mode.hdisplay, mode.hdisplay);
}

TEST_F(ScreenManagerTest, CheckForControllersInMirroredMode) {
  screen_manager_->AddDisplayController(dri_.get(), kPrimaryCrtc,
                                        kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(kPrimaryCrtc,
                                              kPrimaryConnector,
                                              GetPrimaryBounds().origin(),
                                              kDefaultMode);
  screen_manager_->AddDisplayController(dri_.get(), kSecondaryCrtc,
                                        kSecondaryConnector);
  screen_manager_->ConfigureDisplayController(kSecondaryCrtc,
                                              kSecondaryConnector,
                                              GetPrimaryBounds().origin(),
                                              kDefaultMode);

  EXPECT_EQ(2, observer_.num_displays_changed());
  EXPECT_EQ(1, observer_.num_displays_removed());
  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

TEST_F(ScreenManagerTest, CheckMirrorModeTransitions) {
  screen_manager_->AddDisplayController(dri_.get(), kPrimaryCrtc,
                                        kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(kPrimaryCrtc,
                                              kPrimaryConnector,
                                              GetPrimaryBounds().origin(),
                                              kDefaultMode);
  screen_manager_->AddDisplayController(dri_.get(), kSecondaryCrtc,
                                        kSecondaryConnector);
  screen_manager_->ConfigureDisplayController(kSecondaryCrtc,
                                              kSecondaryConnector,
                                              GetSecondaryBounds().origin(),
                                              kDefaultMode);

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_TRUE(screen_manager_->GetDisplayController(GetSecondaryBounds()));

  screen_manager_->ConfigureDisplayController(kPrimaryCrtc,
                                              kPrimaryConnector,
                                              GetPrimaryBounds().origin(),
                                              kDefaultMode);
  screen_manager_->ConfigureDisplayController(kSecondaryCrtc,
                                              kSecondaryConnector,
                                              GetPrimaryBounds().origin(),
                                              kDefaultMode);
  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));

  screen_manager_->ConfigureDisplayController(kPrimaryCrtc,
                                              kPrimaryConnector,
                                              GetSecondaryBounds().origin(),
                                              kDefaultMode);
  screen_manager_->ConfigureDisplayController(kSecondaryCrtc,
                                              kSecondaryConnector,
                                              GetPrimaryBounds().origin(),
                                              kDefaultMode);
  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_TRUE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

TEST_F(ScreenManagerTest, MonitorGoneInMirrorMode) {
  screen_manager_->AddDisplayController(dri_.get(), kPrimaryCrtc,
                                        kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(kPrimaryCrtc,
                                              kPrimaryConnector,
                                              GetPrimaryBounds().origin(),
                                              kDefaultMode);
  screen_manager_->AddDisplayController(dri_.get(), kSecondaryCrtc,
                                        kSecondaryConnector);
  screen_manager_->ConfigureDisplayController(kSecondaryCrtc,
                                              kSecondaryConnector,
                                              GetPrimaryBounds().origin(),
                                              kDefaultMode);

  EXPECT_EQ(2, observer_.num_displays_changed());
  EXPECT_EQ(1, observer_.num_displays_removed());

  screen_manager_->RemoveDisplayController(kSecondaryCrtc);
  EXPECT_TRUE(
      screen_manager_->ConfigureDisplayController(kPrimaryCrtc,
                                                  kPrimaryConnector,
                                                  GetPrimaryBounds().origin(),
                                                  kDefaultMode));
  EXPECT_EQ(3, observer_.num_displays_changed());
  EXPECT_EQ(1, observer_.num_displays_removed());

  EXPECT_TRUE(screen_manager_->GetDisplayController(GetPrimaryBounds()));
  EXPECT_FALSE(screen_manager_->GetDisplayController(GetSecondaryBounds()));
}

TEST_F(ScreenManagerTest, DoNotEnterMirrorModeUnlessSameBounds) {
  screen_manager_->AddDisplayController(dri_.get(), kPrimaryCrtc,
                                        kPrimaryConnector);
  screen_manager_->AddDisplayController(dri_.get(), kSecondaryCrtc,
                                        kSecondaryConnector);

  // Configure displays in extended mode.
  screen_manager_->ConfigureDisplayController(kPrimaryCrtc, kPrimaryConnector,
                                              GetPrimaryBounds().origin(),
                                              kDefaultMode);
  screen_manager_->ConfigureDisplayController(
      kSecondaryCrtc, kSecondaryConnector, GetSecondaryBounds().origin(),
      kDefaultMode);

  drmModeModeInfo new_mode = kDefaultMode;
  new_mode.vdisplay = 10;
  // Shouldn't enter mirror mode unless the display bounds are the same.
  screen_manager_->ConfigureDisplayController(
      kSecondaryCrtc, kSecondaryConnector, GetPrimaryBounds().origin(),
      new_mode);

  EXPECT_FALSE(
      screen_manager_->GetDisplayController(GetPrimaryBounds())->IsMirrored());
}

TEST_F(ScreenManagerTest, ReuseFramebufferIfDisabledThenReEnabled) {
  screen_manager_->AddDisplayController(dri_.get(), kPrimaryCrtc,
                                        kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(kPrimaryCrtc, kPrimaryConnector,
                                              GetPrimaryBounds().origin(),
                                              kDefaultMode);
  uint32_t framebuffer = dri_->current_framebuffer();

  screen_manager_->DisableDisplayController(kPrimaryCrtc);
  EXPECT_EQ(0u, dri_->current_framebuffer());

  screen_manager_->ConfigureDisplayController(kPrimaryCrtc, kPrimaryConnector,
                                              GetPrimaryBounds().origin(),
                                              kDefaultMode);

  // Should reuse existing framebuffer.
  EXPECT_EQ(framebuffer, dri_->current_framebuffer());
}

TEST_F(ScreenManagerTest, CheckMirrorModeAfterBeginReEnabled) {
  screen_manager_->AddDisplayController(dri_.get(), kPrimaryCrtc,
                                        kPrimaryConnector);
  screen_manager_->ConfigureDisplayController(kPrimaryCrtc, kPrimaryConnector,
                                              GetPrimaryBounds().origin(),
                                              kDefaultMode);
  screen_manager_->DisableDisplayController(kPrimaryCrtc);

  screen_manager_->AddDisplayController(dri_.get(), kSecondaryCrtc,
                                        kSecondaryConnector);
  screen_manager_->ConfigureDisplayController(
      kSecondaryCrtc, kSecondaryConnector, GetPrimaryBounds().origin(),
      kDefaultMode);

  ui::HardwareDisplayController* controller =
      screen_manager_->GetDisplayController(GetPrimaryBounds());
  EXPECT_TRUE(controller);
  EXPECT_FALSE(controller->IsMirrored());

  screen_manager_->ConfigureDisplayController(kPrimaryCrtc, kPrimaryConnector,
                                              GetPrimaryBounds().origin(),
                                              kDefaultMode);
  EXPECT_TRUE(controller);
  EXPECT_TRUE(controller->IsMirrored());
}
