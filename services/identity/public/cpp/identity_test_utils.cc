// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/identity_test_utils.h"

#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/list_accounts_test_utils.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace identity {

namespace {

enum class IdentityManagerEvent {
  PRIMARY_ACCOUNT_SET,
  PRIMARY_ACCOUNT_CLEARED,
  REFRESH_TOKENS_LOADED,
  REFRESH_TOKEN_UPDATED,
  REFRESH_TOKEN_REMOVED,
  ACCOUNTS_IN_COOKIE_UPDATED,
};

class OneShotIdentityManagerObserver : public IdentityManager::Observer {
 public:
  OneShotIdentityManagerObserver(IdentityManager* identity_manager,
                                 base::OnceClosure done_closure,
                                 IdentityManagerEvent event_to_wait_on);
  ~OneShotIdentityManagerObserver() override;

 private:
  // IdentityManager::Observer:
  void OnPrimaryAccountSet(
      const CoreAccountInfo& primary_account_info) override;
  void OnPrimaryAccountCleared(
      const CoreAccountInfo& previous_primary_account_info) override;
  void OnRefreshTokensLoaded() override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokenRemovedForAccount(const std::string& account_id) override;
  void OnAccountsInCookieUpdated(
      const AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  IdentityManager* identity_manager_;
  base::OnceClosure done_closure_;
  IdentityManagerEvent event_to_wait_on_;

  DISALLOW_COPY_AND_ASSIGN(OneShotIdentityManagerObserver);
};

OneShotIdentityManagerObserver::OneShotIdentityManagerObserver(
    IdentityManager* identity_manager,
    base::OnceClosure done_closure,
    IdentityManagerEvent event_to_wait_on)
    : identity_manager_(identity_manager),
      done_closure_(std::move(done_closure)),
      event_to_wait_on_(event_to_wait_on) {
  identity_manager_->AddObserver(this);
}

OneShotIdentityManagerObserver::~OneShotIdentityManagerObserver() {
  identity_manager_->RemoveObserver(this);
}

void OneShotIdentityManagerObserver::OnPrimaryAccountSet(
    const CoreAccountInfo& primary_account_info) {
  if (event_to_wait_on_ != IdentityManagerEvent::PRIMARY_ACCOUNT_SET)
    return;

  DCHECK(done_closure_);
  std::move(done_closure_).Run();
}

void OneShotIdentityManagerObserver::OnPrimaryAccountCleared(
    const CoreAccountInfo& previous_primary_account_info) {
  if (event_to_wait_on_ != IdentityManagerEvent::PRIMARY_ACCOUNT_CLEARED)
    return;

  DCHECK(done_closure_);
  std::move(done_closure_).Run();
}

void OneShotIdentityManagerObserver::OnRefreshTokensLoaded() {
  if (event_to_wait_on_ != IdentityManagerEvent::REFRESH_TOKENS_LOADED)
    return;

  DCHECK(done_closure_);
  std::move(done_closure_).Run();
}

void OneShotIdentityManagerObserver::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (event_to_wait_on_ != IdentityManagerEvent::REFRESH_TOKEN_UPDATED)
    return;

  DCHECK(done_closure_);
  std::move(done_closure_).Run();
}

void OneShotIdentityManagerObserver::OnRefreshTokenRemovedForAccount(
    const std::string& account_id) {
  if (event_to_wait_on_ != IdentityManagerEvent::REFRESH_TOKEN_REMOVED)
    return;

  DCHECK(done_closure_);
  std::move(done_closure_).Run();
}

void OneShotIdentityManagerObserver::OnAccountsInCookieUpdated(
    const AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  if (event_to_wait_on_ != IdentityManagerEvent::ACCOUNTS_IN_COOKIE_UPDATED)
    return;

  DCHECK(done_closure_);
  std::move(done_closure_).Run();
}

void WaitForLoadCredentialsToComplete(IdentityManager* identity_manager) {
  base::RunLoop run_loop;
  OneShotIdentityManagerObserver load_credentials_observer(
      identity_manager, run_loop.QuitClosure(),
      IdentityManagerEvent::REFRESH_TOKENS_LOADED);

  if (identity_manager->AreRefreshTokensLoaded())
    return;

  // Do NOT explicitly load credentials here:
  // 1. It is not re-entrant and will DCHECK fail.
  // 2. It should have been called by IdentityManager during its initialization.

  run_loop.Run();
}

// Helper function that updates the refresh token for |account_id| to
// |new_token|. Before updating the refresh token, blocks until refresh tokens
// are loaded. After updating the token, blocks until the update is processed by
// |identity_manager|.
void UpdateRefreshTokenForAccount(
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
    IdentityManager* identity_manager,
    const std::string& account_id,
    const std::string& new_token) {
  DCHECK_EQ(account_tracker_service->GetAccountInfo(account_id).account_id,
            account_id)
      << "To set the refresh token for an unknown account, use "
         "MakeAccountAvailable()";

  // Ensure that refresh tokens are loaded; some platforms enforce the invariant
  // that refresh token mutation cannot occur until refresh tokens are loaded,
  // and it is desired to eventually enforce that invariant across all
  // platforms.
  WaitForLoadCredentialsToComplete(identity_manager);

  base::RunLoop run_loop;
  OneShotIdentityManagerObserver token_updated_observer(
      identity_manager, run_loop.QuitClosure(),
      IdentityManagerEvent::REFRESH_TOKEN_UPDATED);

  token_service->UpdateCredentials(account_id, new_token);

  run_loop.Run();
}

}  // namespace

CoreAccountInfo SetPrimaryAccount(IdentityManager* identity_manager,
                                  const std::string& email) {
  DCHECK(!identity_manager->HasPrimaryAccount());
  SigninManagerBase* signin_manager = identity_manager->GetSigninManager();
  DCHECK(!signin_manager->IsAuthenticated());

  AccountTrackerService* account_tracker_service =
      identity_manager->GetAccountTrackerService();
  AccountInfo account_info =
      account_tracker_service->FindAccountInfoByEmail(email);
  if (account_info.account_id.empty()) {
    std::string gaia_id = GetTestGaiaIdForEmail(email);
    account_tracker_service->SeedAccountInfo(gaia_id, email);
    account_info = account_tracker_service->FindAccountInfoByEmail(email);
  }

  std::string gaia_id = account_info.gaia;
  DCHECK(!gaia_id.empty());

#if defined(OS_CHROMEOS)
  // ChromeOS has no real notion of signin, so just plumb the information
  // through (note: supply an empty string as the refresh token so that no
  // refresh token is set).
  identity_manager->SetPrimaryAccountSynchronously(gaia_id, email,
                                                   /*refresh_token=*/"");
#else
  SigninManager* real_signin_manager =
      SigninManager::FromSigninManagerBase(signin_manager);
  real_signin_manager->OnExternalSigninCompleted(email);
#endif

  DCHECK(signin_manager->IsAuthenticated());
  DCHECK(identity_manager->HasPrimaryAccount());
  return identity_manager->GetPrimaryAccountInfo();
}

void SetRefreshTokenForPrimaryAccount(IdentityManager* identity_manager,
                                      const std::string& token_value) {
  DCHECK(identity_manager->HasPrimaryAccount());
  std::string account_id = identity_manager->GetPrimaryAccountId();
  SetRefreshTokenForAccount(identity_manager, account_id, token_value);
}

void SetInvalidRefreshTokenForPrimaryAccount(
    IdentityManager* identity_manager) {
  DCHECK(identity_manager->HasPrimaryAccount());
  std::string account_id = identity_manager->GetPrimaryAccountId();

  SetInvalidRefreshTokenForAccount(identity_manager, account_id);
}

void RemoveRefreshTokenForPrimaryAccount(IdentityManager* identity_manager) {
  if (!identity_manager->HasPrimaryAccount())
    return;

  std::string account_id = identity_manager->GetPrimaryAccountId();

  RemoveRefreshTokenForAccount(identity_manager, account_id);
}

AccountInfo MakePrimaryAccountAvailable(IdentityManager* identity_manager,
                                        const std::string& email) {
  CoreAccountInfo account_info = SetPrimaryAccount(identity_manager, email);
  SetRefreshTokenForPrimaryAccount(identity_manager);
  base::Optional<AccountInfo> primary_account_info =
      identity_manager->FindAccountInfoForAccountWithRefreshTokenByAccountId(
          account_info.account_id);
  // Ensure that extended information for the account is available after setting
  // the refresh token.
  DCHECK(primary_account_info.has_value());
  return primary_account_info.value();
}

void ClearPrimaryAccount(IdentityManager* identity_manager,
                         ClearPrimaryAccountPolicy policy) {
#if defined(OS_CHROMEOS)
  // TODO(blundell): If we ever need this functionality on ChromeOS (which seems
  // unlikely), plumb this through to just clear the primary account info
  // synchronously with IdentityManager.
  NOTREACHED();
#else
  if (!identity_manager->HasPrimaryAccount())
    return;

  base::RunLoop run_loop;
  OneShotIdentityManagerObserver signout_observer(
      identity_manager, run_loop.QuitClosure(),
      IdentityManagerEvent::PRIMARY_ACCOUNT_CLEARED);

  SigninManager* real_signin_manager = SigninManager::FromSigninManagerBase(
      identity_manager->GetSigninManager());
  signin_metrics::ProfileSignout signout_source_metric =
      signin_metrics::SIGNOUT_TEST;
  signin_metrics::SignoutDelete signout_delete_metric =
      signin_metrics::SignoutDelete::IGNORE_METRIC;

  switch (policy) {
    case ClearPrimaryAccountPolicy::DEFAULT:
      real_signin_manager->SignOut(signout_source_metric,
                                   signout_delete_metric);
      break;
    case ClearPrimaryAccountPolicy::KEEP_ALL_ACCOUNTS:
      real_signin_manager->SignOutAndKeepAllAccounts(signout_source_metric,
                                                     signout_delete_metric);
      break;
    case ClearPrimaryAccountPolicy::REMOVE_ALL_ACCOUNTS:
      real_signin_manager->SignOutAndRemoveAllAccounts(signout_source_metric,
                                                       signout_delete_metric);
      break;
  }

  run_loop.Run();
#endif
}

AccountInfo MakeAccountAvailable(IdentityManager* identity_manager,
                                 const std::string& email) {
  AccountTrackerService* account_tracker_service =
      identity_manager->GetAccountTrackerService();

  DCHECK(account_tracker_service);
  DCHECK(account_tracker_service->FindAccountInfoByEmail(email).IsEmpty());

  // Wait until tokens are loaded, otherwise the account will be removed as soon
  // as tokens finish loading.
  WaitForLoadCredentialsToComplete(identity_manager);

  std::string gaia_id = GetTestGaiaIdForEmail(email);
  account_tracker_service->SeedAccountInfo(gaia_id, email);

  AccountInfo account_info =
      account_tracker_service->FindAccountInfoByEmail(email);
  DCHECK(!account_info.account_id.empty());

  SetRefreshTokenForAccount(identity_manager, account_info.account_id);

  return account_info;
}

void SetRefreshTokenForAccount(IdentityManager* identity_manager,
                               const std::string& account_id,
                               const std::string& token_value) {
  UpdateRefreshTokenForAccount(
      identity_manager->GetTokenService(),
      identity_manager->GetAccountTrackerService(), identity_manager,
      account_id,
      token_value.empty() ? "refresh_token_for_" + account_id : token_value);
}

void SetInvalidRefreshTokenForAccount(IdentityManager* identity_manager,
                                      const std::string& account_id) {
  UpdateRefreshTokenForAccount(
      identity_manager->GetTokenService(),

      identity_manager->GetAccountTrackerService(), identity_manager,
      account_id, OAuth2TokenServiceDelegate::kInvalidRefreshToken);
}

void RemoveRefreshTokenForAccount(IdentityManager* identity_manager,
                                  const std::string& account_id) {
  if (!identity_manager->HasAccountWithRefreshToken(account_id))
    return;

  base::RunLoop run_loop;
  OneShotIdentityManagerObserver token_updated_observer(
      identity_manager, run_loop.QuitClosure(),
      IdentityManagerEvent::REFRESH_TOKEN_REMOVED);

  identity_manager->GetTokenService()->RevokeCredentials(account_id);

  run_loop.Run();
}

void SetCookieAccounts(IdentityManager* identity_manager,
                       network::TestURLLoaderFactory* test_url_loader_factory,
                       const std::vector<CookieParams>& cookie_accounts) {
  // Convert |cookie_accounts| to the format list_accounts_test_utils wants.
  std::vector<signin::CookieParams> gaia_cookie_accounts;
  for (const CookieParams& params : cookie_accounts) {
    gaia_cookie_accounts.push_back({params.email, params.gaia_id,
                                    /*valid=*/true, /*signed_out=*/false,
                                    /*verified=*/true});
  }

  base::RunLoop run_loop;
  OneShotIdentityManagerObserver cookie_observer(
      identity_manager, run_loop.QuitClosure(),
      IdentityManagerEvent::ACCOUNTS_IN_COOKIE_UPDATED);

  signin::SetListAccountsResponseWithParams(gaia_cookie_accounts,
                                            test_url_loader_factory);

  GaiaCookieManagerService* cookie_manager =
      identity_manager->GetGaiaCookieManagerService();
  cookie_manager->set_list_accounts_stale_for_testing(true);
  cookie_manager->ListAccounts(nullptr, nullptr);

  run_loop.Run();
}

void UpdateAccountInfoForAccount(IdentityManager* identity_manager,
                                 AccountInfo account_info) {
  // Make sure the account being updated is a known account.

  AccountTrackerService* account_tracker_service =
      identity_manager->GetAccountTrackerService();

  DCHECK(account_tracker_service);
  DCHECK(!account_tracker_service->GetAccountInfo(account_info.account_id)
              .account_id.empty());

  account_tracker_service->SeedAccountInfo(account_info);
}

void SetFreshnessOfAccountsInGaiaCookie(IdentityManager* identity_manager,
                                        bool accounts_are_fresh) {
  GaiaCookieManagerService* cookie_manager =
      identity_manager->GetGaiaCookieManagerService();
  cookie_manager->set_list_accounts_stale_for_testing(!accounts_are_fresh);
}

std::string GetTestGaiaIdForEmail(const std::string& email) {
  std::string gaia_id =
      std::string("gaia_id_for_") + gaia::CanonicalizeEmail(email);
  // Avoid character '@' in the gaia ID string as there is code in the codebase
  // that asserts that a gaia ID does not contain a "@" character.
  std::replace(gaia_id.begin(), gaia_id.end(), '@', '_');
  return gaia_id;
}

void UpdatePersistentErrorOfRefreshTokenForAccount(
    IdentityManager* identity_manager,
    const std::string& account_id,
    const GoogleServiceAuthError& auth_error) {
  DCHECK(identity_manager->HasAccountWithRefreshToken(account_id));
  identity_manager->GetTokenService()->GetDelegate()->UpdateAuthError(
      account_id, auth_error);
}

void DisableAccessTokenFetchRetries(IdentityManager* identity_manager) {
  identity_manager->GetTokenService()
      ->set_max_authorization_token_fetch_retries_for_testing(0);
}

void CancelAllOngoingGaiaCookieOperations(IdentityManager* identity_manager) {
  identity_manager->GetGaiaCookieManagerService()->CancelAll();
}

}  // namespace identity
