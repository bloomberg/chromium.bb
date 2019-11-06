// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_manager_base.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/signin/core/browser/signin_switches.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"

SigninManagerBase::SigninManagerBase(
    SigninClient* client,
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
    GaiaCookieManagerService* cookie_manager_service,
    signin::AccountConsistencyMethod account_consistency)
    : client_(client),
      token_service_(token_service),
      account_tracker_service_(account_tracker_service),
      initialized_(false),
      account_consistency_(account_consistency),
      weak_pointer_factory_(this) {
  DCHECK(client_);
  DCHECK(account_tracker_service_);
}

SigninManagerBase::~SigninManagerBase() {
  DCHECK(!observer_);
}

// static
void SigninManagerBase::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kGoogleServicesHostedDomain,
                               std::string());
  registry->RegisterStringPref(prefs::kGoogleServicesLastAccountId,
                               std::string());
  registry->RegisterStringPref(prefs::kGoogleServicesLastUsername,
                               std::string());
  registry->RegisterStringPref(prefs::kGoogleServicesAccountId, std::string());
  registry->RegisterStringPref(prefs::kGoogleServicesUserAccountId,
                               std::string());
  registry->RegisterBooleanPref(prefs::kAutologinEnabled, true);
  registry->RegisterListPref(prefs::kReverseAutologinRejectedEmailList);
  registry->RegisterBooleanPref(prefs::kSigninAllowed, true);
  registry->RegisterInt64Pref(prefs::kSignedInTime,
                              base::Time().ToInternalValue());
  registry->RegisterBooleanPref(prefs::kSignedInWithCredentialProvider, false);

  // Deprecated prefs: will be removed in a future release.
  registry->RegisterStringPref(prefs::kGoogleServicesUsername, std::string());
}

// static
void SigninManagerBase::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kGoogleServicesUsernamePattern,
                               std::string());
}

void SigninManagerBase::Initialize(PrefService* local_state) {
  // Should never call Initialize() twice.
  DCHECK(!IsInitialized());
  initialized_ = true;

  // If the user is clearing the token service from the command line, then
  // clear their login info also (not valid to be logged in without any
  // tokens).
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kClearTokenService)) {
    client_->GetPrefs()->ClearPref(prefs::kGoogleServicesAccountId);
    client_->GetPrefs()->ClearPref(prefs::kGoogleServicesUsername);
    client_->GetPrefs()->ClearPref(prefs::kGoogleServicesUserAccountId);
  }

  std::string account_id =
      client_->GetPrefs()->GetString(prefs::kGoogleServicesAccountId);

  // Handle backward compatibility: if kGoogleServicesAccountId is empty, but
  // kGoogleServicesUsername is not, then this is an old profile that needs to
  // be updated.  kGoogleServicesUserAccountId should not be empty, and contains
  // the gaia_id.  Use both properties to prime the account tracker before
  // proceeding.
  if (account_id.empty()) {
    std::string pref_account_username =
        client_->GetPrefs()->GetString(prefs::kGoogleServicesUsername);
    if (!pref_account_username.empty()) {
      // This is an old profile connected to a google account.  Migrate from
      // kGoogleServicesUsername to kGoogleServicesAccountId.
      std::string pref_gaia_id =
          client_->GetPrefs()->GetString(prefs::kGoogleServicesUserAccountId);

      // If kGoogleServicesUserAccountId is empty, then this is either a cros
      // machine or a really old profile on one of the other platforms.  However
      // in this case the account tracker should have the gaia_id so fetch it
      // from there.
      if (pref_gaia_id.empty()) {
        AccountInfo info = account_tracker_service_->FindAccountInfoByEmail(
            pref_account_username);
        pref_gaia_id = info.gaia;
      }

      // If |pref_gaia_id| is still empty, this means the profile has been in
      // an auth error state for some time (since M39).  It could also mean
      // a profile that has not been used since M33.  Before migration to gaia
      // id is complete, the returned value will be the normalized email, which
      // is correct.  After the migration, the returned value will be empty,
      // which means the user is essentially signed out.
      // TODO(rogerta): may want to show a toast or something.
      account_id = account_tracker_service_->SeedAccountInfo(
          pref_gaia_id, pref_account_username);

      // Set account id before removing obsolete user name in case crash in the
      // middle.
      client_->GetPrefs()->SetString(prefs::kGoogleServicesAccountId,
                                     account_id);

      // Now remove obsolete preferences.
      client_->GetPrefs()->ClearPref(prefs::kGoogleServicesUsername);
    }

    // TODO(rogerta): once migration to gaia id is complete, remove
    // kGoogleServicesUserAccountId and change all uses of that pref to
    // kGoogleServicesAccountId.
  }

  if (!account_id.empty()) {
    if (account_tracker_service_->GetMigrationState() ==
        AccountTrackerService::MIGRATION_IN_PROGRESS) {
      AccountInfo account_info =
          account_tracker_service_->FindAccountInfoByEmail(account_id);
      // |account_info.gaia| could be empty if |account_id| is already gaia id.
      if (!account_info.gaia.empty()) {
        account_id = account_info.gaia;
        client_->GetPrefs()->SetString(prefs::kGoogleServicesAccountId,
                                       account_id);
      }
    }
    SetAuthenticatedAccountId(account_id);
  }
  FinalizeInitBeforeLoadingRefreshTokens(local_state);
  token_service()->LoadCredentials(GetAuthenticatedAccountId());
}

void SigninManagerBase::FinalizeInitBeforeLoadingRefreshTokens(
    PrefService* local_state) {}

bool SigninManagerBase::IsInitialized() const {
  return initialized_;
}

AccountInfo SigninManagerBase::GetAuthenticatedAccountInfo() const {
  return account_tracker_service_->GetAccountInfo(GetAuthenticatedAccountId());
}

const std::string& SigninManagerBase::GetAuthenticatedAccountId() const {
  return authenticated_account_id_;
}

void SigninManagerBase::SetAuthenticatedAccountInfo(const std::string& gaia_id,
                                                    const std::string& email) {
  DCHECK(!gaia_id.empty());
  DCHECK(!email.empty());

  std::string account_id =
      account_tracker_service_->SeedAccountInfo(gaia_id, email);
  SetAuthenticatedAccountId(account_id);
}

void SigninManagerBase::SetAuthenticatedAccountId(
    const std::string& account_id) {
  DCHECK(!account_id.empty());
  if (!authenticated_account_id_.empty()) {
    DCHECK_EQ(account_id, authenticated_account_id_)
        << "Changing the authenticated account while authenticated is not "
           "allowed.";
    return;
  }

  std::string pref_account_id =
      client_->GetPrefs()->GetString(prefs::kGoogleServicesAccountId);

  DCHECK(pref_account_id.empty() || pref_account_id == account_id)
      << "account_id=" << account_id << " pref_account_id=" << pref_account_id;
  authenticated_account_id_ = account_id;
  client_->GetPrefs()->SetString(prefs::kGoogleServicesAccountId, account_id);

  // This preference is set so that code on I/O thread has access to the
  // Gaia id of the signed in user.
  AccountInfo info = account_tracker_service_->GetAccountInfo(account_id);

  // When this function is called from Initialize(), it's possible for
  // |info.gaia| to be empty when migrating from a really old profile.
  if (!info.gaia.empty()) {
    client_->GetPrefs()->SetString(prefs::kGoogleServicesUserAccountId,
                                   info.gaia);
  }

  // Go ahead and update the last signed in account info here as well. Once a
  // user is signed in the corresponding preferences should match. Doing it here
  // as opposed to on signin allows us to catch the upgrade scenario.
  client_->GetPrefs()->SetString(prefs::kGoogleServicesLastAccountId,
                                 account_id);
  client_->GetPrefs()->SetString(prefs::kGoogleServicesLastUsername,
                                 info.email);

  // Commit authenticated account info immediately so that it does not get lost
  // if Chrome crashes before the next commit interval.
  client_->GetPrefs()->CommitPendingWrite();

  if (observer_) {
    observer_->AuthenticatedAccountSet(info);
  }
}

void SigninManagerBase::ClearAuthenticatedAccountId() {
  authenticated_account_id_.clear();
  if (observer_) {
    observer_->AuthenticatedAccountCleared();
  }
}

bool SigninManagerBase::IsAuthenticated() const {
  return !authenticated_account_id_.empty();
}

void SigninManagerBase::SetObserver(Observer* observer) {
  DCHECK(!observer_) << "SetObserver shouldn't be called multiple times.";
  observer_ = observer;
}

void SigninManagerBase::ClearObserver() {
  DCHECK(observer_);
  observer_ = nullptr;
}

#if !defined(OS_CHROMEOS)
void SigninManagerBase::SignOut(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  RemoveAccountsOption remove_option =
      (account_consistency_ == signin::AccountConsistencyMethod::kDice)
          ? RemoveAccountsOption::kRemoveAuthenticatedAccountIfInError
          : RemoveAccountsOption::kRemoveAllAccounts;
  StartSignOut(signout_source_metric, signout_delete_metric, remove_option);
}

void SigninManagerBase::SignOutAndRemoveAllAccounts(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  StartSignOut(signout_source_metric, signout_delete_metric,
               RemoveAccountsOption::kRemoveAllAccounts);
}

void SigninManagerBase::SignOutAndKeepAllAccounts(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  StartSignOut(signout_source_metric, signout_delete_metric,
               RemoveAccountsOption::kKeepAllAccounts);
}

void SigninManagerBase::StartSignOut(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric,
    RemoveAccountsOption remove_option) {
  signin_client()->PreSignOut(
      base::BindOnce(&SigninManagerBase::OnSignoutDecisionReached,
                     base::Unretained(this), signout_source_metric,
                     signout_delete_metric, remove_option),
      signout_source_metric);
}

void SigninManagerBase::OnSignoutDecisionReached(
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric,
    RemoveAccountsOption remove_option,
    SigninClient::SignoutDecision signout_decision) {
  DCHECK(IsInitialized());

  signin_metrics::LogSignout(signout_source_metric, signout_delete_metric);
  if (!IsAuthenticated()) {
    return;
  }

  // TODO(crbug.com/887756): Consider moving this higher up, or document why
  // the above blocks are exempt from the |signout_decision| early return.
  if (signout_decision == SigninClient::SignoutDecision::DISALLOW_SIGNOUT) {
    DVLOG(1) << "Ignoring attempt to sign out while signout disallowed";
    return;
  }

  AccountInfo account_info = GetAuthenticatedAccountInfo();
  const std::string account_id = GetAuthenticatedAccountId();
  const std::string username = account_info.email;
  const base::Time signin_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::TimeDelta::FromMicroseconds(
          signin_client()->GetPrefs()->GetInt64(prefs::kSignedInTime)));
  ClearAuthenticatedAccountId();
  signin_client()->GetPrefs()->ClearPref(prefs::kGoogleServicesHostedDomain);
  signin_client()->GetPrefs()->ClearPref(prefs::kGoogleServicesAccountId);
  signin_client()->GetPrefs()->ClearPref(prefs::kGoogleServicesUserAccountId);
  signin_client()->GetPrefs()->ClearPref(prefs::kSignedInTime);

  // Determine the duration the user was logged in and log that to UMA.
  if (!signin_time.is_null()) {
    base::TimeDelta signed_in_duration = base::Time::Now() - signin_time;
    UMA_HISTOGRAM_COUNTS_1M("Signin.SignedInDurationBeforeSignout",
                            signed_in_duration.InMinutes());
  }

  // Revoke all tokens before sending signed_out notification, because there
  // may be components that don't listen for token service events when the
  // profile is not connected to an account.
  switch (remove_option) {
    case RemoveAccountsOption::kRemoveAllAccounts:
      VLOG(0) << "Revoking all refresh tokens on server. Reason: sign out";
      token_service()->RevokeAllCredentials(
          signin_metrics::SourceForRefreshTokenOperation::
              kSigninManager_ClearPrimaryAccount);
      break;
    case RemoveAccountsOption::kRemoveAuthenticatedAccountIfInError:
      if (token_service()->RefreshTokenHasError(account_id))
        token_service()->RevokeCredentials(
            account_id, signin_metrics::SourceForRefreshTokenOperation::
                            kSigninManager_ClearPrimaryAccount);
      break;
    case RemoveAccountsOption::kKeepAllAccounts:
      // Do nothing.
      break;
  }

  FireGoogleSignedOut(account_info);
}

void SigninManagerBase::FireGoogleSignedOut(const AccountInfo& account_info) {
  if (observer_ != nullptr) {
    observer_->GoogleSignedOut(account_info);
  }
}
#endif  // !defined(OS_CHROMEOS)
