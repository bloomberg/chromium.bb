// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_widget.h"

#include "ash/focus_cycler.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/system_tray_focus_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/accessibility/dictation_button_tray.h"
#include "ash/system/accessibility/select_to_speak_tray.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/model/virtual_keyboard_model.h"
#include "ash/system/overview/overview_button_tray.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/session/logout_button_tray.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/status_area_overflow_button_tray.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/virtual_keyboard/virtual_keyboard_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/network/network_handler.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"

using session_manager::SessionState;

namespace ash {

using StatusAreaWidgetTest = AshTestBase;

// Tests that status area trays are constructed.
TEST_F(StatusAreaWidgetTest, Basics) {
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();

  // Status area is visible by default.
  EXPECT_TRUE(status->IsVisible());

  // No bubbles are open at startup.
  EXPECT_FALSE(status->IsMessageBubbleShown());

  // Auto-hidden shelf would not be forced to be visible.
  EXPECT_FALSE(status->ShouldShowShelf());

  // Default trays are constructed.
  EXPECT_TRUE(status->overview_button_tray());
  EXPECT_TRUE(status->unified_system_tray());
  EXPECT_TRUE(status->logout_button_tray_for_testing());
  EXPECT_TRUE(status->ime_menu_tray());
  EXPECT_TRUE(status->virtual_keyboard_tray_for_testing());
  EXPECT_TRUE(status->palette_tray());

  // Default trays are visible.
  EXPECT_FALSE(status->overview_button_tray()->GetVisible());
  EXPECT_TRUE(status->unified_system_tray()->GetVisible());
  EXPECT_FALSE(status->logout_button_tray_for_testing()->GetVisible());
  EXPECT_FALSE(status->ime_menu_tray()->GetVisible());
  EXPECT_FALSE(status->virtual_keyboard_tray_for_testing()->GetVisible());
}

class SystemTrayFocusTestObserver : public SystemTrayFocusObserver {
 public:
  SystemTrayFocusTestObserver() = default;
  ~SystemTrayFocusTestObserver() override = default;

  int focus_out_count() { return focus_out_count_; }
  int reverse_focus_out_count() { return reverse_focus_out_count_; }

 protected:
  // SystemTrayFocusObserver:
  void OnFocusLeavingSystemTray(bool reverse) override {
    reverse ? ++reverse_focus_out_count_ : ++focus_out_count_;
  }

 private:
  int focus_out_count_ = 0;
  int reverse_focus_out_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayFocusTestObserver);
};

class StatusAreaWidgetFocusTest : public AshTestBase {
 public:
  StatusAreaWidgetFocusTest() = default;
  ~StatusAreaWidgetFocusTest() override = default;

  // AshTestBase:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kShowWebUiLock);

    AshTestBase::SetUp();
    test_observer_.reset(new SystemTrayFocusTestObserver);
    Shell::Get()->system_tray_notifier()->AddSystemTrayFocusObserver(
        test_observer_.get());
  }

  // AshTestBase:
  void TearDown() override {
    Shell::Get()->system_tray_notifier()->RemoveSystemTrayFocusObserver(
        test_observer_.get());
    test_observer_.reset();
    AshTestBase::TearDown();
  }

  void GenerateTabEvent(bool reverse) {
    ui::KeyEvent tab_pressed(ui::ET_KEY_PRESSED, ui::VKEY_TAB,
                             reverse ? ui::EF_SHIFT_DOWN : ui::EF_NONE);
    StatusAreaWidgetTestHelper::GetStatusAreaWidget()->OnKeyEvent(&tab_pressed);
  }

 protected:
  std::unique_ptr<SystemTrayFocusTestObserver> test_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StatusAreaWidgetFocusTest);
};

// Tests that tab traversal through status area widget in non-active session
// could properly send FocusOut event.
// TODO(crbug.com/934939): Failing on trybot.
TEST_F(StatusAreaWidgetFocusTest, DISABLED_FocusOutObserverUnified) {
  // Set session state to LOCKED.
  SessionControllerImpl* session = Shell::Get()->session_controller();
  ASSERT_TRUE(session->IsActiveUserSessionStarted());
  TestSessionControllerClient* client = GetSessionControllerClient();
  client->SetSessionState(SessionState::LOCKED);
  ASSERT_TRUE(session->IsScreenLocked());

  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  // Default trays are constructed.
  ASSERT_TRUE(status->overview_button_tray());
  ASSERT_TRUE(status->unified_system_tray());
  ASSERT_TRUE(status->logout_button_tray_for_testing());
  ASSERT_TRUE(status->ime_menu_tray());
  ASSERT_TRUE(status->virtual_keyboard_tray_for_testing());

  // Default trays are visible.
  ASSERT_FALSE(status->overview_button_tray()->GetVisible());
  ASSERT_TRUE(status->unified_system_tray()->GetVisible());
  ASSERT_FALSE(status->logout_button_tray_for_testing()->GetVisible());
  ASSERT_FALSE(status->ime_menu_tray()->GetVisible());
  ASSERT_FALSE(status->virtual_keyboard_tray_for_testing()->GetVisible());

  // In Unified, we don't have notification tray, so ImeMenuTray is used for
  // tab testing.
  status->ime_menu_tray()->OnIMEMenuActivationChanged(true);
  ASSERT_TRUE(status->ime_menu_tray()->GetVisible());

  // Set focus to status area widget. The first focused view will be the IME
  // tray.
  ASSERT_TRUE(Shell::Get()->focus_cycler()->FocusWidget(status));
  views::FocusManager* focus_manager = status->GetFocusManager();
  EXPECT_EQ(status->ime_menu_tray(), focus_manager->GetFocusedView());

  // A tab key event will move focus to the system tray.
  GenerateTabEvent(false);
  EXPECT_EQ(status->unified_system_tray(), focus_manager->GetFocusedView());
  EXPECT_EQ(0, test_observer_->focus_out_count());
  EXPECT_EQ(0, test_observer_->reverse_focus_out_count());

  // Another tab key event will send FocusOut event, since we are not handling
  // this event, focus will remain within the status widhet and will be
  // moved to the IME tray.
  GenerateTabEvent(false);
  EXPECT_EQ(status->ime_menu_tray(), focus_manager->GetFocusedView());
  EXPECT_EQ(1, test_observer_->focus_out_count());
  EXPECT_EQ(0, test_observer_->reverse_focus_out_count());

  // A reverse tab key event will send reverse FocusOut event, since we are not
  // handling this event, focus will remain within the status widget and will
  // be moved to the system tray.
  GenerateTabEvent(true);
  EXPECT_EQ(status->unified_system_tray(), focus_manager->GetFocusedView());
  EXPECT_EQ(1, test_observer_->focus_out_count());
  EXPECT_EQ(1, test_observer_->reverse_focus_out_count());
}

class StatusAreaWidgetPaletteTest : public AshTestBase {
 public:
  StatusAreaWidgetPaletteTest() = default;
  ~StatusAreaWidgetPaletteTest() override = default;

  // testing::Test:
  void SetUp() override {
    base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
    cmd->AppendSwitch(switches::kAshForceEnableStylusTools);
    // It's difficult to write a test that marks the primary display as
    // internal before the status area is constructed. Just force the palette
    // for all displays.
    cmd->AppendSwitch(switches::kAshEnablePaletteOnAllDisplays);
    AshTestBase::SetUp();
  }
};

// Tests that the stylus palette tray is constructed.
TEST_F(StatusAreaWidgetPaletteTest, Basics) {
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  EXPECT_TRUE(status->palette_tray());

  // Auto-hidden shelf would not be forced to be visible.
  EXPECT_FALSE(status->ShouldShowShelf());
}

class UnifiedStatusAreaWidgetTest : public AshTestBase {
 public:
  UnifiedStatusAreaWidgetTest() = default;
  ~UnifiedStatusAreaWidgetTest() override = default;

  // AshTestBase:
  void SetUp() override {
    chromeos::shill_clients::InitializeFakes();
    // Initializing NetworkHandler before ash is more like production.
    chromeos::NetworkHandler::Initialize();
    AshTestBase::SetUp();
    chromeos::NetworkHandler::Get()->InitializePrefServices(&profile_prefs_,
                                                            &local_state_);
    // Networking stubs may have asynchronous initialization.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    // This roughly matches production shutdown order.
    chromeos::NetworkHandler::Get()->ShutdownPrefServices();
    AshTestBase::TearDown();
    chromeos::NetworkHandler::Shutdown();
    chromeos::shill_clients::Shutdown();
  }

 private:
  TestingPrefServiceSimple profile_prefs_;
  TestingPrefServiceSimple local_state_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedStatusAreaWidgetTest);
};

TEST_F(UnifiedStatusAreaWidgetTest, Basics) {
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  EXPECT_TRUE(status->unified_system_tray());
}

class StatusAreaWidgetVirtualKeyboardTest : public AshTestBase {
 protected:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
    ASSERT_TRUE(keyboard::IsKeyboardEnabled());
    keyboard::test::WaitUntilLoaded();

    // These tests only apply to the floating virtual keyboard, as it is the
    // only case where both the virtual keyboard and the shelf are visible.
    const gfx::Rect keyboard_bounds(0, 0, 1, 1);
    keyboard_ui_controller()->SetContainerType(
        keyboard::ContainerType::kFloating, keyboard_bounds, base::DoNothing());
  }

  keyboard::KeyboardUIController* keyboard_ui_controller() {
    return keyboard::KeyboardUIController::Get();
  }
};

// See https://crbug.com/897672.
TEST_F(StatusAreaWidgetVirtualKeyboardTest,
       ClickingVirtualKeyboardTrayHidesShownKeyboard) {
  // Set up the virtual keyboard tray icon along with some other tray icons.
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  status->virtual_keyboard_tray_for_testing()->SetVisiblePreferred(true);
  status->ime_menu_tray()->SetVisiblePreferred(true);

  keyboard_ui_controller()->ShowKeyboard(false /* locked */);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // The keyboard should hide when clicked.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      status->virtual_keyboard_tray_for_testing()
          ->GetBoundsInScreen()
          .CenterPoint());
  generator->ClickLeftButton();
  ASSERT_TRUE(keyboard::WaitUntilHidden());
}

// See https://crbug.com/897672.
TEST_F(StatusAreaWidgetVirtualKeyboardTest,
       TappingVirtualKeyboardTrayHidesShownKeyboard) {
  // Set up the virtual keyboard tray icon along with some other tray icons.
  StatusAreaWidget* status = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  status->virtual_keyboard_tray_for_testing()->SetVisiblePreferred(true);
  status->ime_menu_tray()->SetVisiblePreferred(true);

  keyboard_ui_controller()->ShowKeyboard(false /* locked */);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // The keyboard should hide when tapped.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureTapAt(status->virtual_keyboard_tray_for_testing()
                              ->GetBoundsInScreen()
                              .CenterPoint());
  ASSERT_TRUE(keyboard::WaitUntilHidden());
}

TEST_F(StatusAreaWidgetVirtualKeyboardTest, ClickingHidesVirtualKeyboard) {
  keyboard_ui_controller()->ShowKeyboard(false /* locked */);
  ASSERT_TRUE(keyboard_ui_controller()->IsKeyboardVisible());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()
          ->GetWindowBoundsInScreen()
          .CenterPoint());
  generator->ClickLeftButton();

  // Times out if test fails.
  ASSERT_TRUE(keyboard::WaitUntilHidden());
}

TEST_F(StatusAreaWidgetVirtualKeyboardTest, TappingHidesVirtualKeyboard) {
  keyboard_ui_controller()->ShowKeyboard(false /* locked */);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()
          ->GetWindowBoundsInScreen()
          .CenterPoint());
  generator->PressTouch();

  // Times out if test fails.
  ASSERT_TRUE(keyboard::WaitUntilHidden());
}

TEST_F(StatusAreaWidgetVirtualKeyboardTest, DoesNotHideLockedVirtualKeyboard) {
  keyboard_ui_controller()->ShowKeyboard(true /* locked */);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()
          ->GetWindowBoundsInScreen()
          .CenterPoint());

  generator->ClickLeftButton();
  EXPECT_FALSE(keyboard::IsKeyboardHiding());

  generator->PressTouch();
  EXPECT_FALSE(keyboard::IsKeyboardHiding());
}

class StatusAreaWidgetCollapseStateTest : public AshTestBase {
 protected:
  void SetUp() override {
    AshTestBase::SetUp();

    status_area_ = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
    overflow_button_ = status_area_->overflow_button_tray();
    virtual_keyboard_ = status_area_->virtual_keyboard_tray_for_testing();
    ime_menu_ = status_area_->ime_menu_tray();
    palette_ = status_area_->palette_tray();
    dictation_button_ = status_area_->dictation_button_tray();
    select_to_speak_ = status_area_->select_to_speak_tray();

    virtual_keyboard_->SetVisiblePreferred(true);
    ime_menu_->SetVisiblePreferred(true);
    palette_->SetVisiblePreferred(true);
    dictation_button_->SetVisiblePreferred(true);
    select_to_speak_->SetVisiblePreferred(true);
  }

  void SetCollapseState(StatusAreaWidget::CollapseState collapse_state) {
    status_area_->set_collapse_state_for_test(collapse_state);

    virtual_keyboard_->UpdateAfterStatusAreaCollapseChange();
    ime_menu_->UpdateAfterStatusAreaCollapseChange();
    palette_->UpdateAfterStatusAreaCollapseChange();
    dictation_button_->UpdateAfterStatusAreaCollapseChange();
    select_to_speak_->UpdateAfterStatusAreaCollapseChange();
  }

  StatusAreaWidget::CollapseState collapse_state() const {
    return status_area_->collapse_state();
  }

  StatusAreaWidget* status_area_;
  StatusAreaOverflowButtonTray* overflow_button_;
  TrayBackgroundView* virtual_keyboard_;
  TrayBackgroundView* ime_menu_;
  TrayBackgroundView* palette_;
  TrayBackgroundView* dictation_button_;
  TrayBackgroundView* select_to_speak_;
};

TEST_F(StatusAreaWidgetCollapseStateTest, TrayVisibility) {
  // Initial visibility.
  ime_menu_->SetVisiblePreferred(false);
  virtual_keyboard_->set_show_when_collapsed(false);
  palette_->set_show_when_collapsed(true);
  EXPECT_FALSE(ime_menu_->GetVisible());
  EXPECT_TRUE(virtual_keyboard_->GetVisible());
  EXPECT_TRUE(palette_->GetVisible());

  // Post-collapse visibility.
  SetCollapseState(StatusAreaWidget::CollapseState::COLLAPSED);
  EXPECT_FALSE(ime_menu_->GetVisible());
  EXPECT_FALSE(virtual_keyboard_->GetVisible());
  EXPECT_TRUE(palette_->GetVisible());

  // Expanded visibility.
  SetCollapseState(StatusAreaWidget::CollapseState::EXPANDED);
  EXPECT_FALSE(ime_menu_->GetVisible());
  EXPECT_TRUE(virtual_keyboard_->GetVisible());
  EXPECT_TRUE(palette_->GetVisible());
}

TEST_F(StatusAreaWidgetCollapseStateTest, ImeMenuShownWithVirtualKeyboard) {
  // Set up tray items.
  ime_menu_->set_show_when_collapsed(false);
  palette_->set_show_when_collapsed(true);

  // Collapsing the status area should hide the IME menu tray item.
  SetCollapseState(StatusAreaWidget::CollapseState::COLLAPSED);
  EXPECT_FALSE(ime_menu_->GetVisible());
  EXPECT_TRUE(palette_->GetVisible());

  // But only the IME menu tray item should be shown after showing keyboard,
  // simulated here by OnArcInputMethodSurfaceBoundsChanged().
  Shell::Get()
      ->system_tray_model()
      ->virtual_keyboard()
      ->OnArcInputMethodSurfaceBoundsChanged(gfx::Rect(0, 0, 100, 100));
  EXPECT_TRUE(ime_menu_->GetVisible());
  EXPECT_FALSE(palette_->GetVisible());
  EXPECT_FALSE(virtual_keyboard_->GetVisible());
  EXPECT_FALSE(dictation_button_->GetVisible());
  EXPECT_FALSE(select_to_speak_->GetVisible());
}

TEST_F(StatusAreaWidgetCollapseStateTest, OverflowButtonShownWhenCollapsible) {
  EXPECT_FALSE(overflow_button_->GetVisible());
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshForceStatusAreaCollapsible);
  status_area_->UpdateCollapseState();
  EXPECT_EQ(StatusAreaWidget::CollapseState::COLLAPSED, collapse_state());
  EXPECT_TRUE(overflow_button_->GetVisible());
}

TEST_F(StatusAreaWidgetCollapseStateTest, ClickOverflowButton) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshForceStatusAreaCollapsible);
  status_area_->UpdateCollapseState();

  // By default, status area is collapsed.
  EXPECT_EQ(StatusAreaWidget::CollapseState::COLLAPSED, collapse_state());
  EXPECT_FALSE(select_to_speak_->GetVisible());
  EXPECT_FALSE(ime_menu_->GetVisible());
  EXPECT_FALSE(virtual_keyboard_->GetVisible());
  EXPECT_TRUE(palette_->GetVisible());
  EXPECT_TRUE(overflow_button_->GetVisible());

  // Click overflow button.
  gfx::Point point = overflow_button_->GetBoundsInScreen().origin();
  ui::MouseEvent click(ui::ET_MOUSE_PRESSED, point, point,
                       base::TimeTicks::Now(), 0, 0);
  overflow_button_->PerformAction(click);

  // All tray buttons should be visible in the expanded state.
  EXPECT_EQ(StatusAreaWidget::CollapseState::EXPANDED, collapse_state());
  EXPECT_TRUE(select_to_speak_->GetVisible());
  EXPECT_TRUE(ime_menu_->GetVisible());
  EXPECT_TRUE(virtual_keyboard_->GetVisible());
  EXPECT_TRUE(palette_->GetVisible());
  EXPECT_TRUE(overflow_button_->GetVisible());

  // Clicking the overflow button again should go back to the collapsed state.
  overflow_button_->PerformAction(click);
  EXPECT_EQ(StatusAreaWidget::CollapseState::COLLAPSED, collapse_state());
  EXPECT_FALSE(select_to_speak_->GetVisible());
  EXPECT_FALSE(ime_menu_->GetVisible());
  EXPECT_FALSE(virtual_keyboard_->GetVisible());
  EXPECT_TRUE(palette_->GetVisible());
  EXPECT_TRUE(overflow_button_->GetVisible());
}

TEST_F(StatusAreaWidgetCollapseStateTest, NewTrayShownWhileCollapsed) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshForceStatusAreaCollapsible);
  palette_->SetVisiblePreferred(false);
  status_area_->UpdateCollapseState();

  // The palette tray button should not be visible initially.
  EXPECT_EQ(StatusAreaWidget::CollapseState::COLLAPSED, collapse_state());
  EXPECT_FALSE(ime_menu_->GetVisible());
  EXPECT_TRUE(virtual_keyboard_->GetVisible());
  EXPECT_FALSE(palette_->GetVisible());

  // Showing it should replace the virtual keyboard tray button as it has higher
  // priority.
  palette_->SetVisiblePreferred(true);
  EXPECT_EQ(StatusAreaWidget::CollapseState::COLLAPSED, collapse_state());
  EXPECT_FALSE(ime_menu_->GetVisible());
  EXPECT_FALSE(virtual_keyboard_->GetVisible());
  EXPECT_TRUE(palette_->GetVisible());
}

TEST_F(StatusAreaWidgetCollapseStateTest, TrayHiddenWhileCollapsed) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshForceStatusAreaCollapsible);
  status_area_->UpdateCollapseState();

  // The palette tray button should not visible initially.
  EXPECT_EQ(StatusAreaWidget::CollapseState::COLLAPSED, collapse_state());
  EXPECT_FALSE(ime_menu_->GetVisible());
  EXPECT_FALSE(virtual_keyboard_->GetVisible());
  EXPECT_TRUE(palette_->GetVisible());

  // Hiding it should make the virtual keyboard tray button replace it.
  palette_->SetVisiblePreferred(false);
  EXPECT_EQ(StatusAreaWidget::CollapseState::COLLAPSED, collapse_state());
  EXPECT_FALSE(ime_menu_->GetVisible());
  EXPECT_TRUE(virtual_keyboard_->GetVisible());
  EXPECT_FALSE(palette_->GetVisible());
}

TEST_F(StatusAreaWidgetCollapseStateTest, AllTraysFitInCollapsedState) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshForceStatusAreaCollapsible);
  status_area_->UpdateCollapseState();
  EXPECT_EQ(StatusAreaWidget::CollapseState::COLLAPSED, collapse_state());

  // If all tray buttons can fit in the available space, the overflow button is
  // now shown.
  select_to_speak_->SetVisiblePreferred(false);
  ime_menu_->SetVisiblePreferred(false);
  dictation_button_->SetVisiblePreferred(false);
  EXPECT_EQ(StatusAreaWidget::CollapseState::NOT_COLLAPSIBLE, collapse_state());
  EXPECT_FALSE(overflow_button_->GetVisible());
}

}  // namespace ash
