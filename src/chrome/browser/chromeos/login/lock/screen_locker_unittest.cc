// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/lock/screen_locker.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/test_accessibility_focus_ring_controller.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager_impl.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "chrome/browser/ui/ash/accessibility/fake_accessibility_controller.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "chrome/browser/ui/ash/test_login_screen.h"
#include "chrome/browser/ui/ash/test_session_controller.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_service_manager_context.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "media/audio/audio_manager.h"
#include "media/audio/sounds/sounds_manager.h"
#include "media/audio/test_audio_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class ScreenLockerUnitTest : public testing::Test {
 public:
  ScreenLockerUnitTest() = default;
  ~ScreenLockerUnitTest() override = default;

  void SetUp() override {
    DBusThreadManager::Initialize();

    // MojoSystemInfoDispatcher dependency:
    bluez::BluezDBusManager::GetSetterForTesting();

    // Initialize SessionControllerClient and dependencies:
    chromeos::LoginState::Initialize();
    CHECK(testing_profile_manager_.SetUp());
    session_controller_client_ = std::make_unique<SessionControllerClient>();
    session_controller_client_->Init();

    // Initialize AccessibilityManager and dependencies:
    media::SoundsManager::Create();
    input_method::InputMethodManager::Initialize(
        // Owned by InputMethodManager
        new input_method::MockInputMethodManagerImpl());
    CrasAudioHandler::InitializeForTesting();
    chromeos::AccessibilityManager::Initialize();

    // Initialize ScreenLocker dependencies:
    SystemSaltGetter::Initialize();
    const AccountId account_id =
        AccountId::FromUserEmail("testemail@example.com");
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);
    CHECK(user_manager::UserManager::Get()->GetPrimaryUser());
    session_manager::SessionManager::Get()->CreateSession(
        account_id, account_id.GetUserEmail(), false);
  }

  void TearDown() override {
    input_method::InputMethodManager::Shutdown();
    media::SoundsManager::Shutdown();
    media::AudioManager::Get()->Shutdown();
    session_controller_client_.reset();
    chromeos::LoginState::Shutdown();
  }

 protected:
  // Needed for main loop and posting async tasks.
  content::TestBrowserThreadBundle thread_bundle_;

  // Needed to set up Service Manager and create mojo fakes.
  content::TestServiceManagerContext context_;

  // ViewsScreenLocker dependencies:
  lock_screen_apps::StateController state_controller_;
  // * MojoSystemInfoDispatcher dependencies:
  ScopedTestingCrosSettings scoped_testing_cros_settings_;
  system::ScopedFakeStatisticsProvider fake_statictics_provider_;
  // * ChromeUserSelectionScreen dependencies:
  chromeos::ScopedStubInstallAttributes test_install_attributes_;

  // ScreenLocker dependencies:
  // * AccessibilityManager dependencies:
  FakeAccessibilityController fake_accessibility_controller_;
  TestAccessibilityFocusRingController
      test_accessibility_focus_ring_controller_;
  std::unique_ptr<media::AudioManager> audio_manager_{
      media::AudioManager::CreateForTesting(
          std::make_unique<media::TestAudioThread>())};
  // * LoginScreenClient dependencies:
  session_manager::SessionManager session_manager_;
  TestLoginScreen test_login_screen_;
  LoginScreenClient login_screen_client_;
  // * SessionControllerClient dependencies:
  FakeChromeUserManager* fake_user_manager_{new FakeChromeUserManager()};
  user_manager::ScopedUserManager scoped_user_manager_{
      base::WrapUnique(fake_user_manager_)};
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  ScopedDeviceSettingsTestHelper device_settings_test_helper_;
  TestSessionController test_session_controller_;
  std::unique_ptr<SessionControllerClient> session_controller_client_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockerUnitTest);
};

// Chrome notifies Ash when screen is locked. Ash is responsible for suspending
// the device.
TEST_F(ScreenLockerUnitTest, VerifyAshIsNotifiedOfScreenLocked) {
  EXPECT_EQ(0, test_session_controller_.lock_animation_complete_call_count());
  ScreenLocker::Show();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, test_session_controller_.lock_animation_complete_call_count());
  ScreenLocker::Hide();
  // Needed to perform internal cleanup scheduled in ScreenLocker::Hide()
  base::RunLoop().RunUntilIdle();
}

}  // namespace chromeos
