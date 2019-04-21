// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_window_resizer.h"

#include <string>
#include <utility>

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/pip/pip_test_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/public/keyboard_switches.h"
#include "ui/keyboard/test/keyboard_test_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

std::unique_ptr<views::Widget> CreateWidget(aura::Window* context) {
  std::unique_ptr<views::Widget> widget(new views::Widget);
  views::Widget::InitParams params;
  params.delegate = new views::WidgetDelegateView();
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = context;
  widget->Init(params);
  return widget;
}

}  // namespace

class PipTest : public AshTestBase {
 public:
  PipTest() = default;
  ~PipTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
  }

  void TearDown() override { AshTestBase::TearDown(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(PipTest);
};

TEST_F(PipTest, ShowInactive) {
  auto widget = CreateWidget(Shell::GetPrimaryRootWindow());
  const wm::WMEvent pip_event(wm::WM_EVENT_PIP);
  auto* window_state = wm::GetWindowState(widget->GetNativeWindow());
  window_state->OnWMEvent(&pip_event);
  ASSERT_TRUE(window_state->IsPip());
  ASSERT_FALSE(widget->IsVisible());
  widget->Show();
  ASSERT_TRUE(widget->IsVisible());
  EXPECT_FALSE(widget->IsActive());

  widget->Activate();
  EXPECT_FALSE(widget->IsActive());

  const wm::WMEvent normal_event(wm::WM_EVENT_NORMAL);
  window_state->OnWMEvent(&normal_event);
  EXPECT_FALSE(window_state->IsPip());
  EXPECT_FALSE(widget->IsActive());

  widget->Activate();
  EXPECT_TRUE(widget->IsActive());

  window_state->OnWMEvent(&pip_event);
  EXPECT_FALSE(widget->IsActive());
}

TEST_F(PipTest, ShortcutNavigation) {
  auto widget = CreateWidget(Shell::GetPrimaryRootWindow());
  auto pip_widget = CreateWidget(Shell::GetPrimaryRootWindow());
  widget->Show();
  pip_widget->Show();
  const wm::WMEvent pip_event(wm::WM_EVENT_PIP);
  auto* pip_window_state = wm::GetWindowState(pip_widget->GetNativeWindow());
  pip_window_state->OnWMEvent(&pip_event);
  EXPECT_TRUE(pip_window_state->IsPip());
  EXPECT_FALSE(pip_widget->IsActive());
  ASSERT_TRUE(widget->IsActive());

  auto* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_V, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
  EXPECT_TRUE(pip_widget->IsActive());
  EXPECT_FALSE(widget->IsActive());

  auto* shelf = AshTestBase::GetPrimaryShelf()->shelf_widget();
  auto* status_area =
      Shell::GetPrimaryRootWindowController()->GetStatusAreaWidget();

  // Cycle Backward.
  generator->PressKey(ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(shelf->IsActive());

  generator->PressKey(ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(status_area->IsActive());

  generator->PressKey(ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(widget->IsActive());

  generator->PressKey(ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(pip_widget->IsActive());

  // Forward
  generator->PressKey(ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(widget->IsActive());

  generator->PressKey(ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(status_area->IsActive());

  generator->PressKey(ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(shelf->IsActive());

  generator->PressKey(ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(pip_widget->IsActive());
}

TEST_F(PipTest, PipInitialPositionAvoidsObstacles) {
  UpdateDisplay("400x400");
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 300, 100, 100)));
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  const wm::WMEvent enter_pip(wm::WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  window->Show();

  auto* keyboard_controller = keyboard::KeyboardController::Get();
  keyboard_controller->ShowKeyboard(/*lock=*/true);
  ASSERT_TRUE(keyboard::WaitUntilShown());
  aura::Window* keyboard_window = keyboard_controller->GetKeyboardWindow();
  keyboard_window->SetBounds(gfx::Rect(0, 300, 400, 100));

  // Expect the PIP position is shifted below the keyboard.
  EXPECT_TRUE(window_state->IsPip());
  EXPECT_TRUE(window->layer()->visible());
  EXPECT_EQ(gfx::Rect(100, 192, 100, 100), window->layer()->GetTargetBounds());
}

TEST_F(PipTest, TargetBoundsAffectedByWorkAreaChange) {
  UpdateDisplay("400x400");

  // Place a keyboard window at the initial position of a PIP window.
  auto* keyboard_controller = keyboard::KeyboardController::Get();
  keyboard_controller->ShowKeyboard(/*lock=*/true);
  ASSERT_TRUE(keyboard::WaitUntilShown());
  aura::Window* keyboard_window = keyboard_controller->GetKeyboardWindow();
  keyboard_window->SetBounds(gfx::Rect(0, 300, 400, 100));

  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 300, 100, 100)));
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  const wm::WMEvent enter_pip(wm::WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  window->Show();

  // Ensure the initial PIP position is shifted below the keyboard.
  EXPECT_TRUE(window_state->IsPip());
  EXPECT_TRUE(window->layer()->visible());
  EXPECT_EQ(gfx::Rect(100, 192, 100, 100), window->bounds());
}

TEST_F(PipTest, PipRestoresToPreviousBoundsOnMovementAreaChangeIfTheyExist) {
  ForceHideShelvesForTest();
  UpdateDisplay("400x400");
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(200, 200, 100, 100)));
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  const wm::WMEvent enter_pip(wm::WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  window->Show();

  // Position the PIP window on the side of the screen where it will be next
  // to an edge and therefore in a resting position for the whole test.
  const gfx::Rect bounds = gfx::Rect(292, 200, 100, 100);
  // Set restore position to where the window currently is.
  window->SetBounds(bounds);
  window_state->SetRestoreBoundsInParent(bounds);
  EXPECT_TRUE(window_state->HasRestoreBounds());

  // Update the work area so that the PIP window should be pushed upward.
  UpdateDisplay("400x200");
  ForceHideShelvesForTest();

  // PIP should move up to accommodate the new work area.
  EXPECT_EQ(gfx::Rect(292, 92, 100, 100), window->GetBoundsInScreen());

  // Restore the original work area.
  UpdateDisplay("400x400");
  ForceHideShelvesForTest();

  // Expect that the PIP window is put back to where it was before.
  EXPECT_EQ(gfx::Rect(292, 200, 100, 100), window->GetBoundsInScreen());
}

TEST_F(
    PipTest,
    PipRestoresToPreviousBoundsOnMovementAreaChangeIfTheyExistOnExternalDisplay) {
  UpdateDisplay("400x400,400x400");
  ForceHideShelvesForTest();
  auto* root_window = Shell::GetAllRootWindows()[1];

  // Position the PIP window on the side of the screen where it will be next
  // to an edge and therefore in a resting position for the whole test.
  auto widget = CreateWidget(root_window);
  auto* window = widget->GetNativeWindow();
  wm::WindowState* window_state = wm::GetWindowState(window);
  const wm::WMEvent enter_pip(wm::WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  window->Show();
  window->SetBounds(gfx::Rect(8, 292, 100, 100));

  // Set restore position to where the window currently is.
  window_state->SetRestoreBoundsInParent(window->bounds());
  EXPECT_TRUE(window_state->HasRestoreBounds());

  // Update the work area so that the PIP window should be pushed upward.
  UpdateDisplay("400x400,400x200");
  ForceHideShelvesForTest();

  // PIP should move up to accommodate the new work area.
  EXPECT_EQ(gfx::Rect(408, 92, 100, 100), window->GetBoundsInScreen());

  // Restore the original work area.
  UpdateDisplay("400x400,400x400");
  ForceHideShelvesForTest();

  // Expect that the PIP window is put back to where it was before.
  EXPECT_EQ(gfx::Rect(408, 292, 100, 100), window->GetBoundsInScreen());
}

TEST_F(PipTest, PipRestoreOnWorkAreaChangeDoesNotChangeWindowSize) {
  ForceHideShelvesForTest();
  UpdateDisplay("400x400");
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(200, 200, 100, 100)));
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  const wm::WMEvent enter_pip(wm::WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  window->Show();

  // Position the PIP window on the side of the screen where it will be next
  // to an edge and therefore in a resting position for the whole test.
  const gfx::Rect bounds = gfx::Rect(292, 200, 100, 100);
  window->SetBounds(bounds);
  // Set the restore bounds to be a different size.
  window_state->SetRestoreBoundsInParent(gfx::Rect(342, 250, 50, 100));
  EXPECT_TRUE(window_state->HasRestoreBounds());

  // Update the work area so that the PIP window should be pushed upward.
  UpdateDisplay("400x200");
  ForceHideShelvesForTest();

  // The PIP window should not change size.
  EXPECT_EQ(gfx::Rect(292, 92, 100, 100), window->GetBoundsInScreen());
}

}  // namespace ash
