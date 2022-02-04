// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/login/auth/login_performer.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/metrics/login_event_recorder.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_names.h"
#include "google_apis/gaia/gaia_auth_util.h"

using base::UserMetricsAction;

namespace ash {

LoginPerformer::LoginPerformer(Delegate* delegate)
    : delegate_(delegate),
      last_login_failure_(AuthFailure::AuthFailureNone()) {}

LoginPerformer::~LoginPerformer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Deleting LoginPerformer";
  if (authenticator_.get())
    authenticator_->SetConsumer(NULL);
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, AuthStatusConsumer implementation:

void LoginPerformer::OnAuthFailure(const AuthFailure& failure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::RecordAction(UserMetricsAction("Login_Failure"));

  UMA_HISTOGRAM_ENUMERATION("Login.FailureReason",
                            failure.reason(),
                            AuthFailure::NUM_FAILURE_REASONS);

  LOG(ERROR) << "Login failure, reason=" << failure.reason()
             << ", error.state=" << failure.error().state();

  last_login_failure_ = failure;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&LoginPerformer::NotifyAuthFailure,
                                weak_factory_.GetWeakPtr(), failure));
}

void LoginPerformer::OnAuthSuccess(const UserContext& user_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::RecordAction(UserMetricsAction("Login_Success"));

  // Do not distinguish between offline and online success.
  UMA_HISTOGRAM_ENUMERATION("Login.SuccessReason", OFFLINE_AND_ONLINE,
                            NUM_SUCCESS_REASONS);

  VLOG(1) << "LoginSuccess hash: " << user_context.GetUserIDHash();
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&LoginPerformer::NotifyAuthSuccess,
                                weak_factory_.GetWeakPtr(), user_context));
}

void LoginPerformer::OnOffTheRecordAuthSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::RecordAction(UserMetricsAction("Login_GuestLoginSuccess"));

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&LoginPerformer::NotifyOffTheRecordAuthSuccess,
                                weak_factory_.GetWeakPtr()));
}

void LoginPerformer::OnPasswordChangeDetected(const UserContext& user_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  password_changed_ = true;
  password_changed_callback_count_++;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&LoginPerformer::NotifyPasswordChangeDetected,
                                weak_factory_.GetWeakPtr(), user_context));
}

void LoginPerformer::OnOldEncryptionDetected(const UserContext& user_context,
                                             bool has_incomplete_migration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&LoginPerformer::NotifyOldEncryptionDetected,
                                weak_factory_.GetWeakPtr(), user_context,
                                has_incomplete_migration));
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, public:

void LoginPerformer::PerformLogin(const UserContext& user_context,
                                  AuthorizationMode auth_mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auth_mode_ = auth_mode;
  user_context_ = user_context;

  if (RunTrustedCheck(base::BindOnce(&LoginPerformer::DoPerformLogin,
                                     weak_factory_.GetWeakPtr(), user_context_,
                                     auth_mode))) {
    return;
  }
  DoPerformLogin(user_context_, auth_mode);
}

void LoginPerformer::DoPerformLogin(const UserContext& user_context,
                                    AuthorizationMode auth_mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool wildcard_match = false;

  const AccountId& account_id = user_context.GetAccountId();
  if (!IsUserAllowlisted(account_id, &wildcard_match,
                         user_context.GetUserType())) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&LoginPerformer::NotifyAllowlistCheckFailure,
                                  weak_factory_.GetWeakPtr()));
    return;
  }

  if (user_context.GetAuthFlow() == UserContext::AUTH_FLOW_EASY_UNLOCK)
    SetupEasyUnlockUserFlow(user_context.GetAccountId());

  switch (auth_mode_) {
    case AuthorizationMode::kExternal: {
      RunOnlineAllowlistCheck(
          account_id, wildcard_match, user_context.GetRefreshToken(),
          base::BindOnce(&LoginPerformer::StartLoginCompletion,
                         weak_factory_.GetWeakPtr()),
          base::BindOnce(&LoginPerformer::NotifyAllowlistCheckFailure,
                         weak_factory_.GetWeakPtr()));
      break;
    }
    case AuthorizationMode::kInternal:
      StartAuthentication();
      break;
  }
}

void LoginPerformer::LoginAsPublicSession(const UserContext& user_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CheckPolicyForUser(user_context.GetAccountId())) {
    DCHECK(delegate_);
    delegate_->PolicyLoadFailed();
    return;
  }

  EnsureAuthenticator();
  authenticator_->LoginAsPublicSession(user_context);
}

void LoginPerformer::LoginOffTheRecord() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureAuthenticator();
  authenticator_->LoginOffTheRecord();
}

void LoginPerformer::LoginAsKioskAccount(const AccountId& app_account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureAuthenticator();
  authenticator_->LoginAsKioskAccount(app_account_id);
}

void LoginPerformer::LoginAsArcKioskAccount(
    const AccountId& arc_app_account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureAuthenticator();
  authenticator_->LoginAsArcKioskAccount(arc_app_account_id);
}

void LoginPerformer::LoginAsWebKioskAccount(
    const AccountId& web_app_account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureAuthenticator();
  authenticator_->LoginAsWebKioskAccount(web_app_account_id);
}

void LoginPerformer::RecoverEncryptedData(const std::string& old_password) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  authenticator_->RecoverEncryptedData(old_password);
}

void LoginPerformer::ResyncEncryptedData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  authenticator_->ResyncEncryptedData();
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, private:

void LoginPerformer::NotifyAllowlistCheckFailure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  delegate_->AllowlistCheckFailed(user_context_.GetAccountId().GetUserEmail());
}

void LoginPerformer::NotifyAuthFailure(const AuthFailure& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  delegate_->OnAuthFailure(error);
}

void LoginPerformer::NotifyAuthSuccess(const UserContext& user_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  // After delegate_->OnAuthSuccess(...) is called, delegate_ releases
  // LoginPerformer ownership. LP now manages it's lifetime on its own.
  base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  delegate_->OnAuthSuccess(user_context);
}

void LoginPerformer::NotifyOffTheRecordAuthSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  delegate_->OnOffTheRecordAuthSuccess();
}

void LoginPerformer::NotifyPasswordChangeDetected(
    const UserContext& user_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  delegate_->OnPasswordChangeDetected(user_context);
}

void LoginPerformer::NotifyOldEncryptionDetected(
    const UserContext& user_context,
    bool has_incomplete_migration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  delegate_->OnOldEncryptionDetected(user_context, has_incomplete_migration);
}

void LoginPerformer::StartLoginCompletion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "Online login completion started.";
  chromeos::LoginEventRecorder::Get()->AddLoginTimeMarker("AuthStarted", false);
  EnsureAuthenticator();
  authenticator_->CompleteLogin(std::make_unique<UserContext>(user_context_));
  user_context_.ClearSecrets();
}

void LoginPerformer::StartAuthentication() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "Offline auth started.";
  chromeos::LoginEventRecorder::Get()->AddLoginTimeMarker("AuthStarted", false);
  DCHECK(delegate_);
  EnsureAuthenticator();
  authenticator_->AuthenticateToLogin(
      std::make_unique<UserContext>(user_context_));
  user_context_.ClearSecrets();
}

void LoginPerformer::EnsureAuthenticator() {
  authenticator_ = CreateAuthenticator();
}
}  // namespace ash
