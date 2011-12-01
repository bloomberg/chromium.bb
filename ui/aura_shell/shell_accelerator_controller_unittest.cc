// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/shell.h"
#include "ui/aura_shell/shell_accelerator_controller.h"
#include "ui/aura_shell/test/aura_shell_test_base.h"

namespace aura_shell {
namespace test {

namespace {
class TestTarget : public ui::AcceleratorTarget {
 public:
  TestTarget() : accelerator_pressed_(false) {};
  virtual ~TestTarget() {};

  bool accelerator_pressed() const {
    return accelerator_pressed_;
  }

  void set_accelerator_pressed(bool accelerator_pressed) {
    accelerator_pressed_ = accelerator_pressed;
  }

  // Overridden from ui::AcceleratorTarget:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;

 private:
  bool accelerator_pressed_;
};

bool TestTarget::AcceleratorPressed(const ui::Accelerator& accelerator) {
  set_accelerator_pressed(true);
  return true;
}

}  // namespace

class ShellAcceleratorControllerTest : public AuraShellTestBase {
 public:
  ShellAcceleratorControllerTest() {};
  virtual ~ShellAcceleratorControllerTest() {};

  static ShellAcceleratorController* GetController();

  // testing::Test:
  // virtual void SetUp() OVERRIDE;
  // virtual void TearDown() OVERRIDE;
};

ShellAcceleratorController* ShellAcceleratorControllerTest::GetController() {
  return Shell::GetInstance()->accelerator_controller();
}

// void ShellAcceleratorControllerTest::SetUp() {
//   AuraShellTestBase::SetUp();
// }

// void ShellAcceleratorControllerTest::TearDown() {
//   AuraShellTestBase::TearDown();
// }

TEST_F(ShellAcceleratorControllerTest, Register) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, false, false, false);
  TestTarget target;
  GetController()->Register(accelerator_a, &target);

  // The registered accelerator is processed.
  EXPECT_TRUE(GetController()->Process(accelerator_a));
  EXPECT_TRUE(target.accelerator_pressed());
}

TEST_F(ShellAcceleratorControllerTest, RegisterMultipleTarget) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, false, false, false);
  TestTarget target1;
  GetController()->Register(accelerator_a, &target1);
  TestTarget target2;
  GetController()->Register(accelerator_a, &target2);

  // If multiple targets are registered with the same accelerator, the target
  // registered later processes the accelerator.
  EXPECT_TRUE(GetController()->Process(accelerator_a));
  EXPECT_FALSE(target1.accelerator_pressed());
  EXPECT_TRUE(target2.accelerator_pressed());
}

TEST_F(ShellAcceleratorControllerTest, Unregister) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, false, false, false);
  TestTarget target;
  GetController()->Register(accelerator_a, &target);
  const ui::Accelerator accelerator_b(ui::VKEY_B, false, false, false);
  GetController()->Register(accelerator_b, &target);

  // Unregistering a different accelerator does not affect the other
  // accelerator.
  GetController()->Unregister(accelerator_b, &target);
  EXPECT_TRUE(GetController()->Process(accelerator_a));
  EXPECT_TRUE(target.accelerator_pressed());

  // The unregistered accelerator is no longer processed.
  target.set_accelerator_pressed(false);
  GetController()->Unregister(accelerator_a, &target);
  EXPECT_FALSE(GetController()->Process(accelerator_a));
  EXPECT_FALSE(target.accelerator_pressed());
}

TEST_F(ShellAcceleratorControllerTest, UnregisterAll) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, false, false, false);
  TestTarget target1;
  GetController()->Register(accelerator_a, &target1);
  const ui::Accelerator accelerator_b(ui::VKEY_B, false, false, false);
  GetController()->Register(accelerator_b, &target1);
  const ui::Accelerator accelerator_c(ui::VKEY_C, false, false, false);
  TestTarget target2;
  GetController()->Register(accelerator_c, &target2);
  GetController()->UnregisterAll(&target1);

  // All the accelerators registered for |target1| are no longer processed.
  EXPECT_FALSE(GetController()->Process(accelerator_a));
  EXPECT_FALSE(GetController()->Process(accelerator_b));
  EXPECT_FALSE(target1.accelerator_pressed());

  // UnregisterAll with a different target does not affect the other target.
  EXPECT_TRUE(GetController()->Process(accelerator_c));
  EXPECT_TRUE(target2.accelerator_pressed());
}

TEST_F(ShellAcceleratorControllerTest, Process) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, false, false, false);
  TestTarget target1;
  GetController()->Register(accelerator_a, &target1);

  // The registered accelerator is processed.
  EXPECT_TRUE(GetController()->Process(accelerator_a));
  EXPECT_TRUE(target1.accelerator_pressed());

  // The non-registered accelerator is not processed.
  const ui::Accelerator accelerator_b(ui::VKEY_B, false, false, false);
  EXPECT_FALSE(GetController()->Process(accelerator_b));
}

TEST_F(ShellAcceleratorControllerTest, GlobalAccelerators) {
  // TODO(mazda): Uncomment the followings once they are implemented.
  // CycleBackward
  // EXPECT_TRUE(GetController()->Process(
  //     ui::Accelerator(ui::VKEY_TAB, true, false, true)));
  // CycleForwrard
  // EXPECT_TRUE(GetController()->Process(
  //     ui::Accelerator(ui::VKEY_TAB, false, false, true)));
  // TakeScreenshot
  // EXPECT_TRUE(GetController()->Process(
  //     ui::Accelerator(ui::VKEY_F5, false, true, false)));
  // EXPECT_TRUE(GetController()->Process(
  //     ui::Accelerator(ui::VKEY_PRINT, false, false, false)));
#if !defined(NDEBUG)
  // TODO(mazda): Callig RotateScreen in unit test causes a crash because of
  // "pure virtual method called" for some reasons. Need to investigate.
  // RotateScreen
  // EXPECT_TRUE(GetController()->Process(
  //     ui::Accelerator(ui::VKEY_HOME, false, true, false)));
#if !defined(OS_LINUX)
  // ToggleDesktopFullScreen (not implemented yet on Linux)
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_F11, false, true, false)));
#endif
#endif
}

}  // namespace test
}  // namespace aura_shell
