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
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/signin/core/browser/account_consistency_method.h"
#include "components/signin/core/browser/account_reconcilor_delegate.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/identity/public/cpp/accounts_cookie_mutator.h"
#include "services/identity/public/cpp/accounts_in_cookie_jar_info.h"
#include "services/identity/public/cpp/accounts_mutator.h"

using signin::AccountReconcilorDelegate;

const base::Feature kUseMultiloginEndpoint{"UseMultiloginEndpoint",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

namespace {

class AccountEqualToFunc {
 public:
  explicit AccountEqualToFunc(const gaia::ListedAccount& account)
      : account_(account) {}
  bool operator()(const gaia::ListedAccount& other) const;

 private:
  gaia::ListedAccount account_;
};

bool AccountEqualToFunc::operator()(const gaia::ListedAccount& other) const {
  return account_.valid == other.valid && account_.id == other.id;
}

gaia::ListedAccount AccountForId(const std::string& account_id) {
  gaia::ListedAccount account;
  account.id = account_id;
  return account;
}

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
    identity::IdentityManager* identity_manager,
    signin::AccountReconcilorDelegate::RevokeTokenOption revoke_option,
    const std::string& primary_account,
    bool is_account_consistency_enforced,
    signin_metrics::SourceForRefreshTokenOperation source) {
  bool token_revoked = false;
  if (revoke_option ==
      AccountReconcilorDelegate::RevokeTokenOption::kDoNotRevoke)
    return false;
  for (const AccountInfo& account_info :
       identity_manager->GetAccountsWithRefreshTokens()) {
    std::string account(account_info.account_id);
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
        if (is_account_consistency_enforced) {
          should_revoke = true;
        }
        break;
      case AccountReconcilorDelegate::RevokeTokenOption::kDoNotRevoke:
        NOTREACHED();
        break;
    }
    if (should_revoke) {
      token_revoked = true;
      VLOG(1) << "Revoke token for " << account;
      if (is_account_consistency_enforced) {
        auto* accounts_mutator = identity_manager->GetAccountsMutator();
        accounts_mutator->RemoveAccount(account, source);
      }
    }
  }
  return token_revoked;
}

// Returns true if current array of existing accounts in cookie is different
// from the desired one. If this returns false, the multilogin call would be a
// no-op.
bool AccountsNeedUpdate(
    const signin::MultiloginParameters& parameters,
    const std::vector<gaia::ListedAccount>& existing_accounts) {
  if (parameters.mode ==
          gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER &&
      !existing_accounts.empty() && !parameters.accounts_to_send.empty() &&
      existing_accounts[0].id != parameters.accounts_to_send[0]) {
    // In UPDATE mode update is needed if first accounts don't match.
    return true;
  }
  // Maybe some accounts in cookies are not valid and need refreshing.
  std::set<std::string> accounts_to_send_set(
      parameters.accounts_to_send.begin(), parameters.accounts_to_send.end());
  std::set<std::string> existing_accounts_set;
  for (const gaia::ListedAccount& account : existing_accounts) {
    if (account.valid)
      existing_accounts_set.insert(account.id);
  }
  return (existing_accounts_set != accounts_to_send_set);
}

// Pick the account will become first after this reconcile is finished.
std::string PickFirstGaiaAccount(
    const signin::MultiloginParameters& parameters,
    const std::vector<gaia::ListedAccount>& gaia_accounts) {
  if (parameters.mode ==
          gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER &&
      !gaia_accounts.empty()) {
    return gaia_accounts[0].id;
  }
  return parameters.accounts_to_send.empty() ? ""
                                             : parameters.accounts_to_send[0];
}

}  // namespace

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
    identity::IdentityManager* identity_manager,
    SigninClient* client,
    std::unique_ptr<signin::AccountReconcilorDelegate> delegate)
    : delegate_(std::move(delegate)),
      identity_manager_(identity_manager),
      client_(client),
      registered_with_identity_manager_(false),
      registered_with_content_settings_(false),
      is_reconcile_started_(false),
      first_execution_(true),
      error_during_last_reconcile_(GoogleServiceAuthError::AuthErrorNone()),
      reconcile_is_noop_(true),
      set_accounts_in_progress_(false),
      chrome_accounts_changed_(false),
      account_reconcilor_lock_count_(0),
      reconcile_on_unblock_(false),
      timer_(new base::OneShotTimer),
      weak_factory_(this) {
  VLOG(1) << "AccountReconcilor::AccountReconcilor";
  DCHECK(delegate_);
  delegate_->set_reconcilor(this);
  timeout_ = delegate_->GetReconcileTimeout();
}

AccountReconcilor::~AccountReconcilor() {
  VLOG(1) << "AccountReconcilor::~AccountReconcilor";
  // Make sure shutdown was called first.
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
    RegisterWithAllDependencies();

    // Start a reconcile if the tokens are already loaded.
    if (start_reconcile_if_tokens_available && IsIdentityManagerReady())
      StartReconcile();
  }
}

#if defined(OS_IOS)
void AccountReconcilor::SetIsWKHTTPSystemCookieStoreEnabled(bool is_enabled) {
  is_wkhttp_system_cookie_store_enabled_ = is_enabled;
}
#endif  // defined(OS_IOS)

void AccountReconcilor::EnableReconcile() {
  RegisterWithAllDependencies();
#if !defined(OS_IOS)
  // TODO(droger): Investigate why this breaks tests on iOS.
  if (IsIdentityManagerReady())
    StartReconcile();
#endif  // !defined(OS_IOS)
}

void AccountReconcilor::DisableReconcile(bool logout_all_accounts) {
  AbortReconcile();
  UnregisterWithAllDependencies();

  if (logout_all_accounts)
    PerformLogoutAllAccountsAction();
}

void AccountReconcilor::Shutdown() {
  VLOG(1) << "AccountReconcilor::Shutdown";
  DisableReconcile(false /* logout_all_accounts */);
  delegate_.reset();
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

signin_metrics::AccountReconcilorState AccountReconcilor::GetState() {
  if (!is_reconcile_started_) {
    return (error_during_last_reconcile_.state() !=
            GoogleServiceAuthError::State::NONE)
               ? signin_metrics::ACCOUNT_RECONCILOR_ERROR
               : signin_metrics::ACCOUNT_RECONCILOR_OK;
  }

  return signin_metrics::ACCOUNT_RECONCILOR_RUNNING;
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
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  // If this is not a change to cookie settings, just ignore.
  if (content_type != CONTENT_SETTINGS_TYPE_COOKIES)
    return;

  // If this does not affect GAIA, just ignore.  If the primary pattern is
  // invalid, then assume it could affect GAIA.  The secondary pattern is
  // not needed.
  if (primary_pattern.IsValid() &&
      !primary_pattern.Matches(GaiaUrls::GetInstance()->gaia_url())) {
    return;
  }

  VLOG(1) << "AccountReconcilor::OnContentSettingChanged";
  StartReconcile();
}

void AccountReconcilor::OnEndBatchOfRefreshTokenStateChanges() {
  VLOG(1) << "AccountReconcilor::OnEndBatchOfRefreshTokenStateChanges. "
          << "Reconcilor state: " << is_reconcile_started_;
  // Remember that accounts have changed if a reconcile is already started.
  chrome_accounts_changed_ = is_reconcile_started_;
  StartReconcile();
}

void AccountReconcilor::OnRefreshTokensLoaded() {
  StartReconcile();
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

void AccountReconcilor::PerformMergeAction(const std::string& account_id) {
  reconcile_is_noop_ = false;
  if (!delegate_->IsAccountConsistencyEnforced()) {
    MarkAccountAsAddedToCookie(account_id);
    return;
  }
  VLOG(1) << "AccountReconcilor::PerformMergeAction: " << account_id;
  identity_manager_->GetAccountsCookieMutator()->AddAccountToCookie(
      account_id, delegate_->GetGaiaApiSource(),
      base::BindOnce(&AccountReconcilor::OnAddAccountToCookieCompleted,
                     weak_factory_.GetWeakPtr()));
}

void AccountReconcilor::PerformSetCookiesAction(
    const signin::MultiloginParameters& parameters) {
  reconcile_is_noop_ = false;
  if (!delegate_->IsAccountConsistencyEnforced()) {
    OnSetAccountsInCookieCompleted(GoogleServiceAuthError::AuthErrorNone());
    return;
  }
  VLOG(1) << "AccountReconcilor::PerformSetCookiesAction: "
          << base::JoinString(parameters.accounts_to_send, " ");
  // TODO (https://crbug.com/890321): pass mode to GaiaCookieManagerService.
  //
  // Using Unretained is safe here because the CookieManagerService outlives
  // the AccountReconcilor.
  identity_manager_->GetAccountsCookieMutator()->SetAccountsInCookie(
      parameters.accounts_to_send, delegate_->GetGaiaApiSource(),
      base::BindOnce(&AccountReconcilor::OnSetAccountsInCookieCompleted,
                     base::Unretained(this)));
}

void AccountReconcilor::PerformLogoutAllAccountsAction() {
  reconcile_is_noop_ = false;
  if (!delegate_->IsAccountConsistencyEnforced())
    return;
  VLOG(1) << "AccountReconcilor::PerformLogoutAllAccountsAction";
  identity_manager_->GetAccountsCookieMutator()->LogOutAllAccounts(
      delegate_->GetGaiaApiSource());
}

void AccountReconcilor::StartReconcile() {
  if (is_reconcile_started_)
    return;

  if (IsReconcileBlocked()) {
    VLOG(1) << "AccountReconcilor::StartReconcile: "
            << "Reconcile is blocked, scheduling for later.";
    // Reconcile is locked, it will be restarted when the lock count reaches 0.
    reconcile_on_unblock_ = true;
    return;
  }

  if (!delegate_->IsReconcileEnabled() || !client_->AreSigninCookiesAllowed()) {
    VLOG(1) << "AccountReconcilor::StartReconcile: !enabled or no cookies";
    return;
  }

  // Do not reconcile if tokens are not loaded yet.
  if (!IsIdentityManagerReady()) {
    VLOG(1)
        << "AccountReconcilor::StartReconcile: token service *not* ready yet.";
    return;
  }

  // Begin reconciliation. Reset initial states.
  for (auto& observer : observer_list_)
    observer.OnStartReconcile();
  add_to_cookie_.clear();
  reconcile_start_time_ = base::Time::Now();
  is_reconcile_started_ = true;
  error_during_last_reconcile_ = GoogleServiceAuthError::AuthErrorNone();
  reconcile_is_noop_ = true;

  if (!timeout_.is_max()) {
    timer_->Start(FROM_HERE, timeout_,
                  base::BindOnce(&AccountReconcilor::HandleReconcileTimeout,
                                 base::Unretained(this)));
  }

  const std::string& account_id = identity_manager_->GetPrimaryAccountId();
  if (identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id) &&
      delegate_->ShouldAbortReconcileIfPrimaryHasError()) {
    VLOG(1) << "AccountReconcilor::StartReconcile: primary has error, abort.";
    error_during_last_reconcile_ =
        identity_manager_->GetErrorStateOfRefreshTokenForAccount(account_id);
    AbortReconcile();
    return;
  }

  // Rely on the IdentityManager to manage calls to and responses from
  // ListAccounts.
  identity::AccountsInCookieJarInfo accounts_in_cookie_jar =
      identity_manager_->GetAccountsInCookieJar();
  if (accounts_in_cookie_jar.accounts_are_fresh) {
    OnAccountsInCookieUpdated(
        accounts_in_cookie_jar,
        GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  }
}

void AccountReconcilor::FinishReconcileWithMultiloginEndpoint(
    const std::string& primary_account,
    const std::vector<std::string>& chrome_accounts,
    std::vector<gaia::ListedAccount>&& gaia_accounts) {
  DCHECK(base::FeatureList::IsEnabled(kUseMultiloginEndpoint));
  DCHECK(!set_accounts_in_progress_);

  bool primary_has_error =
      identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account);

  const signin::MultiloginParameters parameters_for_multilogin =
      delegate_->CalculateParametersForMultilogin(
          chrome_accounts, primary_account, gaia_accounts, first_execution_,
          primary_has_error);

  DCHECK(is_reconcile_started_);
  if (AccountsNeedUpdate(parameters_for_multilogin, gaia_accounts)) {
    if (parameters_for_multilogin.mode ==
            gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER &&
        parameters_for_multilogin.accounts_to_send.empty()) {
      // UPDATE mode does not support empty list of accounts, call logout
      // instead.
      PerformLogoutAllAccountsAction();
      gaia_accounts.clear();
      OnSetAccountsInCookieCompleted(GoogleServiceAuthError::AuthErrorNone());
      DCHECK(!is_reconcile_started_);
    } else {
      // Reconcilor has to do some calls to gaia. is_reconcile_started_ is true
      // and any StartReconcile() calls that are made in the meantime will be
      // aborted until OnSetAccountsInCookieCompleted is called and
      // is_reconcile_started_ is set to false.
      set_accounts_in_progress_ = true;
      PerformSetCookiesAction(parameters_for_multilogin);
      DCHECK(is_reconcile_started_);
    }
  } else {
    // Nothing to do, accounts already match.
    OnSetAccountsInCookieCompleted(GoogleServiceAuthError::AuthErrorNone());
    DCHECK(!is_reconcile_started_);
  }

  signin_metrics::RecordAccountsPerProfile(chrome_accounts.size());
  if (!is_reconcile_started_) {
    // TODO(droger): investigate if |is_reconcile_started_| is still needed for
    // multilogin.

    // This happens only when reconcile doesn't make any changes (i.e. the state
    // is consistent). If it is not the case, second reconcile is expected to be
    // triggered after changes are made. For that one the state is supposed to
    // be already consistent.
    DCHECK(!AccountsNeedUpdate(parameters_for_multilogin, gaia_accounts));
    std::string first_gaia_account_after_reconcile =
        PickFirstGaiaAccount(parameters_for_multilogin, gaia_accounts);
    delegate_->OnReconcileFinished(first_gaia_account_after_reconcile,
                                   reconcile_is_noop_);
  }
  first_execution_ = false;
}

void AccountReconcilor::OnAccountsInCookieUpdated(
    const identity::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
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
  if (IsMultiloginEndpointEnabled()) {
    if (set_accounts_in_progress_)
      return;
  } else {
    if (!add_to_cookie_.empty())
      return;
  }

  if (error.state() != GoogleServiceAuthError::NONE) {
    // We may have seen a series of errors during reconciliation. Delegates may
    // rely on the severity of the last seen error (see |OnReconcileError|) and
    // hence do not override a persistent error, if we have seen one.
    if (is_reconcile_started_ &&
        !error_during_last_reconcile_.IsPersistentError()) {
      error_during_last_reconcile_ = error;
    }
    AbortReconcile();
    return;
  }

  if (!is_reconcile_started_) {
    StartReconcile();
    return;
  }

  std::vector<gaia::ListedAccount> verified_gaia_accounts =
      FilterUnverifiedAccounts(accounts);
  VLOG_IF(1, verified_gaia_accounts.size() < accounts.size())
      << "Ignore " << accounts.size() - verified_gaia_accounts.size()
      << " unverified account(s).";

  std::string primary_account = identity_manager_->GetPrimaryAccountId();
  // Revoking tokens for secondary accounts causes the AccountTracker to
  // completely remove them from Chrome.
  // Revoking the token for the primary account is not supported (it should be
  // signed out or put to auth error state instead).
  AccountReconcilorDelegate::RevokeTokenOption revoke_option =
      delegate_->ShouldRevokeSecondaryTokensBeforeReconcile(
          verified_gaia_accounts);
  RevokeAllSecondaryTokens(identity_manager_, revoke_option, primary_account,
                           true,
                           signin_metrics::SourceForRefreshTokenOperation::
                               kAccountReconcilor_GaiaCookiesUpdated);

  std::vector<std::string> chrome_accounts =
      LoadValidAccountsFromTokenService();

  if (delegate_->ShouldAbortReconcileIfPrimaryHasError() &&
      !base::ContainsValue(chrome_accounts, primary_account)) {
    VLOG(1) << "Primary account has error, abort.";
    DCHECK(is_reconcile_started_);
    AbortReconcile();
    return;
  }

  if (IsMultiloginEndpointEnabled()) {
    FinishReconcileWithMultiloginEndpoint(primary_account, chrome_accounts,
                                          std::move(verified_gaia_accounts));
  } else {
    FinishReconcile(primary_account, chrome_accounts,
                    std::move(verified_gaia_accounts));
  }
}

void AccountReconcilor::OnAccountsCookieDeletedByUserAction() {
  if (!delegate_->ShouldRevokeTokensOnCookieDeleted())
    return;

  const std::string& primary_account = identity_manager_->GetPrimaryAccountId();
  // Revoke secondary tokens.
  RevokeAllSecondaryTokens(
      identity_manager_, AccountReconcilorDelegate::RevokeTokenOption::kRevoke,
      primary_account, /*account_consistency_enforced=*/true,
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

std::vector<std::string> AccountReconcilor::LoadValidAccountsFromTokenService()
    const {
  auto chrome_accounts_with_refresh_tokens =
      identity_manager_->GetAccountsWithRefreshTokens();

  std::vector<std::string> chrome_account_ids;

  // Remove any accounts that have an error.  There is no point in trying to
  // reconcile them, since it won't work anyway.  If the list ends up being
  // empty then don't reconcile any accounts.
  for (const auto& chrome_account_with_refresh_tokens :
       chrome_accounts_with_refresh_tokens) {
    if (identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
            chrome_account_with_refresh_tokens.account_id)) {
      VLOG(1) << "AccountReconcilor::ValidateAccountsFromTokenService: "
              << chrome_account_with_refresh_tokens.account_id
              << " has error, don't reconcile";
      continue;
    }
    chrome_account_ids.push_back(chrome_account_with_refresh_tokens.account_id);
  }

  VLOG(1) << "AccountReconcilor::ValidateAccountsFromTokenService: "
          << "Chrome " << chrome_account_ids.size() << " accounts";

  return chrome_account_ids;
}

void AccountReconcilor::OnReceivedManageAccountsResponse(
    signin::GAIAServiceType service_type) {
  if (service_type == signin::GAIA_SERVICE_TYPE_ADDSESSION) {
    identity_manager_->GetAccountsCookieMutator()->TriggerCookieJarUpdate();
  }
}

void AccountReconcilor::FinishReconcile(
    const std::string& primary_account,
    const std::vector<std::string>& chrome_accounts,
    std::vector<gaia::ListedAccount>&& gaia_accounts) {
  VLOG(1) << "AccountReconcilor::FinishReconcile";
  DCHECK(add_to_cookie_.empty());

  size_t number_gaia_accounts = gaia_accounts.size();
  // If there are any accounts in the gaia cookie but not in chrome, then
  // those accounts need to be removed from the cookie.  This means we need
  // to blow the cookie away.
  int removed_from_cookie = 0;
  for (size_t i = 0; i < number_gaia_accounts; ++i) {
    if (gaia_accounts[i].valid &&
        !base::ContainsValue(chrome_accounts, gaia_accounts[i].id)) {
      ++removed_from_cookie;
    }
  }

  std::string first_account = delegate_->GetFirstGaiaAccountForReconcile(
      chrome_accounts, gaia_accounts, primary_account, first_execution_,
      removed_from_cookie > 0);
  bool first_account_mismatch =
      (number_gaia_accounts > 0) && (first_account != gaia_accounts[0].id);

  bool rebuild_cookie = first_account_mismatch || (removed_from_cookie > 0);
  std::vector<gaia::ListedAccount> original_gaia_accounts = gaia_accounts;
  if (rebuild_cookie) {
    VLOG(1) << "AccountReconcilor::FinishReconcile: rebuild cookie";
    // Really messed up state.  Blow away the gaia cookie completely and
    // rebuild it, making sure the primary account as specified by the
    // IdentityManager is the first session in the gaia cookie.
    PerformLogoutAllAccountsAction();
    gaia_accounts.clear();
  }

  if (first_account.empty()) {
    DCHECK(!delegate_->ShouldAbortReconcileIfPrimaryHasError());
    reconcile_is_noop_ = !RevokeAllSecondaryTokens(
        identity_manager_,
        AccountReconcilorDelegate::RevokeTokenOption::kRevoke, primary_account,
        delegate_->IsAccountConsistencyEnforced(),
        signin_metrics::SourceForRefreshTokenOperation::
            kAccountReconcilor_Reconcile);
  } else {
    // Create a list of accounts that need to be added to the Gaia cookie.
    if (base::ContainsValue(chrome_accounts, first_account)) {
      add_to_cookie_.push_back(first_account);
    } else {
      // If the first account is not empty and not in chrome_accounts, it is
      // impossible to rebuild it. It must be already the current default
      // account, and no logout can happen.
      DCHECK_EQ(gaia_accounts[0].gaia_id, first_account);
      DCHECK(!rebuild_cookie);
    }
    for (size_t i = 0; i < chrome_accounts.size(); ++i) {
      if (chrome_accounts[i] != first_account)
        add_to_cookie_.push_back(chrome_accounts[i]);
    }
  }

  // For each account known to chrome, PerformMergeAction() if the account is
  // not already in the cookie jar or its state is invalid, or signal merge
  // completed otherwise.  Make a copy of |add_to_cookie_| since calls to
  // OnAddAccountToCookieCompleted() will change the array.
  std::vector<std::string> add_to_cookie_copy = add_to_cookie_;
  int added_to_cookie = 0;
  for (size_t i = 0; i < add_to_cookie_copy.size(); ++i) {
    if (gaia_accounts.end() !=
        std::find_if(gaia_accounts.begin(), gaia_accounts.end(),
                     AccountEqualToFunc(AccountForId(add_to_cookie_copy[i])))) {
      OnAddAccountToCookieCompleted(add_to_cookie_copy[i],
                                    GoogleServiceAuthError::AuthErrorNone());
    } else {
      PerformMergeAction(add_to_cookie_copy[i]);
      if (original_gaia_accounts.end() ==
          std::find_if(
              original_gaia_accounts.begin(), original_gaia_accounts.end(),
              AccountEqualToFunc(AccountForId(add_to_cookie_copy[i])))) {
        added_to_cookie++;
      }
    }
  }

  signin_metrics::LogSigninAccountReconciliation(
      chrome_accounts.size(), added_to_cookie, removed_from_cookie,
      !first_account_mismatch, first_execution_, number_gaia_accounts);
  first_execution_ = false;
  CalculateIfReconcileIsDone();
  if (!is_reconcile_started_)
    delegate_->OnReconcileFinished(first_account, reconcile_is_noop_);
  ScheduleStartReconcileIfChromeAccountsChanged();
}

void AccountReconcilor::AbortReconcile() {
  VLOG(1) << "AccountReconcilor::AbortReconcile: try again later";
  add_to_cookie_.clear();
  CalculateIfReconcileIsDone();

  DCHECK(!is_reconcile_started_);
  DCHECK(!timer_->IsRunning());
}

void AccountReconcilor::CalculateIfReconcileIsDone() {
  base::TimeDelta duration = base::Time::Now() - reconcile_start_time_;
  // Record the duration if reconciliation was underway and now it is over.
  if (is_reconcile_started_ && add_to_cookie_.empty()) {
    bool was_last_reconcile_successful =
        (error_during_last_reconcile_.state() ==
         GoogleServiceAuthError::State::NONE);
    signin_metrics::LogSigninAccountReconciliationDuration(
        duration, was_last_reconcile_successful);

    // Reconciliation has actually finished (and hence stop the timer), but it
    // may have ended in some failures. Pass this information to the
    // |delegate_|.
    timer_->Stop();
    if (!was_last_reconcile_successful) {
      // Note: This is the only call to |OnReconcileError| in this file. We MUST
      // make sure that we do not call |OnReconcileError| multiple times in the
      // same reconciliation batch.
      // The enclosing if-condition |is_reconcile_started_ &&
      // add_to_cookie_.empty()| represents the halting condition for one batch
      // of reconciliation.
      delegate_->OnReconcileError(error_during_last_reconcile_);
    }
  }

  is_reconcile_started_ = !add_to_cookie_.empty();
  if (!is_reconcile_started_)
    VLOG(1) << "AccountReconcilor::CalculateIfReconcileIsDone: done";
}

void AccountReconcilor::ScheduleStartReconcileIfChromeAccountsChanged() {
  if (is_reconcile_started_)
    return;

  // Start a reconcile as the token accounts have changed.
  VLOG(1) << "AccountReconcilor::StartReconcileIfChromeAccountsChanged";
  if (chrome_accounts_changed_) {
    chrome_accounts_changed_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AccountReconcilor::StartReconcile,
                                  base::Unretained(this)));
  }
}

// Remove the account from the list that is being merged.
bool AccountReconcilor::MarkAccountAsAddedToCookie(
    const std::string& account_id) {
  for (auto i = add_to_cookie_.begin(); i != add_to_cookie_.end(); ++i) {
    if (account_id == *i) {
      add_to_cookie_.erase(i);
      return true;
    }
  }
  return false;
}

bool AccountReconcilor::IsIdentityManagerReady() {
#if defined(OS_CHROMEOS)
  // TODO(droger): ChromeOS should use the same logic as other platforms. See
  // https://crbug.com/749535
  // On ChromeOS, there are cases where the IdentityManager is never fully
  // initialized and AreAllCredentialsLoaded() always return false.
  return identity_manager_->AreRefreshTokensLoaded() ||
         (identity_manager_->GetAccountsWithRefreshTokens().size() > 0);
#else
  return identity_manager_->AreRefreshTokensLoaded();
#endif
}

void AccountReconcilor::OnSetAccountsInCookieCompleted(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "AccountReconcilor::OnSetAccountsInCookieCompleted: "
          << "Error was " << error.ToString();
  if (is_reconcile_started_) {
    if (error.state() != GoogleServiceAuthError::State::NONE &&
        !error_during_last_reconcile_.IsPersistentError()) {
      error_during_last_reconcile_ = error;
      delegate_->OnReconcileError(error_during_last_reconcile_);
    }

    set_accounts_in_progress_ = false;
    is_reconcile_started_ = false;

    timer_->Stop();
    base::TimeDelta duration = base::Time::Now() - reconcile_start_time_;
    signin_metrics::LogSigninAccountReconciliationDuration(
        duration, (error_during_last_reconcile_.state() ==
                   GoogleServiceAuthError::State::NONE));
    ScheduleStartReconcileIfChromeAccountsChanged();
  }
}

void AccountReconcilor::OnAddAccountToCookieCompleted(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  VLOG(1) << "AccountReconcilor::OnAddAccountToCookieCompleted: "
          << "Account added: " << account_id << ", "
          << "Error was " << error.ToString();
  // Always listens to GaiaCookieManagerService. Only proceed if reconciling.
  if (is_reconcile_started_ && MarkAccountAsAddedToCookie(account_id)) {
    // We may have seen a series of errors during reconciliation. Delegates may
    // rely on the severity of the last seen error (see |OnReconcileError|) and
    // hence do not override a persistent error, if we have seen one.
    if (error.state() != GoogleServiceAuthError::State::NONE &&
        !error_during_last_reconcile_.IsPersistentError()) {
      error_during_last_reconcile_ = error;
    }
    CalculateIfReconcileIsDone();
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
    StartReconcile();
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
  // |error_during_last_reconcile_|, through |CalculateIfReconcileIsDone|.
  AbortReconcile();
  DCHECK(!timer_->IsRunning());
}

bool AccountReconcilor::IsMultiloginEndpointEnabled() const {
#if defined(OS_IOS)
  // kUseMultiloginEndpoint feature should not be used if
  // kWKHTTPSystemCookieStore feature is disabbled.
  // See http://crbug.com/902584.
  if (!is_wkhttp_system_cookie_store_enabled_)
    return false;
#endif  // defined(OS_IOS)

#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(signin::kMiceFeature))
    return true;  // Mice is only implemented with multilogin.
#endif
  return base::FeatureList::IsEnabled(kUseMultiloginEndpoint);
}
