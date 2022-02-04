// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EXISTING_USER_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_EXISTING_USER_CONTROLLER_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "ash/components/login/auth/login_performer.h"
#include "ash/components/login/auth/user_context.h"
#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/login/saml/password_sync_token_checkers_collection.h"
#include "chrome/browser/ash/login/screens/encryption_migration_mode.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/ui/login_display.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace base {
class ElapsedTimer;
class ListValue;
}

namespace ash {
class CrosSettings;
class KioskAppId;
class OAuth2TokenInitializer;
enum class SigninError;

namespace login {
class NetworkStateHelper;
}

// ExistingUserController is used to handle login when someone has already
// logged into the machine. ExistingUserController is created and owned by
// LoginDisplayHost.
class ExistingUserController : public LoginDisplay::Delegate,
                               public content::NotificationObserver,
                               public LoginPerformer::Delegate,
                               public UserSessionManagerDelegate,
                               public user_manager::UserManager::Observer {
 public:
  // Returns the current existing user controller fetched from the current
  // LoginDisplayHost instance.
  static ExistingUserController* current_controller();

  // All UI initialization is deferred till Init() call.
  ExistingUserController();

  ExistingUserController(const ExistingUserController&) = delete;
  ExistingUserController& operator=(const ExistingUserController&) = delete;

  ~ExistingUserController() override;

  // Creates and shows login UI for known users.
  void Init(const user_manager::UserList& users);

  // Start the auto-login timer.
  void StartAutoLoginTimer();

  // Stop the auto-login timer when a login attempt begins.
  void StopAutoLoginTimer();

  // Cancels current password changed flow.
  void CancelPasswordChangedFlow();

  // Decrypt cryptohome using user provided `old_password` and migrate to new
  // password.
  void MigrateUserData(const std::string& old_password);

  // Ignore password change, remove existing cryptohome and force full sync of
  // user data.
  void ResyncUserData();

  // Returns name of the currently connected network, for error message,
  std::u16string GetConnectedNetworkName() const;

  // LoginDisplay::Delegate: implementation
  bool IsSigninInProgress() const override;
  void Login(const UserContext& user_context,
             const SigninSpecifics& specifics) override;
  void OnSigninScreenReady() override;
  void OnStartEnterpriseEnrollment() override;
  void OnStartKioskEnableScreen() override;
  void OnStartKioskAutolaunchScreen() override;
  void ResetAutoLoginTimer() override;

  void CompleteLogin(const UserContext& user_context);
  void OnGaiaScreenReady();
  void SetDisplayEmail(const std::string& email);
  void SetDisplayAndGivenName(const std::string& display_name,
                              const std::string& given_name);
  bool IsUserAllowlisted(
      const AccountId& account_id,
      const absl::optional<user_manager::UserType>& user_type);

  // user_manager::UserManager::Observer:
  void LocalStateChanged(user_manager::UserManager* user_manager) override;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Add/remove a delegate that we will pass AuthStatusConsumer events to.
  void AddLoginStatusConsumer(AuthStatusConsumer* consumer);
  void RemoveLoginStatusConsumer(const AuthStatusConsumer* consumer);

  // Returns value of LoginPerformer::auth_mode() (cached if performer is
  // destroyed).
  LoginPerformer::AuthorizationMode auth_mode() const;

  // Returns value of LoginPerformer::password_changed() (cached if performer is
  // destroyed).
  bool password_changed() const;

  // Returns true if auto launch is scheduled and the timer is running.
  bool IsAutoLoginTimerRunningForTesting() const {
    return auto_login_timer_ && auto_login_timer_->IsRunning();
  }

  // Get account id used in last login attempt.
  AccountId GetLastLoginAttemptAccountId() const;

  // Extracts out users allowed on login screen.
  static user_manager::UserList ExtractLoginUsers(
      const user_manager::UserList& users);

 private:
  friend class ExistingUserControllerTest;
  friend class ExistingUserControllerAutoLoginTest;
  friend class ExistingUserControllerPublicSessionTest;
  friend class MockLoginPerformerDelegate;
  friend class ExistingUserControllerForcedOnlineAuthTest;

  FRIEND_TEST_ALL_PREFIXES(ExistingUserControllerTest, ExistingUserLogin);

  class PolicyStoreLoadWaiter;

  void LoginAsGuest();
  void LoginAsPublicSession(const UserContext& user_context);
  void LoginAsKioskApp(KioskAppId kiosk_app_id);
  // Retrieve public session auto-login policy and update the
  // timer.
  void ConfigureAutoLogin();

  // Trigger public session auto-login.
  void OnPublicSessionAutoLoginTimerFire();

  // LoginPerformer::Delegate implementation:
  void OnAuthFailure(const AuthFailure& error) override;
  void OnAuthSuccess(const UserContext& user_context) override;
  void OnOffTheRecordAuthSuccess() override;
  void OnPasswordChangeDetected(const UserContext& user_context) override;
  void OnOldEncryptionDetected(const UserContext& user_context,
                               bool has_incomplete_migration) override;
  void AllowlistCheckFailed(const std::string& email) override;
  void PolicyLoadFailed() override;

  // UserSessionManagerDelegate implementation:
  void OnProfilePrepared(Profile* profile, bool browser_launched) override;

  // Called when device settings change.
  void DeviceSettingsChanged();

  // Show error message corresponding to `error`. If `details` string is not
  // empty, it specify additional error text provided by authenticator, it is
  // not localized.
  void ShowError(SigninError error, const std::string& details);

  // Handles result of ownership check and starts enterprise or kiosk enrollment
  // if applicable.
  void OnEnrollmentOwnershipCheckCompleted(
      DeviceSettingsService::OwnershipStatus status);

  // Handles result of consumer kiosk configurability check and starts
  // enable kiosk screen if applicable.
  void OnConsumerKioskAutoLaunchCheckCompleted(
      KioskAppManager::ConsumerKioskAutoLaunchStatus status);

  // Enters the enterprise enrollment screen.
  void ShowEnrollmentScreen();

  // Shows privacy notification in case of auto lunch managed guest session.
  void ShowAutoLaunchManagedGuestSessionNotification();

  // Shows kiosk feature enable screen.
  void ShowKioskEnableScreen();

  // Shows "kiosk auto-launch permission" screen.
  void ShowKioskAutolaunchScreen();

  // Shows "filesystem encryption migration" screen.
  void ShowEncryptionMigrationScreen(const UserContext& user_context,
                                     EncryptionMigrationMode migration_mode);

  // Shows "critical TPM error" screen.
  void ShowTPMError();

  // Shows "password changed" dialog.
  void ShowPasswordChangedDialog(const UserContext& user_context);

  // Creates `login_performer_` if necessary and calls login() on it.
  void PerformLogin(const UserContext& user_context,
                    LoginPerformer::AuthorizationMode auth_mode);

  // Calls login() on previously-used `login_performer_`.
  void ContinuePerformLogin(LoginPerformer::AuthorizationMode auth_mode,
                            const UserContext& user_context);

  // Removes the constraint that user home mount requires ext4 encryption from
  // `user_context`, then calls login() on previously-used `login_performer`.
  void ContinuePerformLoginWithoutMigration(
      LoginPerformer::AuthorizationMode auth_mode,
      const UserContext& user_context);

  // Asks the user to enter their password again.
  void RestartLogin(const UserContext& user_context);

  // Updates the `login_display_` attached to this controller.
  void UpdateLoginDisplay(const user_manager::UserList& users);

  // Sends an accessibility alert event to extension listeners.
  void SendAccessibilityAlert(const std::string& alert_text);

  // Continues public session login if the associated user cloud policy store is
  // loaded.
  // This is intended to delay public session login if the login is requested
  // before the policy store is initialized (in which case the login attempt
  // would fail).
  void LoginAsPublicSessionWithPolicyStoreReady(
      const UserContext& user_context);

  // Callback invoked when the keyboard layouts available for a public session
  // have been retrieved. Selects the first layout from the list and continues
  // login.
  void SetPublicSessionKeyboardLayoutAndLogin(
      const UserContext& user_context,
      std::unique_ptr<base::ListValue> keyboard_layouts);

  // Starts the actual login process for a public session. Invoked when all
  // preconditions have been verified.
  void LoginAsPublicSessionInternal(const UserContext& user_context);

  // Performs sets of actions right prior to login has been started.
  void PerformPreLoginActions(const UserContext& user_context);

  // Performs set of actions when login has been completed or has been
  // cancelled. If `start_auto_login_timer` is true than
  // auto-login timer is started.
  void PerformLoginFinishedActions(bool start_auto_login_timer);

  // Invokes `continuation` after verifying that cryptohome service is
  // available.
  void ContinueLoginWhenCryptohomeAvailable(base::OnceClosure continuation,
                                            bool service_is_available);

  // Invokes `continuation` after verifying that the device is not disabled.
  void ContinueLoginIfDeviceNotDisabled(base::OnceClosure continuation);

  // Signs in as a new user. This is a continuation of CompleteLogin() that gets
  // invoked after it has been verified that the device is not disabled.
  void DoCompleteLogin(const UserContext& user_context);

  // Signs in as a known user. This is a continuation of Login() that gets
  // invoked after it has been verified that the device is not disabled.
  void DoLogin(const UserContext& user_context,
               const SigninSpecifics& specifics);

  // Callback invoked when `oauth2_token_initializer_` has finished.
  void OnOAuth2TokensFetched(bool success, const UserContext& user_context);

  // Triggers online login for the given `account_id`.
  void ForceOnlineLoginForAccountId(const AccountId& account_id);

  // Clear the recorded displayed email, displayed name, given name so it won't
  // affect any future attempts.
  void ClearRecordedNames();

  // Restart authpolicy daemon in case of Active Directory authentication.
  // Used to prevent data from leaking from one user session into another.
  // Should be called to cancel AuthPolicyHelper::TryAuthenticateUser call.
  void ClearActiveDirectoryState();

  // Public session auto-login timer.
  std::unique_ptr<base::OneShotTimer> auto_login_timer_;

  // Auto-login timeout, in milliseconds.
  int auto_login_delay_;

  // True if a profile has been prepared.
  bool profile_prepared_ = false;

  // AccountId for public session auto-login.
  AccountId public_session_auto_login_account_id_ = EmptyAccountId();

  // Used to execute login operations.
  std::unique_ptr<LoginPerformer> login_performer_;

  // Delegates to forward all authentication status events to.
  // Tests can use this to receive authentication status events.
  base::ObserverList<AuthStatusConsumer> auth_status_consumers_;

  // AccountId of the last login attempt.
  AccountId last_login_attempt_account_id_ = EmptyAccountId();

  // Whether the last login attempt was an auto login.
  bool last_login_attempt_was_auto_login_ = false;

  // Number of login attempts. Used to show help link when > 1 unsuccessful
  // logins for the same user.
  size_t num_login_attempts_ = 0;

  // Interface to the signed settings store.
  CrosSettings* cros_settings_;

  // URL to append to start Guest mode with.
  GURL guest_mode_url_;

  // Used for notifications during the login process.
  content::NotificationRegistrar registrar_;

  // The displayed email for the next login attempt set by `SetDisplayEmail`.
  std::string display_email_;

  // The displayed name for the next login attempt set by
  // `SetDisplayAndGivenName`.
  std::u16string display_name_;

  // The given name for the next login attempt set by `SetDisplayAndGivenName`.
  std::u16string given_name_;

  // Whether login attempt is running.
  bool is_login_in_progress_ = false;

  // True if password has been changed for user who is completing sign in.
  // Set in OnLoginSuccess. Before that use LoginPerformer::password_changed().
  bool password_changed_ = false;

  // Set in OnLoginSuccess. Before that use LoginPerformer::auth_mode().
  // Initialized with `kExternal` as more restricted mode.
  LoginPerformer::AuthorizationMode auth_mode_ =
      LoginPerformer::AuthorizationMode::kExternal;

  // Timer when the signin screen was first displayed. Used to measure the time
  // from showing the screen until a successful login is performed.
  std::unique_ptr<base::ElapsedTimer> timer_init_;

  // Timer for the interval to wait for the reboot after TPM error UI was shown.
  base::OneShotTimer reboot_timer_;

  // Collection of verifiers that check validity of password sync token for SAML
  // users.
  std::unique_ptr<PasswordSyncTokenCheckersCollection> sync_token_checkers_;

  std::unique_ptr<login::NetworkStateHelper> network_state_helper_;

  base::CallbackListSubscription show_user_names_subscription_;
  base::CallbackListSubscription allow_new_user_subscription_;
  base::CallbackListSubscription allow_guest_subscription_;
  base::CallbackListSubscription users_subscription_;
  base::CallbackListSubscription local_account_auto_login_id_subscription_;
  base::CallbackListSubscription local_account_auto_login_delay_subscription_;
  base::CallbackListSubscription family_link_allowed_subscription_;

  std::unique_ptr<OAuth2TokenInitializer> oauth2_token_initializer_;

  // Used to wait for cloud policy store load during public session login, if
  // the store is not yet initialized when the login is attempted.
  std::unique_ptr<PolicyStoreLoadWaiter> policy_store_waiter_;

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      observed_user_manager_{this};

  // Factory of callbacks.
  base::WeakPtrFactory<ExistingUserController> weak_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::ExistingUserController;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_EXISTING_USER_CONTROLLER_H_
