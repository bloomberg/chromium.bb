// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_APP_MODE_KIOSK_LAUNCH_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_APP_MODE_KIOSK_LAUNCH_CONTROLLER_H_

#include "chrome/browser/chromeos/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_profile_loader.h"
#include "chrome/browser/chromeos/login/app_mode/app_launch_signin_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"

namespace chromeos {

class LoginDisplayHost;
class OobeUI;

// Controller for the kiosk launch process, responsible for loading the kiosk
// profile, and updating the splash screen UI.
//
// Splash screen has several specific things the are considered during it's
// implementation:
// 1. There is a timer, which shows the splash screen for at least 10 seconds to
//    allow used to exit the mode.
// 2. User can at any moment open network configuration menu using a shortcut
//    CTRL+ALT+N to set up the network before the app actually starts.
//
// These are taken into consideration while designing the logic of
// KioskLaunchController.
//
// The launch process involves following steps:
// 1. Perform cryptohome login operations and initialize the profile.
// 2. Initialize KioskAppLauncher and wait for it to emit action depending on
//    the current app installation state(e.g. to install the app the
//    launcher will call KioskLaunchController::InitializeNetwork)
// 3. KioskLaunchController waits for network to be ready and when it is, it
//    calls KioskAppLauncher::ContinueWithNetworkReady() to continue the
//    installation.
// 4. After the app is installed, KioskLaunchController is waiting for the
//    splash screen timer to fire. (We can also end up in this state when the
//    app was already installed).
// 5. KioskLaunchController launches the app by calling
//    KioskAppLauncher::LaunchApp.
//
// At any moment the user can press the shortcut to configure network setup, so
// we need to also include the logic for the configuration menu.
// Besides, we should be always sure that while configuring network we are using
// the correct profile(kiosk app profile, not sign in profile). We will postpone
// the configuration if the shortcut is pressed to early.
//
// It is all encompassed within the combination of two states -- AppState and
// NetworkUI state.
class KioskLaunchController : public KioskProfileLoader::Delegate,
                              public AppLaunchSplashScreenView::Delegate,
                              public KioskAppLauncher::Delegate,
                              public AppLaunchSigninScreen::Delegate {
 public:
  using ReturnBoolCallback = base::Callback<bool()>;

  explicit KioskLaunchController(OobeUI* oobe_ui);
  KioskLaunchController(const KioskLaunchController&) = delete;
  KioskLaunchController& operator=(const KioskLaunchController&) = delete;
  ~KioskLaunchController() override;

  static std::unique_ptr<base::AutoReset<bool>>
  DisableWaitTimerAndLoginOperationsForTesting() WARN_UNUSED_RESULT;
  static std::unique_ptr<base::AutoReset<bool>> SkipSplashScreenWaitForTesting()
      WARN_UNUSED_RESULT;
  static std::unique_ptr<base::AutoReset<base::TimeDelta>>
  SetNetworkWaitForTesting(base::TimeDelta wait_time) WARN_UNUSED_RESULT;
  static std::unique_ptr<base::AutoReset<bool>> BlockAppLaunchForTesting()
      WARN_UNUSED_RESULT;
  static void SetNetworkTimeoutCallbackForTesting(base::OnceClosure* callback);
  static void SetCanConfigureNetworkCallbackForTesting(
      ReturnBoolCallback* callback);
  static void SetNeedOwnerAuthToConfigureNetworkCallbackForTesting(
      ReturnBoolCallback* callback);

  static std::unique_ptr<KioskLaunchController> CreateForTesting(
      AppLaunchSplashScreenView* view,
      std::unique_ptr<KioskAppLauncher> app_launcher);

  bool waiting_for_network() const {
    return app_state_ == AppState::INIT_NETWORK;
  }
  bool network_wait_timedout() const { return network_wait_timedout_; }
  bool showing_network_dialog() const {
    return network_ui_state_ == NetworkUIState::SHOWING;
  }

  void Start(const KioskAppId& kiosk_app_id, bool auto_launch);

 private:
  friend class KioskLaunchControllerTest;

  enum AppState {
    CREATING_PROFILE,  // Profile is being created.
    INIT_NETWORK,      // Waiting for the network to initialize.
    INSTALLING,        // App is installing.
    INSTALLED,  // App is installed, waiting for the splash screen timer to
                // fire.
    LAUNCHED    // App is being launched.
  };

  enum NetworkUIState {
    NOT_SHOWING,   // Network configure UI is not being shown.
    NEED_TO_SHOW,  // We need to show the UI as soon as we can.
    SHOWING        // Network configure UI is being shown.
  };

  KioskLaunchController();

  // AppLaunchSplashScreenView::Delegate:
  void OnConfigureNetwork() override;
  void OnCancelAppLaunch() override;
  void OnDeletingSplashScreenView() override;
  void OnNetworkConfigRequested() override;
  void OnNetworkConfigFinished() override;
  void OnNetworkStateChanged(bool online) override;
  KioskAppManagerBase::App GetAppData() override;
  bool IsNetworkRequired() override;

  // KioskAppLauncher::Delegate:
  void InitializeNetwork() override;
  bool IsNetworkReady() const override;
  bool IsShowingNetworkConfigScreen() const override;
  bool ShouldSkipAppInstallation() const override;
  void OnLaunchFailed(KioskAppLaunchError::Error error) override;
  void OnAppInstalling() override;
  void OnAppPrepared() override;
  void OnAppLaunched() override;
  void OnAppDataUpdated() override;
  void OnAppWindowCreated() override;

  // KioskProfileLoader::Delegate:
  void OnProfileLoaded(Profile* profile) override;
  void OnProfileLoadFailed(KioskAppLaunchError::Error error) override;
  void OnOldEncryptionDetected(const UserContext& user_context) override;

  // AppLaunchSigninScreen::Delegate:
  void OnOwnerSigninSuccess() override;

  // Whether the network could be configured during launching.
  bool CanConfigureNetwork();
  // Whether the owner password is needed to configure network.
  bool NeedOwnerAuthToConfigureNetwork();
  // Shows network configuration dialog if kiosk profile was already created or
  // postpones the display upon creation.
  void MaybeShowNetworkConfigureUI();
  // Shows the network configuration dialog.
  void ShowNetworkConfigureUI();
  void CloseNetworkConfigureScreenIfOnline();

  void HandleWebAppInstallFailed();

  void OnNetworkWaitTimedOut();
  void OnTimerFire();
  void CloseSplashScreen();
  void CleanUp();
  void LaunchApp();

  // Current state of the controller.
  AppState app_state_ = AppState::CREATING_PROFILE;
  // Current state of network configure dialog.
  NetworkUIState network_ui_state_ = NetworkUIState::NOT_SHOWING;

  LoginDisplayHost* const host_;  // Not owned, destructed upon shutdown.
  OobeUI* oobe_ui_ = nullptr;
  AppLaunchSplashScreenView* splash_screen_view_ = nullptr;  // Owned by OobeUI.
  KioskAppId kiosk_app_id_;                                  // Current app.
  Profile* profile_ = nullptr;                               // Not owned.

  // Whether app should be launched as soon as it is ready.
  bool launch_on_install_ = false;
  bool network_wait_timedout_ = false;
  // Whether the network is required for the installation.
  bool network_required_ = false;

  // Used to login into kiosk user profile.
  std::unique_ptr<KioskProfileLoader> kiosk_profile_loader_;
  std::unique_ptr<AppLaunchSigninScreen> signin_screen_;

  // A timer to ensure the app splash is shown for a minimum amount of time.
  base::OneShotTimer splash_wait_timer_;

  // Used to prepare and launch the actual kiosk app, is created after
  // profile initialization. Is nullptr for arc kiosks.
  std::unique_ptr<KioskAppLauncher> app_launcher_;

  // A timer that fires when the network was not prepared and we require user
  // network configuration to continue.
  base::OneShotTimer network_wait_timer_;

  base::WeakPtrFactory<KioskLaunchController> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_APP_MODE_KIOSK_LAUNCH_CONTROLLER_H_
