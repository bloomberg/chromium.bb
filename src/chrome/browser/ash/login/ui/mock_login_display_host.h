// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_MOCK_LOGIN_DISPLAY_HOST_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_MOCK_LOGIN_DISPLAY_HOST_H_

#include <string>

#include "ash/public/cpp/login_accelerators.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
// TODO(https://crbug.com/1164001): move to forward declaration
#include "chrome/browser/ash/login/ui/webui_login_view.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "components/user_manager/user_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class MockLoginDisplayHost : public LoginDisplayHost {
 public:
  MockLoginDisplayHost();

  MockLoginDisplayHost(const MockLoginDisplayHost&) = delete;
  MockLoginDisplayHost& operator=(const MockLoginDisplayHost&) = delete;

  virtual ~MockLoginDisplayHost();

  MOCK_METHOD(LoginDisplay*, GetLoginDisplay, (), (override));
  MOCK_METHOD(ExistingUserController*,
              GetExistingUserController,
              (),
              (override));
  MOCK_METHOD(gfx::NativeWindow, GetNativeWindow, (), (const, override));
  MOCK_METHOD(OobeUI*, GetOobeUI, (), (const, override));
  MOCK_METHOD(content::WebContents*, GetOobeWebContents, (), (const, override));
  MOCK_METHOD(WebUILoginView*, GetWebUILoginView, (), (const, override));
  MOCK_METHOD(void, BeforeSessionStart, (), (override));

  // Workaround for move-only args in GMock.
  MOCK_METHOD(void, MockFinalize, (base::OnceClosure*));
  void Finalize(base::OnceClosure completion_callback) override {
    MockFinalize(&completion_callback);
  }

  MOCK_METHOD(void, FinalizeImmediately, (), (override));
  MOCK_METHOD(void, SetStatusAreaVisible, (bool), (override));
  MOCK_METHOD(void, StartWizard, (OobeScreenId), (override));
  MOCK_METHOD(WizardController*, GetWizardController, (), (override));
  MOCK_METHOD(KioskLaunchController*, GetKioskLaunchController, (), (override));

  // Workaround for move-only args in GMock.
  MOCK_METHOD(void, MockStartUserAdding, (base::OnceClosure*));
  void StartUserAdding(base::OnceClosure completion_callback) {
    MockStartUserAdding(&completion_callback);
  }

  MOCK_METHOD(void, CancelUserAdding, (), (override));
  MOCK_METHOD(void, StartSignInScreen, (), (override));
  MOCK_METHOD(void, OnPreferencesChanged, (), (override));
  MOCK_METHOD(void, StartKiosk, (const KioskAppId&, bool), (override));
  MOCK_METHOD(void, AttemptShowEnableConsumerKioskScreen, (), (override));
  MOCK_METHOD(void, ShowGaiaDialog, (const AccountId&), (override));
  MOCK_METHOD(void, ShowOsInstallScreen, (), (override));
  MOCK_METHOD(void, ShowGuestTosScreen, (), (override));
  MOCK_METHOD(void, HideOobeDialog, (), (override));
  MOCK_METHOD(void, SetShelfButtonsEnabled, (bool), (override));
  MOCK_METHOD(void, UpdateOobeDialogState, (OobeDialogState state), (override));

  MOCK_METHOD(void, CompleteLogin, (const UserContext&), (override));
  MOCK_METHOD(void, OnGaiaScreenReady, (), (override));
  MOCK_METHOD(void, SetDisplayEmail, (const std::string&), (override));
  MOCK_METHOD(void,
              SetDisplayAndGivenName,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void, LoadWallpaper, (const AccountId&), (override));
  MOCK_METHOD(void, LoadSigninWallpaper, (), (override));
  MOCK_METHOD(bool,
              IsUserAllowlisted,
              (const AccountId&, const absl::optional<user_manager::UserType>&),
              (override));
  MOCK_METHOD(void, CancelPasswordChangedFlow, (), (override));
  MOCK_METHOD(void, MigrateUserData, (const std::string&), (override));
  MOCK_METHOD(void, ResyncUserData, (), (override));
  MOCK_METHOD(bool,
              HandleAccelerator,
              (LoginAcceleratorAction action),
              (override));
  MOCK_METHOD(void, HandleDisplayCaptivePortal, (), (override));
  MOCK_METHOD(void, UpdateAddUserButtonStatus, (), (override));
  MOCK_METHOD(void, RequestSystemInfoUpdate, (), (override));
  MOCK_METHOD(bool, HasUserPods, (), (override));
  MOCK_METHOD(void, VerifyOwnerForKiosk, (base::OnceClosure), (override));
  MOCK_METHOD(void, AddObserver, (LoginDisplayHost::Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (LoginDisplayHost::Observer*), (override));
  MOCK_METHOD(SigninUI*, GetSigninUI, (), (override));
  MOCK_METHOD(bool, IsWizardControllerCreated, (), (const, final));
  MOCK_METHOD(bool,
              GetKeyboardRemappedPrefValue,
              (const std::string& pref_name, int* value),
              (const, final));
  MOCK_METHOD(void,
              AddWizardCreatedObserverForTests,
              (base::RepeatingClosure on_created),
              (final));
  MOCK_METHOD(WizardContext*, GetWizardContextForTesting, (), (final));
  MOCK_METHOD(WizardContext*, GetWizardContext, (), (override));
  MOCK_METHOD(bool, IsWebUIStarted, (), (const final));
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::MockLoginDisplayHost;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_MOCK_LOGIN_DISPLAY_HOST_H_
