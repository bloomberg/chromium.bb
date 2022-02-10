// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/authpolicy/authpolicy_helper.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen_view.h"
#include "chrome/browser/ash/login/enrollment/enterprise_enrollment_helper.h"
// TODO(https://crbug.com/1164001): move to forward declaration
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/policy/active_directory/active_directory_join_delegate.h"
#include "chrome/browser/ash/policy/enrollment/account_status_check_fetcher.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "net/base/backoff_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class ElapsedTimer;
}

namespace ash {
namespace test {
class EnrollmentHelperMixin;
}

// The screen implementation that links the enterprise enrollment UI into the
// OOBE wizard.
class EnrollmentScreen
    : public BaseScreen,
      public EnterpriseEnrollmentHelper::EnrollmentStatusConsumer,
      public EnrollmentScreenView::Controller,
      public policy::ActiveDirectoryJoinDelegate {
 public:
  enum class Result {
    COMPLETED,
    BACK,
    SKIPPED_FOR_TESTS,
    TPM_ERROR,
    TPM_DBUS_ERROR
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  using TpmStatusCallback = chromeos::TpmManagerClient::TakeOwnershipCallback;
  EnrollmentScreen(EnrollmentScreenView* view,
                   const ScreenExitCallback& exit_callback);

  EnrollmentScreen(const EnrollmentScreen&) = delete;
  EnrollmentScreen& operator=(const EnrollmentScreen&) = delete;

  ~EnrollmentScreen() override;

  static EnrollmentScreen* Get(ScreenManager* manager);

  // Called when `view` has been destroyed. If this instance is destroyed before
  // the `view` it should call view->Unbind().
  void OnViewDestroyed(EnrollmentScreenView* view);

  // Setup how this screen will handle enrollment.
  void SetEnrollmentConfig(const policy::EnrollmentConfig& enrollment_config);

  // EnrollmentScreenView::Controller implementation:
  void OnLoginDone(const std::string& user,
                   const std::string& auth_code) override;
  void OnRetry() override;
  void OnCancel() override;
  void OnConfirmationClosed() override;
  void OnActiveDirectoryCredsProvided(const std::string& machine_name,
                                      const std::string& distinguished_name,
                                      int encryption_types,
                                      const std::string& username,
                                      const std::string& password) override;
  void OnDeviceAttributeProvided(const std::string& asset_id,
                                 const std::string& location) override;
  void OnIdentifierEntered(const std::string& email) override;

  // EnterpriseEnrollmentHelper::EnrollmentStatusConsumer implementation:
  void OnAuthError(const GoogleServiceAuthError& error) override;
  void OnEnrollmentError(policy::EnrollmentStatus status) override;
  void OnOtherError(EnterpriseEnrollmentHelper::OtherError error) override;
  void OnDeviceEnrolled() override;
  void OnDeviceAttributeUploadCompleted(bool success) override;
  void OnDeviceAttributeUpdatePermission(bool granted) override;

  // policy::ActiveDirectoryJoinDelegate implementation:
  void JoinDomain(const std::string& dm_token,
                  const std::string& domain_join_config,
                  policy::OnDomainJoinedCallback on_joined_callback) override;

  // Notification that the browser is being restarted.
  void OnBrowserRestart();

  // Used for testing.
  EnrollmentScreenView* GetView() { return view_; }

  void set_exit_callback_for_testing(const ScreenExitCallback& callback) {
    exit_callback_ = callback;
  }

  void set_tpm_ownership_callback_for_testing(TpmStatusCallback&& callback) {
    tpm_ownership_callback_for_testing_ = std::move(callback);
  }

  TpmStatusCallback get_tpm_ownership_callback_for_testing() {
    return base::BindOnce(&EnrollmentScreen::OnTpmStatusResponse,
                          weak_ptr_factory_.GetWeakPtr());
  }

 protected:
  // BaseScreen:
  bool MaybeSkip(WizardContext* context) override;
  void ShowImpl() override;
  void HideImpl() override;
  bool HandleAccelerator(LoginAcceleratorAction action) override;
  void OnUserAction(const std::string& action_id) override;

  // Expose the exit_callback to test screen overrides.
  ScreenExitCallback* exit_callback() { return &exit_callback_; }

 private:
  friend class ZeroTouchEnrollmentScreenUnitTest;
  friend class AutomaticReenrollmentScreenUnitTest;
  friend class test::EnrollmentHelperMixin;

  FRIEND_TEST_ALL_PREFIXES(AttestationAuthEnrollmentScreenTest, TestCancel);
  FRIEND_TEST_ALL_PREFIXES(ForcedAttestationAuthEnrollmentScreenTest,
                           TestCancel);
  FRIEND_TEST_ALL_PREFIXES(MultiAuthEnrollmentScreenTest, TestCancel);
  FRIEND_TEST_ALL_PREFIXES(ZeroTouchEnrollmentScreenUnitTest, Retry);
  FRIEND_TEST_ALL_PREFIXES(ZeroTouchEnrollmentScreenUnitTest, TestSuccess);
  FRIEND_TEST_ALL_PREFIXES(ZeroTouchEnrollmentScreenUnitTest,
                           DoNotRetryOnTopOfUser);
  FRIEND_TEST_ALL_PREFIXES(ZeroTouchEnrollmentScreenUnitTest,
                           DoNotRetryAfterSuccess);

  // The authentication mechanisms that this class can use.
  enum Auth {
    AUTH_ATTESTATION,
    AUTH_OAUTH,
  };

  // Updates view GAIA flow type which is used to modify visual appearance
  // of GAIA webview,
  void UpdateFlowType();

  // Sets the current config to use for enrollment.
  void SetConfig();

  // Called after account status is fetched.
  void OnAccountStatusFetched(
      const std::string& email,
      bool result,
      policy::AccountStatusCheckFetcher::AccountStatus status);

  // Creates an enrollment helper if needed.
  void CreateEnrollmentHelper();

  // Clears auth in `enrollment_helper_`. Deletes `enrollment_helper_` and runs
  // `callback` on completion. See the comment for
  // EnterpriseEnrollmentHelper::ClearAuth for details.
  void ClearAuth(base::OnceClosure callback);

  // Used as a callback for EnterpriseEnrollmentHelper::ClearAuth.
  virtual void OnAuthCleared(base::OnceClosure callback);

  // Shows successful enrollment status after all enrollment related file
  // operations are completed.
  void ShowEnrollmentStatusOnSuccess();

  // Logs an UMA event in one of the "Enrollment.*" histograms, depending on
  // `enrollment_mode_`.
  void UMA(policy::MetricEnrollment sample);

  // Do attestation based enrollment.
  void AuthenticateUsingAttestation();

  // Shows the interactive screen. Resets auth then shows the signin screen.
  void ShowInteractiveScreen();

  // Shows the signin screen. Used as a callback to run after auth reset.
  void ShowSigninScreen();

  // Shows the device attribute prompt screen.
  // Used as a callback to run after successful enrollment.
  void ShowAttributePromptScreen();

  // Record metrics when we encounter an enrollment error.
  void RecordEnrollmentErrorMetrics();

  // Advance to the next authentication mechanism if possible.
  bool AdvanceToNextAuth();

  // Similar to OnRetry(), but responds to a timer instead of the user
  // pressing the Retry button.
  void AutomaticRetry();

  // Processes a request to retry enrollment.
  // Called by OnRetry() and AutomaticRetry().
  void ProcessRetry();

  // Callback for Active Directory domain join.
  void OnActiveDirectoryJoined(const std::string& machine_name,
                               const std::string& username,
                               authpolicy::ErrorType error,
                               const std::string& machine_domain);

  // Tries to take TPM ownership.
  void TakeTpmOwnership();
  // Processes a reply from tpm_manager.
  void OnTpmStatusResponse(const ::tpm_manager::TakeOwnershipReply& reply);
  // Checks install attribute status to make sure that it is FIRST_INSTALL, in
  // this case we proceed with the enrollment. In other cases we either try to
  // wait for the FIRST_INSTALL status, or show a TpmErrorScreen with an ability
  // to reboot the device.
  void CheckInstallAttributesState();

  EnrollmentScreenView* view_;
  ScreenExitCallback exit_callback_;
  absl::optional<TpmStatusCallback> tpm_ownership_callback_for_testing_;
  policy::EnrollmentConfig config_;
  policy::EnrollmentConfig enrollment_config_;

  // 'Current' and 'Next' authentication mechanisms to be used.
  Auth current_auth_ = AUTH_OAUTH;
  Auth next_auth_ = AUTH_OAUTH;

  bool enrollment_failed_once_ = false;
  bool enrollment_succeeded_ = false;

  // Check tpm before enrollment starts if --tpm-is-dynamic switch is enabled.
  bool tpm_checked_ = false;
  // Number of retries to get other than TPM_NOT_OWNED install attributes state.
  int install_state_retries_ = 0;
  // Timer for install attribute to resolve.
  base::OneShotTimer wait_state_timer_;

  std::string enrolling_user_domain_;
  std::unique_ptr<base::ElapsedTimer> elapsed_timer_;
  net::BackoffEntry::Policy retry_policy_;
  std::unique_ptr<net::BackoffEntry> retry_backoff_;
  base::CancelableOnceClosure retry_task_;
  int num_retries_ = 0;
  std::unique_ptr<EnterpriseEnrollmentHelper> enrollment_helper_;
  policy::OnDomainJoinedCallback on_joined_callback_;
  std::unique_ptr<policy::AccountStatusCheckFetcher> status_checker_;

  // Helper to call AuthPolicyClient and cancel calls if needed. Used to join
  // Active Directory domain.
  std::unique_ptr<AuthPolicyHelper> authpolicy_login_helper_;

  base::WeakPtrFactory<EnrollmentScreen> weak_ptr_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::EnrollmentScreen;
}

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::EnrollmentScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_H_
