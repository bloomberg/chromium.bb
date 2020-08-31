// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <sys/types.h>

#include "ash/public/cpp/ash_pref_names.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/input_method_manager_impl.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/chromeos/system/fake_input_device_settings.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/feedback/tracing_manager.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/fake_ime_keyboard.h"
#include "ui/events/event_utils.h"

namespace chromeos {

class PreferencesTest : public LoginManagerTest {
 public:
  PreferencesTest()
      : LoginManagerTest(), input_settings_(nullptr), keyboard_(nullptr) {
    login_mixin_.AppendRegularUsers(2);
    scoped_testing_cros_settings_.device_settings()->Set(
        kDeviceOwner,
        base::Value(login_mixin_.users()[0].account_id.GetUserEmail()));

    feature_list_.InitAndEnableFeature(features::kAllowScrollSettings);
  }

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    input_settings_ = system::InputDeviceSettings::Get()->GetFakeInterface();
    EXPECT_NE(nullptr, input_settings_);
    keyboard_ = new input_method::FakeImeKeyboard();
    static_cast<input_method::InputMethodManagerImpl*>(
        input_method::InputMethodManager::Get())
        ->SetImeKeyboardForTesting(keyboard_);
  }

  // Sets set of preferences in given |prefs|. Value of prefernece depends of
  // |variant| value. For opposite |variant| values all preferences receive
  // different values.
  void SetPrefs(PrefService* prefs, bool variant) {
    prefs->SetBoolean(ash::prefs::kMouseReverseScroll, variant);
    prefs->SetBoolean(ash::prefs::kNaturalScroll, variant);
    prefs->SetBoolean(prefs::kTapToClickEnabled, variant);
    prefs->SetBoolean(prefs::kPrimaryMouseButtonRight, !variant);
    prefs->SetBoolean(prefs::kMouseAcceleration, variant);
    prefs->SetBoolean(prefs::kMouseScrollAcceleration, variant);
    prefs->SetBoolean(prefs::kTouchpadAcceleration, variant);
    prefs->SetBoolean(prefs::kTouchpadScrollAcceleration, variant);
    prefs->SetBoolean(prefs::kEnableTouchpadThreeFingerClick, !variant);
    prefs->SetInteger(prefs::kMouseSensitivity, !variant);
    prefs->SetInteger(prefs::kMouseScrollSensitivity, variant ? 1 : 4);
    prefs->SetInteger(prefs::kTouchpadSensitivity, variant);
    prefs->SetInteger(prefs::kTouchpadScrollSensitivity, variant ? 1 : 4);
    prefs->SetBoolean(ash::prefs::kXkbAutoRepeatEnabled, variant);
    prefs->SetInteger(ash::prefs::kXkbAutoRepeatDelay, variant ? 100 : 500);
    prefs->SetInteger(ash::prefs::kXkbAutoRepeatInterval, variant ? 1 : 4);
    prefs->SetString(prefs::kLanguagePreloadEngines,
                     variant ? "xkb:us::eng,xkb:us:dvorak:eng"
                             : "xkb:us::eng,xkb:ru::rus");
  }

  void CheckSettingsCorrespondToPrefs(PrefService* prefs) {
    EXPECT_EQ(prefs->GetBoolean(prefs::kTapToClickEnabled),
              input_settings_->current_touchpad_settings().GetTapToClick());
    EXPECT_EQ(prefs->GetBoolean(prefs::kPrimaryMouseButtonRight),
              input_settings_->current_mouse_settings()
                  .GetPrimaryButtonRight());
    EXPECT_EQ(prefs->GetBoolean(ash::prefs::kMouseReverseScroll),
              input_settings_->current_mouse_settings().GetReverseScroll());
    EXPECT_EQ(prefs->GetBoolean(prefs::kMouseAcceleration),
              input_settings_->current_mouse_settings().GetAcceleration());
    EXPECT_EQ(
        prefs->GetBoolean(prefs::kMouseScrollAcceleration),
        input_settings_->current_mouse_settings().GetScrollAcceleration());
    EXPECT_EQ(prefs->GetBoolean(prefs::kTouchpadAcceleration),
              input_settings_->current_touchpad_settings().GetAcceleration());
    EXPECT_EQ(
        prefs->GetBoolean(prefs::kTouchpadScrollAcceleration),
        input_settings_->current_touchpad_settings().GetScrollAcceleration());
    EXPECT_EQ(prefs->GetBoolean(prefs::kEnableTouchpadThreeFingerClick),
              input_settings_->current_touchpad_settings()
                  .GetThreeFingerClick());
    EXPECT_EQ(prefs->GetInteger(prefs::kMouseSensitivity),
              input_settings_->current_mouse_settings().GetSensitivity());
    EXPECT_EQ(prefs->GetInteger(prefs::kMouseScrollSensitivity),
              input_settings_->current_mouse_settings().GetScrollSensitivity());
    EXPECT_EQ(prefs->GetInteger(prefs::kTouchpadSensitivity),
              input_settings_->current_touchpad_settings().GetSensitivity());
    EXPECT_EQ(
        prefs->GetInteger(prefs::kTouchpadScrollSensitivity),
        input_settings_->current_touchpad_settings().GetScrollSensitivity());
    EXPECT_EQ(prefs->GetBoolean(ash::prefs::kXkbAutoRepeatEnabled),
              keyboard_->auto_repeat_is_enabled_);
    input_method::AutoRepeatRate rate = keyboard_->last_auto_repeat_rate_;
    EXPECT_EQ(prefs->GetInteger(ash::prefs::kXkbAutoRepeatDelay),
              (int)rate.initial_delay_in_ms);
    EXPECT_EQ(prefs->GetInteger(ash::prefs::kXkbAutoRepeatInterval),
              (int)rate.repeat_interval_in_ms);
    EXPECT_EQ(prefs->GetString(prefs::kLanguageCurrentInputMethod),
              input_method::InputMethodManager::Get()
                  ->GetActiveIMEState()
                  ->GetCurrentInputMethod()
                  .id());
  }

  void CheckLocalStateCorrespondsToPrefs(PrefService* prefs) {
    PrefService* local_state = g_browser_process->local_state();
    EXPECT_EQ(local_state->GetBoolean(prefs::kOwnerTapToClickEnabled),
              prefs->GetBoolean(prefs::kTapToClickEnabled));
    EXPECT_EQ(local_state->GetBoolean(prefs::kOwnerPrimaryMouseButtonRight),
              prefs->GetBoolean(prefs::kPrimaryMouseButtonRight));
  }

  LoginManagerMixin login_mixin_{&mixin_host_};
  ScopedTestingCrosSettings scoped_testing_cros_settings_;

 private:
  base::test::ScopedFeatureList feature_list_;
  system::InputDeviceSettings::FakeInterface* input_settings_;
  input_method::FakeImeKeyboard* keyboard_;

  DISALLOW_COPY_AND_ASSIGN(PreferencesTest);
};

IN_PROC_BROWSER_TEST_F(PreferencesTest, MultiProfiles) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  const auto& users = login_mixin_.users();
  // Add first user and init its preferences. Check that corresponding
  // settings has been changed.
  LoginUser(users[0].account_id);
  const user_manager::User* user1 = user_manager->FindUser(users[0].account_id);
  PrefService* prefs1 =
      ProfileHelper::Get()->GetProfileByUserUnsafe(user1)->GetPrefs();
  SetPrefs(prefs1, false);
  content::RunAllPendingInMessageLoop();
  CheckSettingsCorrespondToPrefs(prefs1);

  // Add second user and init its prefs with different values.
  UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  AddUser(users[1].account_id);
  content::RunAllPendingInMessageLoop();
  const user_manager::User* user2 = user_manager->FindUser(users[1].account_id);
  EXPECT_TRUE(user2->is_active());
  PrefService* prefs2 =
      ProfileHelper::Get()->GetProfileByUserUnsafe(user2)->GetPrefs();
  SetPrefs(prefs2, true);

  // Check that settings were changed accordingly.
  EXPECT_TRUE(user2->is_active());
  CheckSettingsCorrespondToPrefs(prefs2);

  // Check that changing prefs of the active user doesn't affect prefs of the
  // inactive user.
  std::unique_ptr<base::DictionaryValue> prefs_backup =
      prefs1->GetPreferenceValues(PrefService::INCLUDE_DEFAULTS);
  SetPrefs(prefs2, false);
  CheckSettingsCorrespondToPrefs(prefs2);
  EXPECT_TRUE(prefs_backup->Equals(
      prefs1->GetPreferenceValues(PrefService::INCLUDE_DEFAULTS).get()));
  SetPrefs(prefs2, true);
  CheckSettingsCorrespondToPrefs(prefs2);
  EXPECT_TRUE(prefs_backup->Equals(
      prefs1->GetPreferenceValues(PrefService::INCLUDE_DEFAULTS).get()));

  // Check that changing prefs of the inactive user doesn't affect prefs of the
  // active user.
  prefs_backup = prefs2->GetPreferenceValues(PrefService::INCLUDE_DEFAULTS);
  SetPrefs(prefs1, true);
  CheckSettingsCorrespondToPrefs(prefs2);
  EXPECT_TRUE(prefs_backup->Equals(
      prefs2->GetPreferenceValues(PrefService::INCLUDE_DEFAULTS).get()));
  SetPrefs(prefs1, false);
  CheckSettingsCorrespondToPrefs(prefs2);
  EXPECT_TRUE(prefs_backup->Equals(
      prefs2->GetPreferenceValues(PrefService::INCLUDE_DEFAULTS).get()));

  // Check that changing non-owner prefs doesn't change corresponding local
  // state prefs and vice versa.
  EXPECT_EQ(user_manager->GetOwnerAccountId(), users[0].account_id);
  CheckLocalStateCorrespondsToPrefs(prefs1);
  prefs2->SetBoolean(prefs::kTapToClickEnabled,
                     !prefs1->GetBoolean(prefs::kTapToClickEnabled));
  CheckLocalStateCorrespondsToPrefs(prefs1);
  prefs1->SetBoolean(prefs::kTapToClickEnabled,
                     !prefs1->GetBoolean(prefs::kTapToClickEnabled));
  CheckLocalStateCorrespondsToPrefs(prefs1);

  // Switch user back.
  user_manager->SwitchActiveUser(users[0].account_id);
  CheckSettingsCorrespondToPrefs(prefs1);
  CheckLocalStateCorrespondsToPrefs(prefs1);
}

}  // namespace chromeos
