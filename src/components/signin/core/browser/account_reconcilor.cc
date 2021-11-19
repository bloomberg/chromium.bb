// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_reconcilor.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/signin/core/browser/account_reconcilor_delegate.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"

using signin::AccountReconcilorDelegate;
using signin::ConsentLevel;
using signin_metrics::AccountReconcilorState;

namespace {

// Returns a copy of |accounts| without the unverified accounts.
std::vector<gaia::ListedAccount> FilterUnverifiedAccounts(
    const std::vector<gaia::ListedAccount>& accounts) {
  // Ignore unverified accounts.
  std::vector<gaia::ListedAccount> verified_gaia_accounts;
  std::copy_if(
      accounts.begin(), accounts.end(),
      std::back_inserter(verified_gaia_accounts),
      [](const gaia::ListedAccount& account) { return account.verified; });
  return verified_gaia_accounts;
}

// Revokes tokens for all accounts in chrome_accounts but the primary account.
// Returns true if tokens were revoked, and false if the function did nothing.
bool RevokeAllSecondaryTokens(
    signin::IdentityManager* identity_manager,
    signin::AccountReconcilorDelegate::RevokeTokenOption revoke_option,
    const CoreAccountId& primary_account,
    signin_metrics::SourceForRefreshTokenOperation source) {
  bool token_revoked = false;
  if (revoke_option ==
      AccountReconcilorDelegate::RevokeTokenOption::kDoNotRevoke)
    return false;
  for (const CoreAccountInfo& account_info :
       identity_manager->GetAccountsWithRefreshTokens()) {
    CoreAccountId account = account_info.account_id;
    if (account == primary_account)
      continue;
    bool should_revoke = false;
    switch (revoke_option) {
      case AccountReconcilorDelegate::RevokeTokenOption::kRevokeIfInError:
        if (identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
                account)) {
          VLOG(1) << "Revoke token for " << account;
          should_revoke = true;
        }
        break;
      case AccountReconcilorDelegate::RevokeTokenOption::kRevoke:
        VLOG(1) << "Revoke token for " << account;
        should_revoke = true;
        break;
      case AccountReconcilorDelegate::RevokeTokenOption::kDoNotRevoke:
        NOTREACHED();
        break;
    }
    if (should_revoke) {
      token_revoked = true;
      VLOG(1) << "Revoke token for " << account;
      auto* accounts_mutator = identity_manager->GetAccountsMutator();
      accounts_mutator->RemoveAccount(account, source);
    }
  }
  return token_revoked;
}

// Pick the account will become first after this reconcile is finished.
CoreAccountId PickFirstGaiaAccount(
    const signin::MultiloginParameters& parameters,
    const std::vector<gaia::ListedAccount>& gaia_accounts) {
  if (parameters.mode ==
          gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER &&
      !gaia_accounts.empty()) {
    return gaia_accounts[0].id;
  }
  return parameters.accounts_to_send.empty() ? CoreAccountId()
                                             : parameters.accounts_to_send[0];
}

// Returns true if gaia_accounts contains an invalid account that is unknown to
// the identity manager.
bool HasUnknownInvalidAccountInCookie(
    signin::IdentityManager* identity_manager,
    const std::vector<gaia::ListedAccount>& gaia_accounts) {
  for (const gaia::ListedAccount& account : gaia_accounts) {
    if (!account.valid &&
        !identity_manager->HasAccountWithRefreshToken(account.id)) {
      return true;
    }
  }
  return false;
}

}  // namespace

// static
const char AccountReconcilor::kOperationHistogramName[] =
    "Signin.Reconciler.Operation";

// static
const char AccountReconcilor::kTriggerLogoutHistogramName[] =
    "Signin.Reconciler.Trigger.Logout";

// static
const char AccountReconcilor::kTriggerMultiloginHistogramName[] =
    "Signin.Reconciler.Trigger.Multilogin";

// static
const char AccountReconcilor::kTriggerNoopHistogramName[] =
    "Signin.Reconciler.Trigger.Noop";

// static
const char AccountReconcilor::kTriggerThrottledHistogramName[] =
    "Signin.Reconciler.Trigger.Throttled";

AccountReconcilor::Lock::Lock(AccountReconcilor* reconcilor)
    : reconcilor_(reconcilor->weak_factory_.GetWeakPtr()) {
  DCHECK(reconcilor_);
  reconcilor_->IncrementLockCount();
}

AccountReconcilor::Lock::~Lock() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (reconcilor_)
    reconcilor_->DecrementLockCount();
}

AccountReconcilor::ScopedSyncedDataDeletion::ScopedSyncedDataDeletion(
    AccountReconcilor* reconcilor)
    : reconcilor_(reconcilor->weak_factory_.GetWeakPtr()) {
  DCHECK(reconcilor_);
  ++reconcilor_->synced_data_deletion_in_progress_count_;
}

AccountReconcilor::ScopedSyncedDataDeletion::~ScopedSyncedDataDeletion() {
  if (!reconcilor_)
    return;  // The reconcilor was destroyed.

  DCHECK_GT(reconcilor_->synced_data_deletion_in_progress_count_, 0);
  --reconcilor_->synced_data_deletion_in_progress_count_;
}

AccountReconcilor::AccountReconcilor(
    signin::IdentityManager* identity_manager,
    SigninClient* client,
    std::unique_ptr<signin::AccountReconcilorDelegate> delegate)
    : delegate_(std::move(delegate)),
      identity_manager_(identity_manager),
      client_(client) {
  VLOG(1) << "AccountReconcilor::AccountReconcilor";
  DCHECK(delegate_);
  delegate_->set_reconcilor(this);
  timeout_ = delegate_->GetReconcileTimeout();
}

AccountReconcilor::~AccountReconcilor() {
  VLOG(1) << "AccountReconcilor::~AccountReconcilor";
  // Make sure shutdown was called first.
  DCHECK(WasShutDown());
  DCHECK(!registered_with_identity_manager_);
}

void AccountReconcilor::RegisterWithAllDependencies() {
  RegisterWithContentSettings();
  RegisterWithIdentityManager();
}

void AccountReconcilor::UnregisterWithAllDependencies() {
  UnregisterWithIdentityManager();
  UnregisterWithContentSettings();
}

void AccountReconcilor::Initialize(bool start_reconcile_if_tokens_available) {
  VLOG(1) << "AccountReconcilor::Initialize";
  if (delegate_->IsReconcileEnabled()) {
    SetState(AccountReconcilorState::ACCOUNT_RECONCILOR_SCHEDULED);
    RegisterWithAllDependencies();

    // Start a reconcile if the tokens are already loaded.
    if (start_reconcile_if_tokens_available && IsIdentityManagerReady())
      StartReconcile(Trigger::kInitialized);
  }
}

void AccountReconcilor::EnableReconcile() {
  RegisterWithAllDependencies();
  if (IsIdentityManagerReady())
    StartReconcile(Trigger::kEnableReconcile);
  else
    SetState(AccountReconcilorState::ACCOUNT_RECONCILOR_SCHEDULED);
}

void AccountReconcilor::DisableReconcile(bool logout_all_accounts) {
  AbortReconcile();
  SetState(AccountReconcilorState::ACCOUNT_RECONCILOR_OK);
  UnregisterWithAllDependencies();

  if (logout_all_accounts)
    PerformLogoutAllAccountsAction();
}

void AccountReconcilor::Shutdown() {
  VLOG(1) << "AccountReconcilor::Shutdown";
  if (WasShutDown())
    return;
  was_shut_down_ = true;
  DisableReconcile(false /* logout_all_accounts */);
  delegate_.reset();
  DCHECK(WasShutDown());
}

void AccountReconcilor::RegisterWithContentSettings() {
  VLOG(1) << "AccountReconcilor::RegisterWithContentSettings";
  // During re-auth, the reconcilor will get a callback about successful signin
  // even when the profile is already connected.  Avoid re-registering
  // with the token service since this will DCHECK.
  if (registered_with_content_settings_)
    return;

  client_->AddContentSettingsObserver(this);
  registered_with_content_settings_ = true;
}

void AccountReconcilor::UnregisterWithContentSettings() {
  VLOG(1) << "AccountReconcilor::UnregisterWithContentSettings";
  if (!registered_with_content_settings_)
    return;

  client_->RemoveContentSettingsObserver(this);
  registered_with_content_settings_ = false;
}

void AccountReconcilor::RegisterWithIdentityManager() {
  VLOG(1) << "AccountReconcilor::RegisterWithIdentityManager";
  // During re-auth, the reconcilor will get a callback about successful signin
  // even when the profile is already connected.  Avoid re-registering
  // with the token service since this will DCHECK.
  if (registered_with_identity_manager_)
    return;

  identity_manager_->AddObserver(this);
  registered_with_identity_manager_ = true;
}

void AccountReconcilor::UnregisterWithIdentityManager() {
  VLOG(1) << "AccountReconcilor::UnregisterWithIdentityManager";
  if (!registered_with_identity_manager_)
    return;

  identity_manager_->RemoveObserver(this);
  registered_with_identity_manager_ = false;
}

AccountReconcilorState AccountReconcilor::GetState() {
  return state_;
}

std::unique_ptr<AccountReconcilor::ScopedSyncedDataDeletion>
AccountReconcilor::GetScopedSyncDataDeletion() {
  return base::WrapUnique(new ScopedSyncedDataDeletion(this));
}

void AccountReconcilor::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AccountReconcilor::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AccountReconcilor::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  // If this is not a change to cookie settings, just ignore.
  if (!content_type_set.Contains(ContentSettingsType::COOKIES))
    return;

  // If this does not affect GAIA, just ignore. The secondary pattern is not
  // needed.
  if (!primary_pattern.Matches(GaiaUrls::GetInstance()->gaia_url()))
    return;

  VLOG(1) << "AccountReconcilor::OnContentSettingChanged";
  StartReconcile(Trigger::kCookieSettingChange);
}

void AccountReconcilor::OnEndBatchOfRefreshTokenStateChanges() {
  VLOG(1) << "AccountReconcilor::OnEndBatchOfRefreshTokenStateChanges. "
          << "Reconcilor state: " << is_reconcile_started_;
  // Remember that accounts have changed if a reconcile is already started.
  chrome_accounts_changed_ = is_reconcile_started_;
  StartReconcile(Trigger::kTokenChange);
}

void AccountReconcilor::OnRefreshTokensLoaded() {
  StartReconcile(Trigger::kTokensLoaded);
}

void AccountReconcilor::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error) {
  // Gaia cookies may be invalidated server-side and the client does not get any
  // notification when this happens.
  // Gaia cookies derived from refresh tokens are always invalidated server-side
  // when the tokens are revoked. Trigger a ListAccounts to Gaia when this
  // happens to make sure that the cookies accounts are up-to-date.
  // This should cover well the Mirror and Desktop Identity Consistency cases as
  // the cookies are always bound to the refresh tokens in these cases.
  if (error != GoogleServiceAuthError::AuthErrorNone())
    identity_manager_->GetAccountsCookieMutator()->TriggerCookieJarUpdate();
}

void AccountReconcilor::PerformSetCookiesAction(
    const signin::MultiloginParameters& parameters) {
  reconcile_is_noop_ = false;
  VLOG(1) << "AccountReconcilor::PerformSetCookiesAction: "
          << base::JoinString(ToStringList(parameters.accounts_to_send), " ");
  // TODO (https://crbug.com/890321): pass mode to GaiaCookieManagerService.
  //
  // Using Unretained is safe here because the CookieManagerService outlives
  // the AccountReconcilor.
  identity_manager_->GetAccountsCookieMutator()->SetAccountsInCookie(
      parameters, delegate_->GetGaiaApiSource(),
      base::BindOnce(&AccountReconcilor::OnSetAccountsInCookieCompleted,
                     base::Unretained(this)));
}

void AccountReconcilor::PerformLogoutAllAccountsAction() {
  reconcile_is_noop_ = false;
  VLOG(1) << "AccountReconcilor::PerformLogoutAllAccountsAction";
  identity_manager_->GetAccountsCookieMutator()->LogOutAllAccounts(
      delegate_->GetGaiaApiSource(),
      base::BindOnce(&AccountReconcilor::OnLogOutFromCookieCompleted,
                     weak_factory_.GetWeakPtr()));
}

void AccountReconcilor::StartReconcile(Trigger trigger) {
  if (WasShutDown())
    return;

  if (is_reconcile_started_)
    return;

  if (IsReconcileBlocked()) {
    VLOG(1) << "AccountReconcilor::StartReconcile: "
            << "Reconcile is blocked, scheduling for later.";
    // Reconcile is locked, it will be restarted when the lock count reaches 0.
    reconcile_on_unblock_ = true;
    SetState(AccountReconcilorState::ACCOUNT_RECONCILOR_SCHEDULED);
    return;
  }

  // TODO(crbug.com/967603): remove when root cause is found.
  CHECK(delegate_);
  CHECK(client_);
  if (!delegate_->IsReconcileEnabled() || !client_->AreSigninCookiesAllowed()) {
    VLOG(1) << "AccountReconcilor::StartReconcile: !enabled or no cookies";
    SetState(AccountReconcilorState::ACCOUNT_RECONCILOR_OK);
    return;
  }

  // Do not reconcile if tokens are not loaded yet.
  if (!IsIdentityManagerReady()) {
    SetState(AccountReconcilorState::ACCOUNT_RECONCILOR_SCHEDULED);
    VLOG(1)
        << "AccountReconcilor::StartReconcile: token service *not* ready yet.";
    return;
  }

  // Begin reconciliation. Reset initial states.
  SetState(AccountReconcilorState::ACCOUNT_RECONCILOR_RUNNING);
  reconcile_start_time_ = base::Time::Now();
  is_reconcile_started_ = true;
  error_during_last_reconcile_ = GoogleServiceAuthError::AuthErrorNone();
  reconcile_is_noop_ = true;
  trigger_ = trigger;

  if (!timeout_.is_max()) {
    timer_->Start(FROM_HERE, timeout_,
                  base::BindOnce(&AccountReconcilor::HandleReconcileTimeout,
                                 base::Unretained(this)));
  }

  CoreAccountId account_id = identity_manager_->GetPrimaryAccountId(
      delegate_->GetConsentLevelForPrimaryAccount());
  if (identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id) &&
      delegate_->ShouldAbortReconcileIfPrimaryHasError()) {
    VLOG(1) << "AccountReconcilor::StartReconcile: primary has error, abort.";
    SetState(AccountReconcilorState::ACCOUNT_RECONCILOR_ERROR);
    error_during_last_reconcile_ =
        identity_manager_->GetErrorStateOfRefreshTokenForAccount(account_id);
    AbortReconcile();
    return;
  }

  // Rely on the IdentityManager to manage calls to and responses from
  // ListAccounts.
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar =
      identity_manager_->GetAccountsInCookieJar();
  if (accounts_in_cookie_jar.accounts_are_fresh) {
    OnAccountsInCookieUpdated(
        accounts_in_cookie_jar,
        GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  }
}

void AccountReconcilor::FinishReconcileWithMultiloginEndpoint(
    const CoreAccountId& primary_account,
    const std::vector<CoreAccountId>& chrome_accounts,
    std::vector<gaia::ListedAccount>&& gaia_accounts) {
  DCHECK(!set_accounts_in_progress_);
  DCHECK(!log_out_in_progress_);
  DCHECK_EQ(AccountReconcilorState::ACCOUNT_RECONCILOR_RUNNING, state_);

  bool primary_has_error =
      identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account);

  const signin::MultiloginParameters kLogoutParameters(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      std::vector<CoreAccountId>());

  const bool should_revoke_tokens =
      delegate_->ShouldRevokeTokensBeforeMultilogin(
          chrome_accounts, primary_account, gaia_accounts, first_execution_,
          primary_has_error);

  DCHECK(is_reconcile_started_);
  signin::MultiloginParameters parameters_for_multilogin;
  if (should_revoke_tokens) {
    // Set parameters for logout for deleting cookies.
    parameters_for_multilogin = kLogoutParameters;
    RevokeAllSecondaryTokens(
        identity_manager_,
        AccountReconcilorDelegate::RevokeTokenOption::kRevoke, primary_account,
        signin_metrics::SourceForRefreshTokenOperation::
            kAccountReconcilor_Reconcile);
  } else {
    parameters_for_multilogin = delegate_->CalculateParametersForMultilogin(
        chrome_accounts, primary_account, gaia_accounts, first_execution_,
        primary_has_error);
  }
  if (CookieNeedsUpdate(parameters_for_multilogin, gaia_accounts)) {
    // Verify the account reconcilor is not trapped into a loop of repeating the
    // same request with the same params.
    if (throttler_.TryMultiloginOperation(parameters_for_multilogin)) {
      if (parameters_for_multilogin == kLogoutParameters) {
        RecordReconcileOperation(trigger_, Operation::kLogout);
        // UPDATE mode does not support empty list of accounts, call logout
        // instead.
        log_out_in_progress_ = true;
        PerformLogoutAllAccountsAction();
      } else {
        // Reconcilor has to do some calls to gaia. is_reconcile_started_ is
        // true and any StartReconcile() calls that are made in the meantime
        // will be aborted until OnSetAccountsInCookieCompleted is called and
        // is_reconcile_started_ is set to false.
        RecordReconcileOperation(trigger_, Operation::kMultilogin);
        set_accounts_in_progress_ = true;
        PerformSetCookiesAction(parameters_for_multilogin);
      }
    } else {
      // Too many requests with the same parameters led to a backoff time
      // required between successive identical requests that has not yet passed.
      error_during_last_reconcile_ =
          GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
      CalculateIfMultiloginReconcileIsDone();
      ScheduleStartReconcileIfChromeAccountsChanged();
      RecordReconcileOperation(trigger_, Operation::kThrottled);
    }
  } else {
    // Nothing to do, accounts already match.
    RecordReconcileOperation(trigger_, Operation::kNoop);
    throttler_.Reset();
    error_during_last_reconcile_ = GoogleServiceAuthError::AuthErrorNone();
    CalculateIfMultiloginReconcileIsDone();

    // TODO(droger): investigate if |is_reconcile_started_| is still needed for
    // multilogin.

    // This happens only when reconcile doesn't make any changes (i.e. the state
    // is consistent). If it is not the case, second reconcile is expected to be
    // triggered after changes are made. For that one the state is supposed to
    // be already consistent.
    DCHECK(!is_reconcile_started_);
    DCHECK_NE(AccountReconcilorState::ACCOUNT_RECONCILOR_OK, state_);

    CoreAccountId first_gaia_account_after_reconcile =
        PickFirstGaiaAccount(parameters_for_multilogin, gaia_accounts);
    delegate_->OnReconcileFinished(first_gaia_account_after_reconcile);
    ScheduleStartReconcileIfChromeAccountsChanged();
  }

  signin_metrics::RecordAccountsPerProfile(chrome_accounts.size());
  first_execution_ = false;
}

void AccountReconcilor::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  const std::vector<gaia::ListedAccount>& accounts(
      accounts_in_cookie_jar_info.signed_in_accounts);
  VLOG(1) << "AccountReconcilor::OnAccountsInCookieUpdated: "
          << "CookieJar " << accounts.size() << " accounts, "
          << "Reconcilor's state is " << is_reconcile_started_ << ", "
          << "Error was " << error.ToString();

  // If cookies change while the reconcilor is running, ignore the changes and
  // let it complete. Adding accounts to the cookie will trigger new
  // notifications anyway, and these will be handled in a new reconciliation
  // cycle. See https://crbug.com/923716
  //
  // TODO(droger): Should we also check if |logout_in_progress_|?
  if (set_accounts_in_progress_)
    return;

  if (!is_reconcile_started_) {
    StartReconcile(Trigger::kCookieChange);
    return;
  }

  if (error.state() != GoogleServiceAuthError::NONE) {
    // We may have seen a series of errors during reconciliation. Delegates may
    // rely on the severity of the last seen error (see |OnReconcileError|) and
    // hence do not override a persistent error, if we have seen one.
    if (!error_during_last_reconcile_.IsPersistentError())
      error_during_last_reconcile_ = error;
    SetState(AccountReconcilorState::ACCOUNT_RECONCILOR_ERROR);
    AbortReconcile();
    return;
  }

  std::vector<gaia::ListedAccount> verified_gaia_accounts =
      FilterUnverifiedAccounts(accounts);
  VLOG_IF(1, verified_gaia_accounts.size() < accounts.size())
      << "Ignore " << accounts.size() - verified_gaia_accounts.size()
      << " unverified account(s).";

  ConsentLevel consent_level = delegate_->GetConsentLevelForPrimaryAccount();
  CoreAccountId primary_account =
      identity_manager_->GetPrimaryAccountId(consent_level);

  // Revoking tokens for secondary accounts causes the AccountTracker to
  // completely remove them from Chrome.
  // Revoking the token for the primary account is not supported (it should be
  // signed out or put to auth error state instead).
  // TODO(https://crbug.com/1122551): Move to |DiceAccountReconcilorDelegate|.
  AccountReconcilorDelegate::RevokeTokenOption revoke_option =
      delegate_->ShouldRevokeSecondaryTokensBeforeReconcile(
          verified_gaia_accounts);
  RevokeAllSecondaryTokens(identity_manager_, revoke_option, primary_account,
                           signin_metrics::SourceForRefreshTokenOperation::
                               kAccountReconcilor_GaiaCookiesUpdated);

  std::vector<CoreAccountId> chrome_accounts =
      LoadValidAccountsFromTokenService();

  if (delegate_->ShouldAbortReconcileIfPrimaryHasError() &&
      !base::Contains(chrome_accounts, primary_account)) {
    VLOG(1) << "Primary account has error, abort.";
    DCHECK(is_reconcile_started_);
    AbortReconcile();
    SetState(AccountReconcilorState::ACCOUNT_RECONCILOR_ERROR);
    return;
  }

  FinishReconcileWithMultiloginEndpoint(primary_account, chrome_accounts,
                                        std::move(verified_gaia_accounts));
}

void AccountReconcilor::OnAccountsCookieDeletedByUserAction() {
  if (!delegate_->ShouldRevokeTokensOnCookieDeleted())
    return;

  // This code is only used with DiceAccountReconcilorDelegate and should thus
  // use sync account.
  // TODO(https://crbug.com/1122551): Move to |DiceAccountReconcilorDelegate|.
  DCHECK_EQ(delegate_->GetConsentLevelForPrimaryAccount(), ConsentLevel::kSync);

  CoreAccountId primary_account =
      identity_manager_->GetPrimaryAccountId(ConsentLevel::kSync);
  // Revoke secondary tokens.
  RevokeAllSecondaryTokens(
      identity_manager_, AccountReconcilorDelegate::RevokeTokenOption::kRevoke,
      primary_account,
      signin_metrics::SourceForRefreshTokenOperation::
          kAccountReconcilor_GaiaCookiesDeletedByUser);
  if (primary_account.empty())
    return;
  if (identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account) ||
      synced_data_deletion_in_progress_count_ == 0) {
    // Invalidate the primary token, but do not revoke it.
    auto* accounts_mutator = identity_manager_->GetAccountsMutator();
    accounts_mutator->InvalidateRefreshTokenForPrimaryAccount(
        signin_metrics::SourceForRefreshTokenOperation::
            kAccountReconcilor_GaiaCookiesDeletedByUser);
  }
}

std::vector<CoreAccountId>
AccountReconcilor::LoadValidAccountsFromTokenService() const {
  auto chrome_accounts_with_refresh_tokens =
      identity_manager_->GetAccountsWithRefreshTokens();

  std::vector<CoreAccountId> chrome_account_ids;

  // Remove any accounts that have an error. There is no point in trying to
  // reconcile them, since it won't work anyway. If the list ends up being
  // empty then don't reconcile any accounts.
  for (const auto& chrome_account_with_refresh_tokens :
       chrome_accounts_with_refresh_tokens) {
    if (identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
            chrome_account_with_refresh_tokens.account_id)) {
      VLOG(1) << "AccountReconcilor::LoadValidAccountsFromTokenService: "
              << chrome_account_with_refresh_tokens.account_id
              << " has error, don't reconcile";
      continue;
    }
    chrome_account_ids.push_back(chrome_account_with_refresh_tokens.account_id);
  }

  VLOG(1) << "AccountReconcilor::LoadValidAccountsFromTokenService: "
          << "Chrome " << chrome_account_ids.size() << " accounts";

  return chrome_account_ids;
}

void AccountReconcilor::OnReceivedManageAccountsResponse(
    signin::GAIAServiceType service_type) {
#if !defined(OS_CHROMEOS)
  // TODO(https://crbug.com/1224872): check if it's still required on Android
  // and iOS.
  if (service_type == signin::GAIA_SERVICE_TYPE_ADDSESSION) {
    identity_manager_->GetAccountsCookieMutator()->TriggerCookieJarUpdate();
  }
#endif  // !defined(OS_CHROMEOS)
}

void AccountReconcilor::AbortReconcile() {
  VLOG(1) << "AccountReconcilor::AbortReconcile: try again later";
  log_out_in_progress_ = false;
  set_accounts_in_progress_ = false;
  CalculateIfMultiloginReconcileIsDone();
  DCHECK(!is_reconcile_started_);
  DCHECK(!timer_->IsRunning());
}

void AccountReconcilor::ScheduleStartReconcileIfChromeAccountsChanged() {
  if (is_reconcile_started_)
    return;

  if (GetState() == AccountReconcilorState::ACCOUNT_RECONCILOR_SCHEDULED)
    return;

  // Start a reconcile as the token accounts have changed.
  VLOG(1) << "AccountReconcilor::StartReconcileIfChromeAccountsChanged";
  if (chrome_accounts_changed_) {
    chrome_accounts_changed_ = false;
    SetState(AccountReconcilorState::ACCOUNT_RECONCILOR_SCHEDULED);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AccountReconcilor::StartReconcile,
                                  base::Unretained(this),
                                  Trigger::kTokenChangeDuringReconcile));
  } else if (error_during_last_reconcile_.state() ==
             GoogleServiceAuthError::NONE) {
    SetState(AccountReconcilorState::ACCOUNT_RECONCILOR_OK);
  } else {
    SetState(AccountReconcilorState::ACCOUNT_RECONCILOR_ERROR);
  }
}

bool AccountReconcilor::IsIdentityManagerReady() {
  return identity_manager_->AreRefreshTokensLoaded();
}

void AccountReconcilor::OnSetAccountsInCookieCompleted(
    signin::SetAccountsInCookieResult result) {
  VLOG(1) << "AccountReconcilor::OnSetAccountsInCookieCompleted: "
          << "Error was " << static_cast<int>(result);

  if (!set_accounts_in_progress_ || !is_reconcile_started_)
    return;
  set_accounts_in_progress_ = false;
  switch (result) {
    case signin::SetAccountsInCookieResult::kSuccess:
      error_during_last_reconcile_ = GoogleServiceAuthError::AuthErrorNone();
      break;
    case signin::SetAccountsInCookieResult::kTransientError:
      if (!error_during_last_reconcile_.IsPersistentError())
        error_during_last_reconcile_ =
            GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED);
      break;
    case signin::SetAccountsInCookieResult::kPersistentError:
      error_during_last_reconcile_ =
          GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_ERROR);
      break;
  }
  CalculateIfMultiloginReconcileIsDone();
  ScheduleStartReconcileIfChromeAccountsChanged();
}

void AccountReconcilor::CalculateIfMultiloginReconcileIsDone() {
  DCHECK(!set_accounts_in_progress_);
  DCHECK(!log_out_in_progress_);
  VLOG(1) << "AccountReconcilor::CalculateIfMultiloginReconcileIsDone: "
          << "Error was " << error_during_last_reconcile_.ToString();

  if (!is_reconcile_started_)
    return;

  bool was_last_reconcile_successful = error_during_last_reconcile_.state() ==
                                       GoogleServiceAuthError::State::NONE;

  if (!was_last_reconcile_successful)
    delegate_->OnReconcileError(error_during_last_reconcile_);

  is_reconcile_started_ = false;
  timer_->Stop();
  base::TimeDelta duration = base::Time::Now() - reconcile_start_time_;
  signin_metrics::LogSigninAccountReconciliationDuration(
      duration, was_last_reconcile_successful);
}

void AccountReconcilor::OnLogOutFromCookieCompleted(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "AccountReconcilor::OnLogOutFromCookieCompleted: "
          << "Error was " << error.ToString();

  // When switching the primary account, there is a sequence of calls to
  // DisableReconclie() followed by EnableReconcile(). This starts a logout call
  // and then starts a reconcile loop without waiting for the initial logout
  // to end. The initial logout should not be considered as the end of the
  // reconcile loop. See crbug.com/1175395
  if (is_reconcile_started_ && log_out_in_progress_) {
    log_out_in_progress_ = false;
    if (error.state() != GoogleServiceAuthError::State::NONE &&
        !error_during_last_reconcile_.IsPersistentError()) {
      error_during_last_reconcile_ = error;
    }
    CalculateIfMultiloginReconcileIsDone();
    ScheduleStartReconcileIfChromeAccountsChanged();
  }
}

void AccountReconcilor::IncrementLockCount() {
  DCHECK_GE(account_reconcilor_lock_count_, 0);
  ++account_reconcilor_lock_count_;
  if (account_reconcilor_lock_count_ == 1)
    BlockReconcile();
}

void AccountReconcilor::DecrementLockCount() {
  DCHECK_GT(account_reconcilor_lock_count_, 0);
  --account_reconcilor_lock_count_;
  if (account_reconcilor_lock_count_ == 0)
    UnblockReconcile();
}

bool AccountReconcilor::IsReconcileBlocked() const {
  DCHECK_GE(account_reconcilor_lock_count_, 0);
  return account_reconcilor_lock_count_ > 0;
}

void AccountReconcilor::BlockReconcile() {
  DCHECK(IsReconcileBlocked());
  VLOG(1) << "AccountReconcilor::BlockReconcile.";
  if (is_reconcile_started_) {
    AbortReconcile();
    SetState(AccountReconcilorState::ACCOUNT_RECONCILOR_SCHEDULED);
    reconcile_on_unblock_ = true;
  }
  for (auto& observer : observer_list_)
    observer.OnBlockReconcile();
}

void AccountReconcilor::UnblockReconcile() {
  DCHECK(!IsReconcileBlocked());
  VLOG(1) << "AccountReconcilor::UnblockReconcile.";
  for (auto& observer : observer_list_)
    observer.OnUnblockReconcile();
  if (reconcile_on_unblock_) {
    reconcile_on_unblock_ = false;
    StartReconcile(Trigger::kUnblockReconcile);
  }
}

void AccountReconcilor::set_timer_for_testing(
    std::unique_ptr<base::OneShotTimer> timer) {
  timer_ = std::move(timer);
}

void AccountReconcilor::HandleReconcileTimeout() {
  // A reconciliation was still succesfully in progress but could not complete
  // in the given time. For a delegate, this is equivalent to a
  // |GoogleServiceAuthError::State::CONNECTION_FAILED|.
  if (error_during_last_reconcile_.state() ==
      GoogleServiceAuthError::State::NONE) {
    error_during_last_reconcile_ = GoogleServiceAuthError(
        GoogleServiceAuthError::State::CONNECTION_FAILED);
  }

  // Will stop reconciliation and inform |delegate_| about
  // |error_during_last_reconcile_|, through
  // |CalculateIfReconcileIsDone|.
  AbortReconcile();
  DCHECK(!timer_->IsRunning());
}

bool AccountReconcilor::CookieNeedsUpdate(
    const signin::MultiloginParameters& parameters,
    const std::vector<gaia::ListedAccount>& existing_accounts) {
  bool should_remove_unknown_account =
      !delegate_->IsUnknownInvalidAccountInCookieAllowed() &&
      HasUnknownInvalidAccountInCookie(identity_manager_, existing_accounts);
  if (should_remove_unknown_account) {
    // Removing unknown accounts in the cookie is only supported for UPDATE
    // mode.
    DCHECK_EQ(parameters.mode,
              gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER);
    return true;
  }

  if (parameters.mode ==
          gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER &&
      !existing_accounts.empty() && !parameters.accounts_to_send.empty() &&
      existing_accounts[0].id != parameters.accounts_to_send[0]) {
    // In UPDATE mode update is needed if first accounts don't match.
    return true;
  }

  // Maybe some accounts in cookies are not valid and need refreshing.
  std::set<CoreAccountId> accounts_to_send_set(
      parameters.accounts_to_send.begin(), parameters.accounts_to_send.end());
  std::set<CoreAccountId> existing_accounts_set;
  for (const gaia::ListedAccount& account : existing_accounts) {
    if (account.valid)
      existing_accounts_set.insert(account.id);
  }
  return (existing_accounts_set != accounts_to_send_set);
}

void AccountReconcilor::SetState(AccountReconcilorState state) {
  if (state == state_)
    return;

  state_ = state;
  for (auto& observer : observer_list_)
    observer.OnStateChanged(state_);
}

bool AccountReconcilor::WasShutDown() const {
  return was_shut_down_;
}

// static
void AccountReconcilor::RecordReconcileOperation(Trigger trigger,
                                                 Operation operation) {
  // Using the histogram macro for histogram that may be recorded in a loop.
  UMA_HISTOGRAM_ENUMERATION(kOperationHistogramName, operation);
  switch (operation) {
    case Operation::kNoop:
      base::UmaHistogramEnumeration(kTriggerNoopHistogramName, trigger);
      break;
    case Operation::kLogout:
      base::UmaHistogramEnumeration(kTriggerLogoutHistogramName, trigger);
      break;
    case Operation::kMultilogin:
      base::UmaHistogramEnumeration(kTriggerMultiloginHistogramName, trigger);
      break;
    case Operation::kThrottled:
      UMA_HISTOGRAM_ENUMERATION(kTriggerThrottledHistogramName, trigger);
      break;
  }
}
