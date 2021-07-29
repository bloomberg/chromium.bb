// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/login_performer.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/single_thread_task_runner.h"
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

namespace chromeos {

LoginPerformer::LoginPerformer(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    Delegate* delegate)
    : delegate_(delegate),
      task_runner_(std::move(task_runner)),
      last_login_failure_(AuthFailure::AuthFailureNone()) {}

LoginPerformer::~LoginPerformer() {
  DVLOG(1) << "Deleting LoginPerformer";
  if (authenticator_.get())
    authenticator_->SetConsumer(NULL);
  if (extended_authenticator_.get())
    extended_authenticator_->SetConsumer(NULL);
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, AuthStatusConsumer implementation:

void LoginPerformer::OnAuthFailure(const AuthFailure& failure) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::RecordAction(UserMetricsAction("Login_Failure"));

  UMA_HISTOGRAM_ENUMERATION("Login.FailureReason",
                            failure.reason(),
                            AuthFailure::NUM_FAILURE_REASONS);

  LOG(ERROR) << "Login failure, reason=" << failure.reason()
             << ", error.state=" << failure.error().state();

  last_login_failure_ = failure;
  if (delegate_) {
    delegate_->OnAuthFailure(failure);
    return;
  }

  // COULD_NOT_MOUNT_CRYPTOHOME, COULD_NOT_MOUNT_TMPFS:
  // happens during offline auth only.
  NOTREACHED();
}

void LoginPerformer::OnAuthSuccess(const UserContext& user_context) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::RecordAction(UserMetricsAction("Login_Success"));

  // Do not distinguish between offline and online success.
  UMA_HISTOGRAM_ENUMERATION("Login.SuccessReason", OFFLINE_AND_ONLINE,
                            NUM_SUCCESS_REASONS);

  VLOG(1) << "LoginSuccess hash: " << user_context.GetUserIDHash();
  DCHECK(delegate_);
  // After delegate_->OnAuthSuccess(...) is called, delegate_ releases
  // LoginPerformer ownership. LP now manages it's lifetime on its own.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  delegate_->OnAuthSuccess(user_context);
}

void LoginPerformer::OnOffTheRecordAuthSuccess() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::RecordAction(UserMetricsAction("Login_GuestLoginSuccess"));

  if (delegate_)
    delegate_->OnOffTheRecordAuthSuccess();
  else
    NOTREACHED();
}

void LoginPerformer::OnPasswordChangeDetected(const UserContext& user_context) {
  password_changed_ = true;
  password_changed_callback_count_++;
  if (delegate_) {
    delegate_->OnPasswordChangeDetected(user_context);
  } else {
    NOTREACHED();
  }
}

void LoginPerformer::OnOldEncryptionDetected(const UserContext& user_context,
                                             bool has_incomplete_migration) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (delegate_)
    delegate_->OnOldEncryptionDetected(user_context, has_incomplete_migration);
  else
    NOTREACHED();
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, public:

void LoginPerformer::NotifyAllowlistCheckFailure() {
  if (delegate_)
    delegate_->AllowlistCheckFailed(
        user_context_.GetAccountId().GetUserEmail());
  else
    NOTREACHED();
}

void LoginPerformer::PerformLogin(const UserContext& user_context,
                                  AuthorizationMode auth_mode) {
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
  bool wildcard_match = false;

  const AccountId& account_id = user_context.GetAccountId();
  if (!IsUserAllowlisted(account_id, &wildcard_match,
                         user_context.GetUserType())) {
    NotifyAllowlistCheckFailure();
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
  if (!CheckPolicyForUser(user_context.GetAccountId())) {
    DCHECK(delegate_);
    if (delegate_)
      delegate_->PolicyLoadFailed();
    return;
  }

  EnsureAuthenticator();
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&Authenticator::LoginAsPublicSession,
                                        authenticator_.get(), user_context));
}

void LoginPerformer::LoginOffTheRecord() {
  EnsureAuthenticator();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Authenticator::LoginOffTheRecord, authenticator_.get()));
}

void LoginPerformer::LoginAsKioskAccount(const AccountId& app_account_id) {
  EnsureAuthenticator();
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&Authenticator::LoginAsKioskAccount,
                                        authenticator_.get(), app_account_id));
}

void LoginPerformer::LoginAsArcKioskAccount(
    const AccountId& arc_app_account_id) {
  EnsureAuthenticator();
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Authenticator::LoginAsArcKioskAccount,
                                authenticator_.get(), arc_app_account_id));
}

void LoginPerformer::LoginAsWebKioskAccount(
    const AccountId& web_app_account_id) {
  EnsureAuthenticator();
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Authenticator::LoginAsWebKioskAccount,
                                authenticator_.get(), web_app_account_id));
}

void LoginPerformer::RecoverEncryptedData(const std::string& old_password) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&Authenticator::RecoverEncryptedData,
                                        authenticator_.get(), old_password));
}

void LoginPerformer::ResyncEncryptedData() {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&Authenticator::ResyncEncryptedData,
                                        authenticator_.get()));
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, private:

void LoginPerformer::EnsureExtendedAuthenticator() {
  if (extended_authenticator_.get())
    extended_authenticator_->SetConsumer(NULL);
  extended_authenticator_ = ExtendedAuthenticator::Create(this);
}

void LoginPerformer::StartLoginCompletion() {
  VLOG(1) << "Online login completion started.";
  chromeos::LoginEventRecorder::Get()->AddLoginTimeMarker("AuthStarted", false);
  EnsureAuthenticator();
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&chromeos::Authenticator::CompleteLogin,
                                        authenticator_.get(), user_context_));
  user_context_.ClearSecrets();
}

void LoginPerformer::StartAuthentication() {
  VLOG(1) << "Offline auth started.";
  chromeos::LoginEventRecorder::Get()->AddLoginTimeMarker("AuthStarted", false);
  if (delegate_) {
    EnsureAuthenticator();
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&Authenticator::AuthenticateToLogin,
                                          authenticator_.get(), user_context_));
  } else {
    NOTREACHED();
  }
  user_context_.ClearSecrets();
}

void LoginPerformer::EnsureAuthenticator() {
  authenticator_ = CreateAuthenticator();
}
}  // namespace chromeos
