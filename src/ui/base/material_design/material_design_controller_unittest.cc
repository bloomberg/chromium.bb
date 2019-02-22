// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/material_design/material_design_controller_observer.h"
#include "ui/base/test/material_design_controller_test_api.h"
#include "ui/base/ui_base_switches.h"

namespace ui {

TEST(MaterialDesignControllerDeathTest, CrashesWithoutInitialization) {
  ASSERT_FALSE(MaterialDesignController::is_mode_initialized());
  EXPECT_DEATH_IF_SUPPORTED(
      MaterialDesignController::IsTouchOptimizedUiEnabled(), "");
}

namespace {

// Test fixture for the MaterialDesignController class.
class MaterialDesignControllerTest : public testing::Test {
 public:
  MaterialDesignControllerTest() = default;
  ~MaterialDesignControllerTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    MaterialDesignController::Initialize();
  }

  void TearDown() override {
    test::MaterialDesignControllerTestAPI::Uninitialize();
    testing::Test::TearDown();
  }

  void SetCommandLineSwitch(const std::string& value_string) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kTopChromeMD, value_string);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MaterialDesignControllerTest);
};

}  // namespace

#if !defined(OS_CHROMEOS)
// Verify that non-touch is the default.
TEST_F(MaterialDesignControllerTest, NoCommandLineFlagIsRefresh) {
  ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kTopChromeMD));
  EXPECT_FALSE(MaterialDesignController::IsTouchOptimizedUiEnabled());
}
#endif

namespace {

class MaterialDesignControllerTestCommandLineRefresh
    : public MaterialDesignControllerTest {
 public:
  MaterialDesignControllerTestCommandLineRefresh() {
    SetCommandLineSwitch(switches::kTopChromeMDMaterialRefresh);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MaterialDesignControllerTestCommandLineRefresh);
};

}  // namespace

// Verify switches::kTopChromeMDMaterialRefresh maps to non-touch (the default).
TEST_F(MaterialDesignControllerTestCommandLineRefresh, CheckApiReturns) {
  EXPECT_FALSE(MaterialDesignController::IsTouchOptimizedUiEnabled());
}

namespace {

class MaterialDesignControllerTestCommandLineForceTouchRefresh
    : public MaterialDesignControllerTest {
 public:
  MaterialDesignControllerTestCommandLineForceTouchRefresh() {
    SetCommandLineSwitch(switches::kTopChromeMDMaterialRefreshTouchOptimized);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      MaterialDesignControllerTestCommandLineForceTouchRefresh);
};

}  // namespace

// Verify switches::kTopChromeMDMaterialRefreshTouchOptimized maps to touch.
TEST_F(MaterialDesignControllerTestCommandLineForceTouchRefresh,
       CheckApiReturns) {
  EXPECT_TRUE(MaterialDesignController::IsTouchOptimizedUiEnabled());
}

namespace {

class TestObserver : public ui::MaterialDesignControllerObserver {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  bool on_md_mode_changed_called() { return on_md_mode_changed_called_; }

 private:
  // ui::MaterialDesignControllerObserver:
  void OnMdModeChanged() override { on_md_mode_changed_called_ = true; }

  bool on_md_mode_changed_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

TEST(MaterialDesignControllerObserver, InitializationOnMdModeChanged) {
  // Verifies that the MaterialDesignControllerObserver gets called back when
  // the mode changes at startup.
  TestObserver observer;
  ASSERT_FALSE(observer.on_md_mode_changed_called());
  MaterialDesignController::GetInstance()->AddObserver(&observer);

  // Trigger a mode change by setting it for the first time.
  MaterialDesignController::Initialize();

  EXPECT_TRUE(observer.on_md_mode_changed_called());

  test::MaterialDesignControllerTestAPI::Uninitialize();
  MaterialDesignController::GetInstance()->RemoveObserver(&observer);
}

TEST(MaterialDesignControllerObserver, TabletOnMdModeChanged) {
  // Verifies that the MaterialDesignControllerObserver gets called back when
  // the tablet mode toggles.
  test::MaterialDesignControllerTestAPI::SetDynamicRefreshUi(true);

  MaterialDesignController::Initialize();
  TestObserver tablet_enabled_observer;
  MaterialDesignController::GetInstance()->AddObserver(
      &tablet_enabled_observer);
  ASSERT_FALSE(tablet_enabled_observer.on_md_mode_changed_called());

#if !defined(OS_CHROMEOS)
  EXPECT_FALSE(MaterialDesignController::IsTouchOptimizedUiEnabled());
#endif

  MaterialDesignController::OnTabletModeToggled(true);

  EXPECT_TRUE(MaterialDesignController::IsTouchOptimizedUiEnabled());

  EXPECT_TRUE(tablet_enabled_observer.on_md_mode_changed_called());

  TestObserver tablet_disabled_observer;
  MaterialDesignController::GetInstance()->AddObserver(
      &tablet_disabled_observer);
  ASSERT_FALSE(tablet_disabled_observer.on_md_mode_changed_called());

  MaterialDesignController::OnTabletModeToggled(false);

#if !defined(OS_CHROMEOS)
  EXPECT_FALSE(MaterialDesignController::IsTouchOptimizedUiEnabled());
#endif

  EXPECT_TRUE(tablet_disabled_observer.on_md_mode_changed_called());

  test::MaterialDesignControllerTestAPI::Uninitialize();
  MaterialDesignController::GetInstance()->RemoveObserver(
      &tablet_disabled_observer);
  MaterialDesignController::GetInstance()->RemoveObserver(
      &tablet_enabled_observer);

  test::MaterialDesignControllerTestAPI::SetDynamicRefreshUi(false);
}

}  // namespace ui
