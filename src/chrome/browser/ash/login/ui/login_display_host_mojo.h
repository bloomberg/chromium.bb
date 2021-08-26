// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_HOST_MOJO_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_HOST_MOJO_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/challenge_response_auth_keys_loader.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ash/login/existing_user_controller.h"
// TODO(https://crbug.com/1164001): use forward declaration.
#include "chrome/browser/ash/login/mojo_system_info_dispatcher.h"
#include "chrome/browser/ash/login/screens/user_selection_screen.h"
#include "chrome/browser/ash/login/security_token_pin_dialog_host_ash_impl.h"
#include "chrome/browser/ash/login/ui/login_display_host_common.h"
#include "chrome/browser/ash/login/ui/oobe_ui_dialog_delegate.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ash/login/user_board_view_mojo.h"
// TODO(https://crbug.com/1164001): use forward declaration.
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/ash/login_screen_client_impl.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/challenge_response_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace views {
class View;
}  // namespace views

namespace ash {
class LoginDisplayMojo;
class OobeUIDialogDelegate;

// A LoginDisplayHost instance that sends requests to the views-based signin
// screen.
class LoginDisplayHostMojo : public LoginDisplayHostCommon,
                             public LoginScreenClientImpl::Delegate,
                             public AuthStatusConsumer,
                             public OobeUI::Observer,
                             public views::ViewObserver {
 public:
  explicit LoginDisplayHostMojo(DisplayedScreen displayed_screen);
  ~LoginDisplayHostMojo() override;

  // Called when the gaia dialog is destroyed.
  void OnDialogDestroyed(const OobeUIDialogDelegate* dialog);

  void SetUserCount(int user_count);

  // Show allowlist check failed error. Happens after user completes online
  // signin but allowlist check fails.
  void ShowAllowlistCheckFailedError();

  UserSelectionScreen* user_selection_screen() {
    return user_selection_screen_.get();
  }

  // LoginDisplayHost:
  LoginDisplay* GetLoginDisplay() override;
  ExistingUserController* GetExistingUserController() override;
  gfx::NativeWindow GetNativeWindow() const override;
  OobeUI* GetOobeUI() const override;
  content::WebContents* GetOobeWebContents() const override;
  WebUILoginView* GetWebUILoginView() const override;
  void OnFinalize() override;
  void SetStatusAreaVisible(bool visible) override;
  void StartWizard(OobeScreenId first_screen) override;
  WizardController* GetWizardController() override;
  void OnStartUserAdding() override;
  void CancelUserAdding() override;
  void OnStartSignInScreen() override;
  void OnPreferencesChanged() override;
  void OnStartAppLaunch() override;
  void OnBrowserCreated() override;
  void ShowGaiaDialog(const AccountId& prefilled_account) override;
  void HideOobeDialog() override;
  void SetShelfButtonsEnabled(bool enabled) override;
  void UpdateOobeDialogState(OobeDialogState state) override;
  void OnCancelPasswordChangedFlow() override;
  void ShowEnableConsumerKioskScreen() override;
  void HandleDisplayCaptivePortal() override;
  void UpdateAddUserButtonStatus() override;
  void RequestSystemInfoUpdate() override;
  bool HasUserPods() override;
  void VerifyOwnerForKiosk(base::OnceClosure on_success) override;
  void ShowPasswordChangedDialog(const AccountId& account_id,
                                 bool show_password_error) override;
  void StartBrowserDataMigration() override;
  void AddObserver(LoginDisplayHost::Observer* observer) override;
  void RemoveObserver(LoginDisplayHost::Observer* observer) override;

  // LoginScreenClientImpl::Delegate:
  void HandleAuthenticateUserWithPasswordOrPin(
      const AccountId& account_id,
      const std::string& password,
      bool authenticated_by_pin,
      base::OnceCallback<void(bool)> callback) override;
  void HandleAuthenticateUserWithEasyUnlock(
      const AccountId& account_id) override;
  void HandleAuthenticateUserWithChallengeResponse(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) override;
  void HandleHardlockPod(const AccountId& account_id) override;
  void HandleOnFocusPod(const AccountId& account_id) override;
  void HandleOnNoPodFocused() override;
  bool HandleFocusLockScreenApps(bool reverse) override;
  void HandleFocusOobeDialog() override;
  void HandleLaunchPublicSession(const AccountId& account_id,
                                 const std::string& locale,
                                 const std::string& input_method) override;

  // AuthStatusConsumer:
  void OnAuthFailure(const AuthFailure& error) override;
  void OnAuthSuccess(const UserContext& user_context) override;
  void OnPasswordChangeDetected(const UserContext& user_context) override;
  void OnOldEncryptionDetected(const UserContext& user_context,
                               bool has_incomplete_migration) override;

  // OobeUI::Observer:
  void OnCurrentScreenChanged(OobeScreenId current_screen,
                              OobeScreenId new_screen) override;
  void OnDestroyingOobeUI() override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

  bool IsOobeUIDialogVisible() const override;

  OobeUIDialogDelegate* dialog_for_test() { return dialog_; }

 private:
  void LoadOobeDialog();

  // Callback to be invoked when the `challenge_response_auth_keys_loader_`
  // completes building the currently available challenge-response keys. Used
  // only during the challenge-response authentication.
  void OnChallengeResponseKeysPrepared(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> on_auth_complete_callback,
      std::vector<ChallengeResponseKey> challenge_response_keys);

  // Helper methods to show and hide the dialog.
  void ShowDialog();
  void ShowFullScreen();
  void HideDialog();

  // Adds this as a `OobeUI::Observer` if it has not already been added as one.
  void ObserveOobeUI();

  // Removes this as a `OobeUI::Observer` if it has been added as an observer.
  void StopObservingOobeUI();

  // Create ExistingUserController and link it to LoginDisplayHostMojo so we can
  // consume auth status events.
  void CreateExistingUserController();

  // Consumer kiosk owner authentication functions.
  void CheckOwnerCredentials(const UserContext& user_context);
  void OnOwnerSigninSuccess();

  // Sets an extra flag that can hide/unhide offline login link if the offline
  // login timer has expired for a focused user.
  void MaybeUpdateOfflineLoginLinkVisibility(const AccountId& account_id);

  // State associated with a pending authentication attempt.
  struct AuthState {
    AuthState(AccountId account_id, base::OnceCallback<void(bool)> callback);
    ~AuthState();

    // Account that is being authenticated.
    AccountId account_id;
    // Callback that should be executed the authentication result is available.
    base::OnceCallback<void(bool)> callback;
  };
  std::unique_ptr<AuthState> pending_auth_state_;

  std::unique_ptr<LoginDisplayMojo> login_display_;

  std::unique_ptr<UserBoardViewMojo> user_board_view_mojo_;
  std::unique_ptr<UserSelectionScreen> user_selection_screen_;

  std::unique_ptr<ExistingUserController> existing_user_controller_;

  // Called after host deletion.
  std::vector<base::OnceClosure> completion_callbacks_;
  OobeUIDialogDelegate* dialog_ = nullptr;  // Not owned.
  std::unique_ptr<WizardController> wizard_controller_;

  // Number of users that are visible in the views login screen.
  int user_count_ = 0;

  // The account id of the user pod that's being focused.
  AccountId focused_pod_account_id_;

  // Fetches system information and sends it to the UI over mojo.
  std::unique_ptr<MojoSystemInfoDispatcher> system_info_updater_;

  // Prevents repeated calls to OnStartSigninScreen, which can happen when a
  // user cancels the Powerwash dialog in the login screen. Set to true on the
  // first OnStartSigninScreen and remains true afterward.
  bool signin_screen_started_ = false;

  ChallengeResponseAuthKeysLoader challenge_response_auth_keys_loader_;

  SecurityTokenPinDialogHostAshImpl security_token_pin_dialog_host_ash_impl_;

  // Set if this has been added as a `OobeUI::Observer`.
  bool added_as_oobe_observer_ = false;

  // Set if Gaia dialog is shown with prefilled email.
  absl::optional<AccountId> gaia_reauth_account_id_;

  // Store which screen is currently displayed.
  DisplayedScreen displayed_screen_ = DisplayedScreen::SIGN_IN_SCREEN;

  // Consumer kiosk owner fields.
  AccountId owner_account_id_;
  base::OnceClosure owner_verified_callback_;
  scoped_refptr<ExtendedAuthenticator> extended_authenticator_;

  base::ScopedObservation<views::View, views::ViewObserver> scoped_observation_{
      this};

  base::ObserverList<LoginDisplayHost::Observer> observers_;

  base::WeakPtrFactory<LoginDisplayHostMojo> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LoginDisplayHostMojo);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_HOST_MOJO_H_
