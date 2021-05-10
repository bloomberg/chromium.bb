// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_COMMON_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_COMMON_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/login_accelerators.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/login/ui/kiosk_app_menu_controller.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/signin_ui.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class AccountId;

namespace chromeos {

class DemoAppLauncher;
class LoginFeedback;

// LoginDisplayHostCommon contains code which is not specific to a particular UI
// implementation - the goal is to reduce code duplication between
// LoginDisplayHostMojo and LoginDisplayHostWebUI.
class LoginDisplayHostCommon : public LoginDisplayHost,
                               public BrowserListObserver,
                               public content::NotificationObserver,
                               public SigninUI {
 public:
  LoginDisplayHostCommon();
  ~LoginDisplayHostCommon() override;

  // LoginDisplayHost:
  void BeforeSessionStart() final;
  void Finalize(base::OnceClosure completion_callback) final;
  void FinalizeImmediately() final;
  KioskLaunchController* GetKioskLaunchController() final;
  void StartUserAdding(base::OnceClosure completion_callback) final;
  void StartSignInScreen() final;
  void PrewarmAuthentication() final;
  void StartDemoAppLaunch() final;
  void StartKiosk(const KioskAppId& kiosk_app_id, bool is_auto_launch) final;
  void AttemptShowEnableConsumerKioskScreen() final;
  void CompleteLogin(const UserContext& user_context) final;
  void OnGaiaScreenReady() final;
  void SetDisplayEmail(const std::string& email) final;
  void SetDisplayAndGivenName(const std::string& display_name,
                              const std::string& given_name) final;
  void LoadWallpaper(const AccountId& account_id) final;
  void LoadSigninWallpaper() final;
  bool IsUserAllowlisted(
      const AccountId& account_id,
      const base::Optional<user_manager::UserType>& user_type) final;
  void CancelPasswordChangedFlow() final;
  void MigrateUserData(const std::string& old_password) final;
  void ResyncUserData() final;
  bool HandleAccelerator(ash::LoginAcceleratorAction action) final;
  SigninUI* GetSigninUI() final;

  // SigninUI:
  void SetAuthSessionForOnboarding(const UserContext& user_context) final;
  void StartUserOnboarding() final;
  void StartSupervisionTransition() final;
  void StartEncryptionMigration(
      const UserContext& user_context,
      EncryptionMigrationMode migration_mode,
      base::OnceCallback<void(const UserContext&)> on_skip_migration) final;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 protected:
  virtual void OnStartSignInScreen() = 0;
  virtual void OnStartAppLaunch() = 0;
  virtual void OnBrowserCreated() = 0;
  virtual void OnStartUserAdding() = 0;
  virtual void OnFinalize() = 0;
  virtual void OnCancelPasswordChangedFlow() = 0;
  virtual void ShowEnableConsumerKioskScreen() = 0;

  // Deletes `auth_prewarmer_`.
  void OnAuthPrewarmDone();

  // Marks display host for deletion.
  void ShutdownDisplayHost();

  // Common code for OnStartSignInScreen() call above.
  void OnStartSignInScreenCommon();

  // Common code for ShowGaiaDialog() call above.
  void ShowGaiaDialogCommon(const AccountId& prefilled_account);

  // Active instance of authentication prewarmer.
  std::unique_ptr<AuthPrewarmer> auth_prewarmer_;

  // Kiosk launch controller.
  std::unique_ptr<KioskLaunchController> kiosk_launch_controller_;

  // Demo app launcher.
  std::unique_ptr<DemoAppLauncher> demo_app_launcher_;

  content::NotificationRegistrar registrar_;

 private:
  void Cleanup();
  // Callback invoked after the feedback is finished.
  void OnFeedbackFinished();

  // True if session start is in progress.
  bool session_starting_ = false;

  // Has ShutdownDisplayHost() already been called?  Used to avoid posting our
  // own deletion to the message loop twice if the user logs out while we're
  // still in the process of cleaning up after login (http://crbug.com/134463).
  bool shutting_down_ = false;

  // Used to make sure Finalize() is not called twice.
  bool is_finalizing_ = false;

  // Make sure chrome won't exit while we are at login/oobe screen.
  ScopedKeepAlive keep_alive_;

  // Called after host deletion.
  std::vector<base::OnceClosure> completion_callbacks_;

  KioskAppMenuController kiosk_app_menu_controller_;

  std::unique_ptr<LoginFeedback> login_feedback_;

  base::WeakPtrFactory<LoginDisplayHostCommon> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LoginDisplayHostCommon);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_COMMON_H_
