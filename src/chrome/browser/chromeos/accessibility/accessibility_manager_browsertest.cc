// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/api/braille_display_private/mock_braille_controller.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/ui/ash/ksv/keyboard_shortcut_viewer_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

using chromeos::input_method::InputMethodManager;
using chromeos::input_method::InputMethodUtil;
using chromeos::input_method::InputMethodDescriptors;
using content::BrowserThread;
using extensions::api::braille_display_private::BrailleObserver;
using extensions::api::braille_display_private::DisplayState;
using extensions::api::braille_display_private::KeyEvent;
using extensions::api::braille_display_private::MockBrailleController;
using testing::WithParamInterface;

namespace chromeos {

namespace {

// Use a real domain to avoid policy loading problems.
constexpr char kTestUserName[] = "owner@gmail.com";
constexpr char kTestUserGaiaId[] = "9876543210";

constexpr int kTestAutoclickDelayMs = 2000;

// Test user name for supervised user. The domain part must be matched with
// user_manager::kSupervisedUserDomain.
constexpr char kTestSupervisedUserName[] = "test@locally-managed.localhost";

class MockAccessibilityObserver {
 public:
  MockAccessibilityObserver() {
    AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
    CHECK(accessibility_manager);
    accessibility_subscription_ = accessibility_manager->RegisterCallback(
        base::Bind(&MockAccessibilityObserver::OnAccessibilityStatusChanged,
                   base::Unretained(this)));
  }

  virtual ~MockAccessibilityObserver() = default;

  bool observed() const { return observed_; }
  bool observed_enabled() const { return observed_enabled_; }
  int observed_type() const { return observed_type_; }

  void reset() { observed_ = false; }

 private:
  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details) {
    if (details.notification_type != ACCESSIBILITY_TOGGLE_SCREEN_MAGNIFIER) {
      observed_type_ = details.notification_type;
      observed_enabled_ = details.enabled;
      observed_ = true;
    }
  }

  bool observed_ = false;
  bool observed_enabled_ = false;
  int observed_type_ = -1;

  std::unique_ptr<AccessibilityStatusSubscription> accessibility_subscription_;

  DISALLOW_COPY_AND_ASSIGN(MockAccessibilityObserver);
};

Profile* GetActiveUserProfile() {
  return ProfileManager::GetActiveUserProfile();
}

PrefService* GetActiveUserPrefs() {
  return GetActiveUserProfile()->GetPrefs();
}

void SetLargeCursorEnabled(bool enabled) {
  AccessibilityManager::Get()->EnableLargeCursor(enabled);
}

bool IsLargeCursorEnabled() {
  return AccessibilityManager::Get()->IsLargeCursorEnabled();
}

bool ShouldShowAccessibilityMenu() {
  return AccessibilityManager::Get()->ShouldShowAccessibilityMenu();
}

void SetHighContrastEnabled(bool enabled) {
  AccessibilityManager::Get()->EnableHighContrast(enabled);
}

bool IsHighContrastEnabled() {
  return AccessibilityManager::Get()->IsHighContrastEnabled();
}

void SetSpokenFeedbackEnabled(bool enabled) {
  AccessibilityManager::Get()->EnableSpokenFeedback(enabled);
}

bool IsSpokenFeedbackEnabled() {
  return AccessibilityManager::Get()->IsSpokenFeedbackEnabled();
}

void SetAutoclickEnabled(bool enabled) {
  AccessibilityManager::Get()->EnableAutoclick(enabled);
}

bool IsAutoclickEnabled() {
  return AccessibilityManager::Get()->IsAutoclickEnabled();
}

void SetAutoclickDelay(int delay_ms) {
  GetActiveUserPrefs()->SetInteger(ash::prefs::kAccessibilityAutoclickDelayMs,
                                   delay_ms);
  GetActiveUserPrefs()->CommitPendingWrite();
}

int GetAutoclickDelay() {
  return GetActiveUserPrefs()->GetInteger(
      ash::prefs::kAccessibilityAutoclickDelayMs);
}

void SetVirtualKeyboardEnabled(bool enabled) {
  AccessibilityManager::Get()->EnableVirtualKeyboard(enabled);
}

bool IsVirtualKeyboardEnabled() {
  return AccessibilityManager::Get()->IsVirtualKeyboardEnabled();
}

void SetMonoAudioEnabled(bool enabled) {
  AccessibilityManager::Get()->EnableMonoAudio(enabled);
}

bool IsMonoAudioEnabled() {
  return AccessibilityManager::Get()->IsMonoAudioEnabled();
}

void SetSelectToSpeakEnabled(bool enabled) {
  AccessibilityManager::Get()->SetSelectToSpeakEnabled(enabled);
}

bool IsSelectToSpeakEnabled() {
  return AccessibilityManager::Get()->IsSelectToSpeakEnabled();
}

void SetAlwaysShowMenuEnabledPref(bool enabled) {
  GetActiveUserPrefs()->SetBoolean(
      ash::prefs::kShouldAlwaysShowAccessibilityMenu, enabled);
}

void SetLargeCursorEnabledPref(bool enabled) {
  GetActiveUserPrefs()->SetBoolean(ash::prefs::kAccessibilityLargeCursorEnabled,
                                   enabled);
}

void SetHighContrastEnabledPref(bool enabled) {
  GetActiveUserPrefs()->SetBoolean(
      ash::prefs::kAccessibilityHighContrastEnabled, enabled);
}

void SetSpokenFeedbackEnabledPref(bool enabled) {
  GetActiveUserPrefs()->SetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled, enabled);
}

void SetAutoclickEnabledPref(bool enabled) {
  GetActiveUserPrefs()->SetBoolean(ash::prefs::kAccessibilityAutoclickEnabled,
                                   enabled);
}

void SetAutoclickDelayPref(int delay_ms) {
  GetActiveUserPrefs()->SetInteger(ash::prefs::kAccessibilityAutoclickDelayMs,
                                   delay_ms);
}

void SetVirtualKeyboardEnabledPref(bool enabled) {
  GetActiveUserPrefs()->SetBoolean(
      ash::prefs::kAccessibilityVirtualKeyboardEnabled, enabled);
}

void SetMonoAudioEnabledPref(bool enabled) {
  GetActiveUserPrefs()->SetBoolean(ash::prefs::kAccessibilityMonoAudioEnabled,
                                   enabled);
}

void SetSelectToSpeakEnabledPref(bool enabled) {
  GetActiveUserPrefs()->SetBoolean(
      ash::prefs::kAccessibilitySelectToSpeakEnabled, enabled);
}

bool IsBrailleImeActive() {
  InputMethodManager* imm = InputMethodManager::Get();
  std::unique_ptr<InputMethodDescriptors> descriptors =
      imm->GetActiveIMEState()->GetActiveInputMethods();
  for (const auto& descriptor : *descriptors) {
    if (descriptor.id() == extension_ime_util::kBrailleImeEngineId)
      return true;
  }
  return false;
}

bool IsBrailleImeCurrent() {
  InputMethodManager* imm = InputMethodManager::Get();
  return imm->GetActiveIMEState()->GetCurrentInputMethod().id() ==
         extension_ime_util::kBrailleImeEngineId;
}

}  // namespace

// For user session accessibility manager tests.
class AccessibilityManagerTest : public InProcessBrowserTest {
 protected:
  AccessibilityManagerTest()
      : disable_animations_(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {}
  ~AccessibilityManagerTest() override = default;

  void SetUpOnMainThread() override {
    default_autoclick_delay_ = GetAutoclickDelay();
  }

  int default_autoclick_delay() const { return default_autoclick_delay_; }

  int default_autoclick_delay_ = 0;

  const AccountId test_account_id_ =
      AccountId::FromUserEmailGaiaId(kTestUserName, kTestUserGaiaId);

 private:
  ui::ScopedAnimationDurationScaleMode disable_animations_;
  DISALLOW_COPY_AND_ASSIGN(AccessibilityManagerTest);
};

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest, TypePref) {
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());
  EXPECT_EQ(default_autoclick_delay_, GetAutoclickDelay());
  EXPECT_FALSE(IsVirtualKeyboardEnabled());
  EXPECT_FALSE(IsMonoAudioEnabled());
  EXPECT_FALSE(IsSelectToSpeakEnabled());

  SetLargeCursorEnabledPref(true);
  EXPECT_TRUE(IsLargeCursorEnabled());

  SetSpokenFeedbackEnabledPref(true);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());

  SetHighContrastEnabledPref(true);
  EXPECT_TRUE(IsHighContrastEnabled());

  SetAutoclickEnabledPref(true);
  EXPECT_TRUE(IsAutoclickEnabled());

  SetAutoclickDelayPref(kTestAutoclickDelayMs);
  EXPECT_EQ(kTestAutoclickDelayMs, GetAutoclickDelay());

  SetVirtualKeyboardEnabledPref(true);
  EXPECT_TRUE(IsVirtualKeyboardEnabled());

  SetMonoAudioEnabledPref(true);
  EXPECT_TRUE(IsMonoAudioEnabled());

  SetSelectToSpeakEnabledPref(true);
  EXPECT_TRUE(IsSelectToSpeakEnabled());

  SetLargeCursorEnabledPref(false);
  EXPECT_FALSE(IsLargeCursorEnabled());

  SetSpokenFeedbackEnabledPref(false);
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  SetHighContrastEnabledPref(false);
  EXPECT_FALSE(IsHighContrastEnabled());

  SetAutoclickEnabledPref(false);
  EXPECT_FALSE(IsAutoclickEnabled());

  SetVirtualKeyboardEnabledPref(false);
  EXPECT_FALSE(IsVirtualKeyboardEnabled());

  SetMonoAudioEnabledPref(false);
  EXPECT_FALSE(IsMonoAudioEnabled());

  SetSelectToSpeakEnabledPref(false);
  EXPECT_FALSE(IsSelectToSpeakEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest,
                       ChangingTypeInvokesNotification) {
  MockAccessibilityObserver observer;

  SetSpokenFeedbackEnabled(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());

  observer.reset();
  SetSpokenFeedbackEnabled(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK);
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  observer.reset();
  SetHighContrastEnabled(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE);
  EXPECT_TRUE(IsHighContrastEnabled());

  observer.reset();
  SetHighContrastEnabled(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE);
  EXPECT_FALSE(IsHighContrastEnabled());

  observer.reset();
  SetVirtualKeyboardEnabled(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_VIRTUAL_KEYBOARD);
  EXPECT_TRUE(IsVirtualKeyboardEnabled());

  observer.reset();
  SetVirtualKeyboardEnabled(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_VIRTUAL_KEYBOARD);
  EXPECT_FALSE(IsVirtualKeyboardEnabled());

  observer.reset();
  SetMonoAudioEnabled(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_MONO_AUDIO);
  EXPECT_TRUE(IsMonoAudioEnabled());

  observer.reset();
  SetMonoAudioEnabled(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_MONO_AUDIO);
  EXPECT_FALSE(IsMonoAudioEnabled());

  observer.reset();
  SetSelectToSpeakEnabled(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_SELECT_TO_SPEAK);
  EXPECT_TRUE(IsSelectToSpeakEnabled());

  observer.reset();
  SetSelectToSpeakEnabled(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_SELECT_TO_SPEAK);
  EXPECT_FALSE(IsSelectToSpeakEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest,
                       ChangingTypePrefInvokesNotification) {
  MockAccessibilityObserver observer;

  SetSpokenFeedbackEnabledPref(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());

  observer.reset();
  SetSpokenFeedbackEnabledPref(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK);
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  observer.reset();
  SetHighContrastEnabledPref(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE);
  EXPECT_TRUE(IsHighContrastEnabled());

  observer.reset();
  SetHighContrastEnabledPref(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE);
  EXPECT_FALSE(IsHighContrastEnabled());

  observer.reset();
  SetVirtualKeyboardEnabledPref(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_VIRTUAL_KEYBOARD);
  EXPECT_TRUE(IsVirtualKeyboardEnabled());

  observer.reset();
  SetVirtualKeyboardEnabledPref(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_VIRTUAL_KEYBOARD);
  EXPECT_FALSE(IsVirtualKeyboardEnabled());

  observer.reset();
  SetMonoAudioEnabledPref(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_MONO_AUDIO);
  EXPECT_TRUE(IsMonoAudioEnabled());

  observer.reset();
  SetMonoAudioEnabledPref(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_MONO_AUDIO);
  EXPECT_FALSE(IsMonoAudioEnabled());

  observer.reset();
  SetSelectToSpeakEnabledPref(true);
  EXPECT_TRUE(observer.observed());
  EXPECT_TRUE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_SELECT_TO_SPEAK);
  EXPECT_TRUE(IsSelectToSpeakEnabled());

  observer.reset();
  SetSelectToSpeakEnabledPref(false);
  EXPECT_TRUE(observer.observed());
  EXPECT_FALSE(observer.observed_enabled());
  EXPECT_EQ(observer.observed_type(), ACCESSIBILITY_TOGGLE_SELECT_TO_SPEAK);
  EXPECT_FALSE(IsSelectToSpeakEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest, AccessibilityMenuVisibility) {
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());
  EXPECT_FALSE(ShouldShowAccessibilityMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabled());
  EXPECT_FALSE(IsMonoAudioEnabled());

  EXPECT_FALSE(ShouldShowAccessibilityMenu());
  SetAlwaysShowMenuEnabledPref(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetAlwaysShowMenuEnabledPref(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  SetLargeCursorEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetLargeCursorEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  SetSpokenFeedbackEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetSpokenFeedbackEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  SetHighContrastEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetHighContrastEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  SetAutoclickEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetAutoclickEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  SetVirtualKeyboardEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetVirtualKeyboardEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  SetMonoAudioEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetMonoAudioEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());

  SetSelectToSpeakEnabled(true);
  EXPECT_TRUE(ShouldShowAccessibilityMenu());
  SetSelectToSpeakEnabled(false);
  EXPECT_FALSE(ShouldShowAccessibilityMenu());
}

// Tests text caret highlighting for remote mojo applications (e.g. shortcut
// viewer). This test integration of AccessibilityManager with the IME driver.
IN_PROC_BROWSER_TEST_F(AccessibilityManagerTest, CaretHighlightInRemoteApp) {
  AccessibilityManager::Get()->SetCaretHighlightEnabled(true);

  // App launch is asynchronous so we will wait for a non-empty caret bounds
  // update.
  auto wait_for_bounds = [](base::RunLoop* run_loop, const gfx::Rect& bounds) {
    // Under mash the first bounds update we see might be from the ash process
    // clearing its caret highlight. Wait for the app's highlight to show up.
    if (!bounds.IsEmpty())
      run_loop->Quit();
  };
  base::RunLoop run_loop;
  AccessibilityManager::Get()->SetCaretBoundsObserverForTest(
      base::BindRepeating(wait_for_bounds, &run_loop));

  // Focus will move to the search field and show a text caret highlight.
  keyboard_shortcut_viewer_util::ToggleKeyboardShortcutViewer();

  // Wait for the app to launch, the IME session to start and the caret bounds
  // to be set.
  run_loop.Run();

  // Browser tests spin the message loop during shutdown which can lead to
  // additional bounds updates.
  AccessibilityManager::Get()->SetCaretBoundsObserverForTest(base::DoNothing());
}

// For signin screen to user session accessibility manager tests.
class AccessibilityManagerLoginTest : public OobeBaseTest {
 protected:
  AccessibilityManagerLoginTest()
      : disable_animations_(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {}
  ~AccessibilityManagerLoginTest() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    AccessibilityManager::SetBrailleControllerForTest(&braille_controller_);
    default_autoclick_delay_ = GetAutoclickDelay();
  }

  void TearDownOnMainThread() override {
    AccessibilityManager::SetBrailleControllerForTest(nullptr);
    OobeBaseTest::TearDownOnMainThread();
  }

  void CreateSession(const AccountId& account_id) {
    auto* session_manager = session_manager::SessionManager::Get();
    session_manager->CreateSession(account_id, account_id.GetUserEmail(),
                                   false);
  }

  void StartUserSession(const AccountId& account_id) {
    ProfileHelper::GetProfileByUserIdHashForTest(
        user_manager::UserManager::Get()
            ->FindUser(account_id)
            ->username_hash());
    session_manager::SessionManager::Get()->SessionStarted();
  }

  void SetBrailleDisplayAvailability(bool available) {
    braille_controller_.SetAvailable(available);
    braille_controller_.GetObserver()->OnBrailleDisplayStateChanged(
        *braille_controller_.GetDisplayState());
    AccessibilityManager::Get()->FlushForTesting();
  }

  int default_autoclick_delay_ = 0;

  MockBrailleController braille_controller_;

  const AccountId test_account_id_ =
      AccountId::FromUserEmailGaiaId(kTestUserName, kTestUserGaiaId);

 private:
  ui::ScopedAnimationDurationScaleMode disable_animations_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityManagerLoginTest);
};

IN_PROC_BROWSER_TEST_F(AccessibilityManagerLoginTest, BrailleOnLoginScreen) {
  WaitForSigninScreen();
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  // Signal the accessibility manager that a braille display was connected.
  SetBrailleDisplayAvailability(true);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
}

#if defined(OS_CHROMEOS)
#define MAYBE_Login DISABLED_Login
#else
#define MAYBE_Login Login
#endif
IN_PROC_BROWSER_TEST_F(AccessibilityManagerLoginTest, MAYBE_Login) {
  WaitForSigninScreen();
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());
  EXPECT_FALSE(IsVirtualKeyboardEnabled());
  EXPECT_FALSE(IsMonoAudioEnabled());
  EXPECT_EQ(default_autoclick_delay_, GetAutoclickDelay());

  CreateSession(test_account_id_);

  // Confirms that the features are still disabled just after login.
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());
  EXPECT_FALSE(IsVirtualKeyboardEnabled());
  EXPECT_FALSE(IsMonoAudioEnabled());
  EXPECT_EQ(default_autoclick_delay_, GetAutoclickDelay());

  StartUserSession(test_account_id_);

  // Confirms that the features are still disabled after session starts.
  EXPECT_FALSE(IsLargeCursorEnabled());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsHighContrastEnabled());
  EXPECT_FALSE(IsAutoclickEnabled());
  EXPECT_FALSE(IsVirtualKeyboardEnabled());
  EXPECT_FALSE(IsMonoAudioEnabled());
  EXPECT_EQ(default_autoclick_delay_, GetAutoclickDelay());

  SetLargeCursorEnabled(true);
  EXPECT_TRUE(IsLargeCursorEnabled());

  SetSpokenFeedbackEnabled(true);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());

  SetHighContrastEnabled(true);
  EXPECT_TRUE(IsHighContrastEnabled());

  SetAutoclickEnabled(true);
  EXPECT_TRUE(IsAutoclickEnabled());

  SetAutoclickDelay(kTestAutoclickDelayMs);
  EXPECT_EQ(kTestAutoclickDelayMs, GetAutoclickDelay());

  SetVirtualKeyboardEnabled(true);
  EXPECT_TRUE(IsVirtualKeyboardEnabled());

  SetMonoAudioEnabled(true);
  EXPECT_TRUE(IsMonoAudioEnabled());
}

class AccessibilityManagerUserTypeTest : public AccessibilityManagerTest,
                                         public WithParamInterface<AccountId> {
 protected:
  AccessibilityManagerUserTypeTest() = default;
  virtual ~AccessibilityManagerUserTypeTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam() == user_manager::GuestAccountId()) {
      command_line->AppendSwitch(chromeos::switches::kGuestSession);
      command_line->AppendSwitch(::switches::kIncognito);
      command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                      "hash");
      command_line->AppendSwitchASCII(
          chromeos::switches::kLoginUser,
          user_manager::GuestAccountId().GetUserEmail());
    } else if (GetParam() ==
               AccountId::FromUserEmail(kTestSupervisedUserName)) {
      command_line->AppendSwitchASCII(::switches::kSupervisedUserId,
                                      supervised_users::kChildAccountSUID);
#if defined(OS_CHROMEOS)
      command_line->AppendSwitchASCII(
          chromeos::switches::kLoginUser,
          "supervised_user@locally-managed.localhost");
      command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                      "hash");
#endif
    }
    AccessibilityManager::SetBrailleControllerForTest(&braille_controller_);
  }

  void TearDownOnMainThread() override {
    AccessibilityManager::SetBrailleControllerForTest(nullptr);
    AccessibilityManagerTest::TearDownOnMainThread();
  }

  void SetBrailleDisplayAvailability(bool available) {
    braille_controller_.SetAvailable(available);
    braille_controller_.GetObserver()->OnBrailleDisplayStateChanged(
        *braille_controller_.GetDisplayState());
    AccessibilityManager::Get()->FlushForTesting();
  }

  MockBrailleController braille_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityManagerUserTypeTest);
};

// TODO(yoshiki): Enable a test for retail mode (i.e. RetailAccountId).
INSTANTIATE_TEST_SUITE_P(
    UserTypeInstantiation,
    AccessibilityManagerUserTypeTest,
    ::testing::Values(AccountId::FromUserEmailGaiaId(kTestUserName,
                                                     kTestUserGaiaId),
                      user_manager::GuestAccountId(),
                      AccountId::FromUserEmail(kTestSupervisedUserName)));

IN_PROC_BROWSER_TEST_P(AccessibilityManagerUserTypeTest, BrailleWhenLoggedIn) {
  // This object watches for IME preference changes and reflects those in
  // the IME framework state.
  chromeos::Preferences prefs;
  prefs.InitUserPrefsForTesting(
      PrefServiceSyncableFromProfile(GetActiveUserProfile()),
      user_manager::UserManager::Get()->GetActiveUser(),
      UserSessionManager::GetInstance()->GetDefaultIMEState(
          GetActiveUserProfile()));

  // Make sure we start in the expected state.
  EXPECT_FALSE(IsBrailleImeActive());
  EXPECT_FALSE(IsSpokenFeedbackEnabled());

  // Signal the accessibility manager that a braille display was connected.
  SetBrailleDisplayAvailability(true);

  // Now, both spoken feedback and the Braille IME should be enabled.
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
  EXPECT_TRUE(IsBrailleImeActive());

  // Send a braille dots key event and make sure that the braille IME is
  // enabled.
  KeyEvent event;
  event.command = extensions::api::braille_display_private::KEY_COMMAND_DOTS;
  event.braille_dots.reset(new int(0));
  braille_controller_.GetObserver()->OnBrailleKeyEvent(event);
  EXPECT_TRUE(IsBrailleImeCurrent());

  // Unplug the display.  Spolken feedback remains on, but the Braille IME
  // should get deactivated.
  SetBrailleDisplayAvailability(false);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
  EXPECT_FALSE(IsBrailleImeActive());
  EXPECT_FALSE(IsBrailleImeCurrent());

  // Plugging in a display while spoken feedback is enabled should activate
  // the Braille IME.
  SetBrailleDisplayAvailability(true);
  EXPECT_TRUE(IsSpokenFeedbackEnabled());
  EXPECT_TRUE(IsBrailleImeActive());
}

}  // namespace chromeos
