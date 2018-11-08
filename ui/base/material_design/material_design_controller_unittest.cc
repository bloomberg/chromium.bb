// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/material_design/material_design_controller_observer.h"
#include "ui/base/test/material_design_controller_test_api.h"
#include "ui/base/ui_base_switches.h"

namespace ui {

TEST(MaterialDesignControllerDeathTest, CrashesWithoutInitialization) {
  ASSERT_FALSE(MaterialDesignController::is_mode_initialized());
  EXPECT_DEATH_IF_SUPPORTED({ MaterialDesignController::GetMode(); }, "");
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

TEST_F(MaterialDesignControllerTest, NoCommandLineFlagIsRefresh) {
  ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kTopChromeMD));
  EXPECT_EQ(MaterialDesignController::DefaultMode(),
            MaterialDesignController::GetMode());

  EXPECT_FALSE(MaterialDesignController::IsTouchOptimizedUiEnabled());
  EXPECT_TRUE(MaterialDesignController::IsNewerMaterialUi());
  EXPECT_TRUE(MaterialDesignController::IsRefreshUi());
}

namespace {

class MaterialDesignControllerTestCommandLineForceMaterial
    : public MaterialDesignControllerTest {
 public:
  MaterialDesignControllerTestCommandLineForceMaterial() {
    SetCommandLineSwitch(switches::kTopChromeMDMaterial);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      MaterialDesignControllerTestCommandLineForceMaterial);
};

}  // namespace

// Verify command line value |switches::kTopChromeMDMaterial| maps to
// Mode::MATERIAL when the compile time flag is defined.
TEST_F(MaterialDesignControllerTestCommandLineForceMaterial, CheckApiReturns) {
  EXPECT_EQ(MaterialDesignController::Mode::MATERIAL_NORMAL,
            MaterialDesignController::GetMode());

  EXPECT_FALSE(MaterialDesignController::IsTouchOptimizedUiEnabled());
  EXPECT_FALSE(MaterialDesignController::IsNewerMaterialUi());
  EXPECT_FALSE(MaterialDesignController::IsRefreshUi());
}

namespace {

class MaterialDesignControllerTestCommandLineForceHybrid
    : public MaterialDesignControllerTest {
 public:
  MaterialDesignControllerTestCommandLineForceHybrid() {
    SetCommandLineSwitch(switches::kTopChromeMDMaterialHybrid);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MaterialDesignControllerTestCommandLineForceHybrid);
};

}  // namespace

// Verify command line value |switches::kTopChromeMDMaterialHybrid| maps to
// Mode::MATERIAL_HYBRID when the command line flag is defined.
TEST_F(MaterialDesignControllerTestCommandLineForceHybrid, CheckApiReturns) {
  EXPECT_EQ(MaterialDesignController::Mode::MATERIAL_HYBRID,
            MaterialDesignController::GetMode());

  EXPECT_FALSE(MaterialDesignController::IsTouchOptimizedUiEnabled());
  EXPECT_FALSE(MaterialDesignController::IsNewerMaterialUi());
  EXPECT_FALSE(MaterialDesignController::IsRefreshUi());
}

namespace {

class MaterialDesignControllerTestCommandLineForceTouchOptimized
    : public MaterialDesignControllerTest {
 public:
  MaterialDesignControllerTestCommandLineForceTouchOptimized() {
    SetCommandLineSwitch("material-touch-optimized");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      MaterialDesignControllerTestCommandLineForceTouchOptimized);
};

}  // namespace

// Verify command line value "material-touch-optimized" maps to
// Mode::MATERIAL_TOUCH_OPTIMIZED when the command line flag is defined.
TEST_F(MaterialDesignControllerTestCommandLineForceTouchOptimized,
       CheckApiReturns) {
  EXPECT_EQ(MaterialDesignController::Mode::MATERIAL_TOUCH_OPTIMIZED,
            MaterialDesignController::GetMode());

  EXPECT_TRUE(MaterialDesignController::IsTouchOptimizedUiEnabled());
  EXPECT_TRUE(MaterialDesignController::IsNewerMaterialUi());
  EXPECT_FALSE(MaterialDesignController::IsRefreshUi());
}

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

// Verify command line value |switches::kTopChromeMDMaterialRefresh| maps to
// Mode::MATERIAL_REFRESH when the command line flag is defined.
TEST_F(MaterialDesignControllerTestCommandLineRefresh, CheckApiReturns) {
  EXPECT_EQ(MaterialDesignController::Mode::MATERIAL_REFRESH,
            MaterialDesignController::GetMode());

  EXPECT_FALSE(MaterialDesignController::IsTouchOptimizedUiEnabled());
  EXPECT_TRUE(MaterialDesignController::IsNewerMaterialUi());
  EXPECT_TRUE(MaterialDesignController::IsRefreshUi());
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

// Verify command line value
// |switches::kTopChromeMDMaterialRefreshTouchOptimized| maps to
// Mode::MATERIAL_REFRESH when the command line flag is defined.
TEST_F(MaterialDesignControllerTestCommandLineForceTouchRefresh,
       CheckApiReturns) {
  EXPECT_EQ(MaterialDesignController::Mode::MATERIAL_TOUCH_REFRESH,
            MaterialDesignController::GetMode());

  EXPECT_TRUE(MaterialDesignController::IsTouchOptimizedUiEnabled());
  EXPECT_TRUE(MaterialDesignController::IsNewerMaterialUi());
  EXPECT_TRUE(MaterialDesignController::IsRefreshUi());
}

namespace {

class MaterialDesignControllerTestDefault :
    public MaterialDesignControllerTest {
 public:
  MaterialDesignControllerTestDefault() {
    SetCommandLineSwitch(std::string());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MaterialDesignControllerTestDefault);
};

}  // namespace

// Verify command line value "" maps to the default mode when the command line
// flag is defined.
TEST_F(MaterialDesignControllerTestDefault, CheckApiReturns) {
  EXPECT_EQ(MaterialDesignController::DefaultMode(),
            MaterialDesignController::GetMode());

  EXPECT_FALSE(MaterialDesignController::IsTouchOptimizedUiEnabled());
  EXPECT_TRUE(MaterialDesignController::IsNewerMaterialUi());
  EXPECT_TRUE(MaterialDesignController::IsRefreshUi());
}

namespace {

class MaterialDesignControllerTestCommandLineForceInvalidValue
    : public MaterialDesignControllerTest {
 public:
  MaterialDesignControllerTestCommandLineForceInvalidValue() {
    const std::string kInvalidValue = "1nvalid-valu3";
    SetCommandLineSwitch(kInvalidValue);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      MaterialDesignControllerTestCommandLineForceInvalidValue);
};

}  // namespace

// Verify an invalid command line value uses the default mode.
TEST_F(MaterialDesignControllerTestCommandLineForceInvalidValue,
       CheckApiReturns) {
  EXPECT_EQ(MaterialDesignController::DefaultMode(),
            MaterialDesignController::GetMode());

  EXPECT_FALSE(MaterialDesignController::IsTouchOptimizedUiEnabled());
  EXPECT_TRUE(MaterialDesignController::IsNewerMaterialUi());
  EXPECT_TRUE(MaterialDesignController::IsRefreshUi());
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

  EXPECT_EQ(MaterialDesignController::DefaultMode(),
            MaterialDesignController::GetMode());

  MaterialDesignController::OnTabletModeToggled(true);

  EXPECT_EQ(MaterialDesignController::Mode::MATERIAL_TOUCH_REFRESH,
            MaterialDesignController::GetMode());

  EXPECT_TRUE(tablet_enabled_observer.on_md_mode_changed_called());

  TestObserver tablet_disabled_observer;
  MaterialDesignController::GetInstance()->AddObserver(
      &tablet_disabled_observer);
  ASSERT_FALSE(tablet_disabled_observer.on_md_mode_changed_called());

  MaterialDesignController::OnTabletModeToggled(false);

  EXPECT_EQ(MaterialDesignController::DefaultMode(),
            MaterialDesignController::GetMode());

  EXPECT_TRUE(tablet_disabled_observer.on_md_mode_changed_called());

  test::MaterialDesignControllerTestAPI::Uninitialize();
  MaterialDesignController::GetInstance()->RemoveObserver(
      &tablet_disabled_observer);
  MaterialDesignController::GetInstance()->RemoveObserver(
      &tablet_enabled_observer);

  test::MaterialDesignControllerTestAPI::SetDynamicRefreshUi(false);
}

}  // namespace ui
