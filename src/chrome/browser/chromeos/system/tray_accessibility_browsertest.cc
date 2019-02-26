// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/interfaces/system_tray_test_api.mojom.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/test_utils.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

using testing::Return;
using testing::_;
using testing::WithParamInterface;

namespace chromeos {

enum PrefSettingMechanism {
  PREF_SERVICE,
  POLICY,
};

namespace {

////////////////////////////////////////////////////////////////////////////////
// Changing accessibility settings may change preferences, so these helpers spin
// the message loop to ensure ash sees the change.

void SetMagnifierEnabled(bool enabled) {
  MagnificationManager::Get()->SetMagnifierEnabled(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableSpokenFeedback(bool enabled) {
  AccessibilityManager::Get()->EnableSpokenFeedback(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableSelectToSpeak(bool enabled) {
  AccessibilityManager::Get()->SetSelectToSpeakEnabled(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableHighContrast(bool enabled) {
  AccessibilityManager::Get()->EnableHighContrast(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableAutoclick(bool enabled) {
  AccessibilityManager::Get()->EnableAutoclick(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableVirtualKeyboard(bool enabled) {
  AccessibilityManager::Get()->EnableVirtualKeyboard(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableLargeCursor(bool enabled) {
  AccessibilityManager::Get()->EnableLargeCursor(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableMonoAudio(bool enabled) {
  AccessibilityManager::Get()->EnableMonoAudio(enabled);
  base::RunLoop().RunUntilIdle();
}

void SetCaretHighlightEnabled(bool enabled) {
  AccessibilityManager::Get()->SetCaretHighlightEnabled(enabled);
  base::RunLoop().RunUntilIdle();
}

void SetCursorHighlightEnabled(bool enabled) {
  AccessibilityManager::Get()->SetCursorHighlightEnabled(enabled);
  base::RunLoop().RunUntilIdle();
}

void SetFocusHighlightEnabled(bool enabled) {
  AccessibilityManager::Get()->SetFocusHighlightEnabled(enabled);
  base::RunLoop().RunUntilIdle();
}

void EnableStickyKeys(bool enabled) {
  AccessibilityManager::Get()->EnableStickyKeys(enabled);
  base::RunLoop().RunUntilIdle();
}

}  // namespace

// Uses InProcessBrowserTest instead of OobeBaseTest because most of the tests
// don't need to test the login screen.
class TrayAccessibilityTest
    : public InProcessBrowserTest,
      public WithParamInterface<PrefSettingMechanism> {
 public:
  TrayAccessibilityTest()
      : disable_animations_(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {}
  ~TrayAccessibilityTest() override = default;

  // The profile which should be used by these tests.
  Profile* GetProfile() { return ProfileManager::GetActiveUserProfile(); }

  // Connect / reconnect to SystemTrayTestApi.
  void BindTestApi() {
    tray_test_api_.reset();
    content::ServiceManagerConnection::GetForProcess()
        ->GetConnector()
        ->BindInterface(ash::mojom::kServiceName, &tray_test_api_);
  }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    BindTestApi();
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetShowAccessibilityOptionsInSystemTrayMenu(bool value) {
    if (GetParam() == PREF_SERVICE) {
      PrefService* prefs = GetProfile()->GetPrefs();
      prefs->SetBoolean(ash::prefs::kShouldAlwaysShowAccessibilityMenu, value);
      // Prefs are sent to ash asynchronously.
      base::RunLoop().RunUntilIdle();
    } else if (GetParam() == POLICY) {
      policy::PolicyMap policy_map;
      policy_map.Set(policy::key::kShowAccessibilityOptionsInSystemTrayMenu,
                     policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                     policy::POLICY_SOURCE_CLOUD,
                     std::make_unique<base::Value>(value), nullptr);
      provider_.UpdateChromePolicy(policy_map);
      base::RunLoop().RunUntilIdle();
    } else {
      FAIL() << "Unknown test parameterization";
    }
  }

  bool IsMenuButtonVisible() {
    ash::mojom::SystemTrayTestApiAsyncWaiter wait_for(tray_test_api_.get());
    bool visible = false;
    wait_for.IsBubbleViewVisible(ash::VIEW_ID_ACCESSIBILITY_TRAY_ITEM,
                                 true /* open_tray */, &visible);
    wait_for.CloseBubble();
    return visible;
  }

  void CreateDetailedMenu() {
    ash::mojom::SystemTrayTestApiAsyncWaiter wait_for(tray_test_api_.get());
    wait_for.ShowDetailedView(ash::mojom::TrayItem::kAccessibility);
  }

  bool IsBubbleOpen() {
    ash::mojom::SystemTrayTestApiAsyncWaiter wait_for(tray_test_api_.get());
    bool is_open = false;
    wait_for.IsTrayBubbleOpen(&is_open);
    return is_open;
  }

  void ClickAutoclickOnDetailMenu() {
    ash::mojom::SystemTrayTestApiAsyncWaiter wait_for(tray_test_api_.get());
    wait_for.ClickBubbleView(ash::VIEW_ID_ACCESSIBILITY_AUTOCLICK);
  }

  bool IsAutoclickEnabledOnDetailMenu() const {
    ash::mojom::SystemTrayTestApiAsyncWaiter wait_for(tray_test_api_.get());
    bool visible = false;
    wait_for.IsBubbleViewVisible(ash::VIEW_ID_ACCESSIBILITY_AUTOCLICK_ENABLED,
                                 false /* open_tray */, &visible);
    return visible;
  }

  // Disable animations so that tray icons hide immediately.
  ui::ScopedAnimationDurationScaleMode disable_animations_;

  policy::MockConfigurationPolicyProvider provider_;
  ash::mojom::SystemTrayTestApiPtr tray_test_api_;
};

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, ShowMenu) {
  SetShowAccessibilityOptionsInSystemTrayMenu(false);

  // Confirms that the menu is hidden.
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling spoken feedback changes the visibility of the menu.
  EnableSpokenFeedback(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSpokenFeedback(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling high contrast changes the visibility of the menu.
  EnableHighContrast(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableHighContrast(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling screen magnifier changes the visibility of the menu.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetMagnifierEnabled(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling autoclick changes the visibility of the menu.
  EnableAutoclick(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableAutoclick(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling virtual keyboard changes the visibility of the menu.
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableVirtualKeyboard(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling large mouse cursor changes the visibility of the menu.
  EnableLargeCursor(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLargeCursor(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling mono audio changes the visibility of the menu.
  EnableMonoAudio(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableMonoAudio(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling caret highlight changes the visibility of the menu.
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCaretHighlightEnabled(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling highlight mouse cursor changes the visibility of the menu.
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCursorHighlightEnabled(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling highlight keyboard focus changes the visibility of the menu.
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetFocusHighlightEnabled(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling sticky keys changes the visibility of the menu.
  EnableStickyKeys(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableStickyKeys(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Toggling select-to-speak dragging changes the visibility of the menu.
  EnableSelectToSpeak(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSelectToSpeak(false);
  EXPECT_FALSE(IsMenuButtonVisible());

  // Enabling all accessibility features.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableHighContrast(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSpokenFeedback(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSelectToSpeak(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableAutoclick(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLargeCursor(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableMonoAudio(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableStickyKeys(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableAutoclick(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSpokenFeedback(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSelectToSpeak(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableHighContrast(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLargeCursor(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableMonoAudio(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableStickyKeys(false);
  EXPECT_FALSE(IsMenuButtonVisible());
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, ShowMenuWithShowMenuOption) {
  SetShowAccessibilityOptionsInSystemTrayMenu(true);

  // Confirms that the menu is visible.
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling spoken feedback.
  EnableSpokenFeedback(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSpokenFeedback(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling high contrast.
  EnableHighContrast(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableHighContrast(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling screen magnifier.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling autoclick.
  EnableAutoclick(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableAutoclick(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling on-screen keyboard.
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling large mouse cursor.
  EnableLargeCursor(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLargeCursor(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling mono audio.
  EnableMonoAudio(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableMonoAudio(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling caret highlight.
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling highlight mouse cursor.
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling highlight keyboard focus.
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of the toggling sticky keys.
  EnableStickyKeys(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableStickyKeys(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling select-to-speak.
  EnableSelectToSpeak(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSelectToSpeak(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // Enabling all accessibility features.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableHighContrast(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSpokenFeedback(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSelectToSpeak(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableAutoclick(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLargeCursor(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableMonoAudio(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableStickyKeys(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableAutoclick(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSpokenFeedback(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSelectToSpeak(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableHighContrast(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLargeCursor(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableMonoAudio(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableStickyKeys(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  SetShowAccessibilityOptionsInSystemTrayMenu(false);

  // Confirms that the menu is invisible.
  EXPECT_FALSE(IsMenuButtonVisible());
}

IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, KeepMenuVisibilityOnLockScreen) {
  // Enables high contrast mode.
  EnableHighContrast(true);
  EXPECT_TRUE(IsMenuButtonVisible());

  // Locks the screen.
  SessionControllerClient::Get()->RequestLockScreen();
  // Resets binding because UnifiedSystemTray is recreated.
  BindTestApi();
  EXPECT_TRUE(IsMenuButtonVisible());

  // Disables high contrast mode.
  EnableHighContrast(false);

  // Confirms that the menu is still visible.
  EXPECT_TRUE(IsMenuButtonVisible());
}

// Verify that the accessiblity system detailed menu remains open when an item
// is selected or deselected.
IN_PROC_BROWSER_TEST_P(TrayAccessibilityTest, DetailMenuRemainsOpen) {
  CreateDetailedMenu();

  ClickAutoclickOnDetailMenu();
  EXPECT_TRUE(IsAutoclickEnabledOnDetailMenu());
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_TRUE(IsBubbleOpen());

  ClickAutoclickOnDetailMenu();
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_TRUE(IsBubbleOpen());
}

class TrayAccessibilityLoginTest : public TrayAccessibilityTest {
 public:
  TrayAccessibilityLoginTest() = default;
  ~TrayAccessibilityLoginTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    TrayAccessibilityTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TrayAccessibilityLoginTest);
};

IN_PROC_BROWSER_TEST_P(TrayAccessibilityLoginTest,
                       ShowMenuWithShowOnLoginScreen) {
  EXPECT_FALSE(user_manager::UserManager::Get()->IsUserLoggedIn());

  // Confirms that the menu is visible.
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling spoken feedback.
  EnableSpokenFeedback(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSpokenFeedback(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling high contrast.
  EnableHighContrast(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableHighContrast(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling screen magnifier.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling on-screen keyboard.
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling large mouse cursor.
  EnableLargeCursor(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLargeCursor(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling mono audio.
  EnableMonoAudio(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableMonoAudio(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling caret highlight.
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling highlight mouse cursor.
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling highlight keyboard focus.
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // The menu remains visible regardless of toggling sticky keys.
  EnableStickyKeys(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableStickyKeys(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  // Enabling all accessibility features.
  SetMagnifierEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableHighContrast(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSpokenFeedback(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableVirtualKeyboard(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLargeCursor(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableMonoAudio(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCaretHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCursorHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetFocusHighlightEnabled(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableStickyKeys(true);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableVirtualKeyboard(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableSpokenFeedback(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableHighContrast(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetMagnifierEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableLargeCursor(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableMonoAudio(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCaretHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetCursorHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  SetFocusHighlightEnabled(false);
  EXPECT_TRUE(IsMenuButtonVisible());
  EnableStickyKeys(false);
  EXPECT_TRUE(IsMenuButtonVisible());

  SetShowAccessibilityOptionsInSystemTrayMenu(true);

  // Confirms that the menu remains visible.
  EXPECT_TRUE(IsMenuButtonVisible());

  SetShowAccessibilityOptionsInSystemTrayMenu(false);

  // Confirms that the menu remains visible.
  EXPECT_TRUE(IsMenuButtonVisible());
}

INSTANTIATE_TEST_CASE_P(TrayAccessibilityTestInstance,
                        TrayAccessibilityTest,
                        testing::Values(PREF_SERVICE,
                                        POLICY));
INSTANTIATE_TEST_CASE_P(TrayAccessibilityLoginTestInstance,
                        TrayAccessibilityLoginTest,
                        testing::Values(PREF_SERVICE, POLICY));

}  // namespace chromeos
