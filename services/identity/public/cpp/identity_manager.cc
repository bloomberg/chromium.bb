// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/identity_manager.h"

#include "google_apis/gaia/gaia_auth_util.h"

namespace identity {

namespace {

// Local copy of the account ID used for supervised users (defined in //chrome
// as supervised_users::kSupervisedUserPseudoEmail). Simply copied to avoid
// plumbing it from //chrome all the way down through the Identity Service just
// to handle the corner cases below.
// TODO(860492): Remove this once supervised user support is removed.
const char kSupervisedUserPseudoEmail[] = "managed_user@localhost";

// A made-up Gaia ID to populate the supervised user's AccountInfo with in order
// to maintain the invariant that the AccountInfos passed out by IdentityManager
// always have an account ID, Gaia ID, and email set.
// TODO(860492): Remove this once supervised user support is removed.
const char kSupervisedUserPseudoGaiaID[] = "managed_user_gaia_id";

// Maps a vector of gaia::ListedAccount structs to a corresponding vector of
// AccountInfo structs.
std::vector<AccountInfo> ListedAccountsToAccountInfos(
    const std::vector<gaia::ListedAccount>& listed_accounts) {
  std::vector<AccountInfo> account_infos;

  for (const auto& listed_account : listed_accounts) {
    AccountInfo account_info;
    account_info.account_id = listed_account.id;
    account_info.gaia = listed_account.gaia_id;
    account_info.email = listed_account.email;
    account_infos.push_back(account_info);
  }

  return account_infos;
}

}  // namespace

IdentityManager::IdentityManager(
    SigninManagerBase* signin_manager,
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
    GaiaCookieManagerService* gaia_cookie_manager_service)
    : signin_manager_(signin_manager),
      token_service_(token_service),
      account_tracker_service_(account_tracker_service),
      gaia_cookie_manager_service_(gaia_cookie_manager_service) {
  // Initialize the state of accounts with refresh tokens.
  // |account_id| is moved into |accounts_with_refresh_tokens_|.
  // Do not change this to "const std::string&".
  for (std::string account_id : token_service->GetAccounts()) {
    AccountInfo account_info =
        account_tracker_service_->GetAccountInfo(account_id);

    // In the context of supervised users, the ProfileOAuth2TokenService is used
    // without the AccountTrackerService being used. This is the only case in
    // which the AccountTrackerService will potentially not know about the
    // account. In this context, |account_id| is always set to
    // kSupervisedUserPseudoEmail.
    // TODO(860492): Remove this special case once supervised user support is
    // removed.
    DCHECK(!account_info.IsEmpty() || account_id == kSupervisedUserPseudoEmail);

    if (account_id == kSupervisedUserPseudoEmail && account_info.IsEmpty()) {
      // Populate the information manually to maintain the invariant that the
      // account ID, gaia ID, and email are always set.
      account_info.account_id = account_id;
      account_info.email = kSupervisedUserPseudoEmail;
      account_info.gaia = kSupervisedUserPseudoGaiaID;
    }

    accounts_with_refresh_tokens_.emplace(std::move(account_id),
                                          std::move(account_info));
  }

  signin_manager_->AddObserver(this);
  token_service_->AddDiagnosticsObserver(this);
  token_service_->AddObserver(this);
  token_service_->set_diagnostics_client(this);
  gaia_cookie_manager_service_->AddObserver(this);
}

IdentityManager::~IdentityManager() {
  signin_manager_->RemoveObserver(this);
  token_service_->RemoveObserver(this);
  token_service_->RemoveDiagnosticsObserver(this);
  token_service_->set_diagnostics_client(nullptr);
  gaia_cookie_manager_service_->RemoveObserver(this);
}

AccountInfo IdentityManager::GetPrimaryAccountInfo() const {
  return signin_manager_->GetAuthenticatedAccountInfo();
}

bool IdentityManager::HasPrimaryAccount() const {
  return !GetPrimaryAccountInfo().account_id.empty();
}

#if !defined(OS_CHROMEOS)
void IdentityManager::ClearPrimaryAccount(
    ClearAccountTokensAction token_action,
    signin_metrics::ProfileSignout signout_source_metric,
    signin_metrics::SignoutDelete signout_delete_metric) {
  SigninManager* signin_manager =
      SigninManager::FromSigninManagerBase(signin_manager_);

  switch (token_action) {
    case IdentityManager::ClearAccountTokensAction::kDefault:
      signin_manager->SignOut(signout_source_metric, signout_delete_metric);
      break;
    case IdentityManager::ClearAccountTokensAction::kKeepAll:
      signin_manager->SignOutAndKeepAllAccounts(signout_source_metric,
                                                signout_delete_metric);
      break;
    case IdentityManager::ClearAccountTokensAction::kRemoveAll:
      signin_manager->SignOutAndRemoveAllAccounts(signout_source_metric,
                                                  signout_delete_metric);
      break;
  }

  // NOTE: IdentityManager::Observers are notified in GoogleSignedOut().
}
#endif  // defined(OS_CHROMEOS)

std::vector<AccountInfo> IdentityManager::GetAccountsWithRefreshTokens() const {
  std::vector<std::string> account_ids_with_tokens =
      token_service_->GetAccounts();

  std::vector<AccountInfo> accounts;
  accounts.reserve(account_ids_with_tokens.size());

  for (const std::string& account_id : account_ids_with_tokens) {
    AccountInfo account_info =
        account_tracker_service_->GetAccountInfo(account_id);

    DCHECK(!account_info.IsEmpty() || account_id == kSupervisedUserPseudoEmail);
    if (account_id == kSupervisedUserPseudoEmail && account_info.IsEmpty()) {
      // Populate the information manually to maintain the invariant that the
      // account ID, gaia ID, and email are always set.
      account_info.account_id = account_id;
      account_info.email = kSupervisedUserPseudoEmail;
      account_info.gaia = kSupervisedUserPseudoGaiaID;
    }

    accounts.push_back(account_info);
  }

  return accounts;
}

std::vector<AccountInfo> IdentityManager::GetAccountsInCookieJar(
    const std::string& source) const {
  // TODO(859882): Change this implementation to interact asynchronously with
  // GaiaCookieManagerService as detailed in
  // https://docs.google.com/document/d/1hcrJ44facCSHtMGBmPusvcoP-fAR300Hi-UFez8ffYQ/edit?pli=1#heading=h.w97eil1cygs2.
  std::vector<gaia::ListedAccount> listed_accounts;
  gaia_cookie_manager_service_->ListAccounts(&listed_accounts, nullptr, source);

  return ListedAccountsToAccountInfos(listed_accounts);
}

bool IdentityManager::HasAccountWithRefreshToken(
    const std::string& account_id) const {
  return token_service_->RefreshTokenIsAvailable(account_id);
}

bool IdentityManager::HasPrimaryAccountWithRefreshToken() const {
  return HasAccountWithRefreshToken(GetPrimaryAccountInfo().account_id);
}

std::unique_ptr<AccessTokenFetcher>
IdentityManager::CreateAccessTokenFetcherForAccount(
    const std::string& account_id,
    const std::string& oauth_consumer_name,
    const OAuth2TokenService::ScopeSet& scopes,
    AccessTokenFetcher::TokenCallback callback,
    AccessTokenFetcher::Mode mode) {
  return std::make_unique<AccessTokenFetcher>(account_id, oauth_consumer_name,
                                              token_service_, scopes,
                                              std::move(callback), mode);
}

void IdentityManager::RemoveAccessTokenFromCache(
    const std::string& account_id,
    const OAuth2TokenService::ScopeSet& scopes,
    const std::string& access_token) {
  // TODO(843510): Consider making the request to ProfileOAuth2TokenService
  // asynchronously once there are no direct clients of PO2TS. This change would
  // need to be made together with changing all callsites to
  // ProfileOAuth2TokenService::RequestAccessToken() to be made asynchronously
  // as well (to maintain ordering in the case where a client removes an access
  // token from the cache and then immediately requests an access token).
  token_service_->InvalidateAccessToken(account_id, scopes, access_token);
}

void IdentityManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void IdentityManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void IdentityManager::AddDiagnosticsObserver(DiagnosticsObserver* observer) {
  diagnostics_observer_list_.AddObserver(observer);
}

void IdentityManager::RemoveDiagnosticsObserver(DiagnosticsObserver* observer) {
  diagnostics_observer_list_.RemoveObserver(observer);
}

void IdentityManager::SetPrimaryAccountSynchronouslyForTests(
    const std::string& gaia_id,
    const std::string& email_address,
    const std::string& refresh_token) {
  DCHECK(!refresh_token.empty());
  SetPrimaryAccountSynchronously(gaia_id, email_address, refresh_token);
}

void IdentityManager::SetPrimaryAccountSynchronously(
    const std::string& gaia_id,
    const std::string& email_address,
    const std::string& refresh_token) {
  signin_manager_->SetAuthenticatedAccountInfo(gaia_id, email_address);

  if (!refresh_token.empty()) {
    token_service_->UpdateCredentials(GetPrimaryAccountInfo().account_id,
                                      refresh_token);
  }
}

void IdentityManager::GoogleSigninSucceeded(const AccountInfo& account_info) {
  for (auto& observer : observer_list_) {
    observer.OnPrimaryAccountSet(account_info);
  }
}

void IdentityManager::GoogleSignedOut(const AccountInfo& account_info) {
  DCHECK(!HasPrimaryAccount());
  for (auto& observer : observer_list_) {
    observer.OnPrimaryAccountCleared(account_info);
  }
}

void IdentityManager::WillFireOnRefreshTokenAvailable(
    const std::string& account_id,
    bool is_valid) {
  DCHECK(!pending_token_available_state_);
  AccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);

  // In the context of supervised users, the ProfileOAuth2TokenService is used
  // without the AccountTrackerService being used. This is the only case in
  // which the AccountTrackerService will potentially not know about the
  // account. In this context, |account_id| is always set to
  // kSupervisedUserPseudoEmail.
  // TODO(860492): Remove this special case once supervised user support is
  // removed.
  DCHECK(!account_info.IsEmpty() || account_id == kSupervisedUserPseudoEmail);
  if (account_id == kSupervisedUserPseudoEmail && account_info.IsEmpty()) {
    // Populate the information manually to maintain the invariant that the
    // account ID, gaia ID, and email are always set.
    account_info.account_id = account_id;
    account_info.email = kSupervisedUserPseudoEmail;
    account_info.gaia = kSupervisedUserPseudoGaiaID;
  }

  // The account might have already been present (e.g., this method can fire on
  // updating an invalid token to a valid one or vice versa); in this case we
  // sanity-check that the cached account info has the expected values.
  auto iterator = accounts_with_refresh_tokens_.find(account_id);
  if (iterator != accounts_with_refresh_tokens_.end()) {
    DCHECK_EQ(account_info.gaia, iterator->second.gaia);
    DCHECK(gaia::AreEmailsSame(account_info.email, iterator->second.email));
  } else {
    auto insertion_result = accounts_with_refresh_tokens_.emplace(
        account_id, std::move(account_info));
    DCHECK(insertion_result.second);
    iterator = insertion_result.first;
  }

  PendingTokenAvailableState pending_token_available_state;
  pending_token_available_state.account_info = iterator->second;
  pending_token_available_state.refresh_token_is_valid = is_valid;
  pending_token_available_state_ = std::move(pending_token_available_state);
}

void IdentityManager::WillFireOnRefreshTokenRevoked(
    const std::string& account_id) {
  DCHECK(!pending_token_revoked_info_);

  auto iterator = accounts_with_refresh_tokens_.find(account_id);
  if (iterator == accounts_with_refresh_tokens_.end()) {
    // A corner case exists wherein the token service revokes tokens while
    // loading tokens during initial startup. This is the only case in which it
    // is expected that we could receive this notification without having
    // previously received a notification that this account was available. In
    // this case, we simply do not forward on the notification, for the
    // following reasons: (1) We may not have a fully-populated |account_info|
    // to send as the argument. (2) Sending the notification breaks clients'
    // expectations that IdentityManager will only fire RefreshTokenRemoved
    // notifications for accounts that it previously knew about.
    DCHECK(!token_service_->AreAllCredentialsLoaded());
    return;
  }

  accounts_with_refresh_tokens_.erase(iterator);

  pending_token_revoked_info_ =
      account_tracker_service_->GetAccountInfo(account_id);

  // In the context of supervised users, the ProfileOAuth2TokenService is used
  // without the AccountTrackerService being used. This is the only case in
  // which the AccountTrackerService will potentially not know about the
  // account. In this context, |account_id| is always set to
  // kSupervisedUserPseudoEmail.

  // TODO(860492): Remove this special case once supervised user support is
  // removed.
  DCHECK(!pending_token_revoked_info_->IsEmpty() ||
         account_id == kSupervisedUserPseudoEmail);
  if (account_id == kSupervisedUserPseudoEmail &&
      pending_token_revoked_info_->IsEmpty()) {
    // Populate the information manually to maintain the invariant that the
    // account ID, gaia ID, and email are always set.
    pending_token_revoked_info_->account_id = account_id;
    pending_token_revoked_info_->email = account_id;
    pending_token_revoked_info_->gaia = kSupervisedUserPseudoGaiaID;
  }
}

void IdentityManager::OnRefreshTokenAvailable(const std::string& account_id) {
  DCHECK(pending_token_available_state_);
  DCHECK_EQ(pending_token_available_state_->account_info.account_id,
            account_id);

  // Move the state out of |pending_token_available_state_| in case any observer
  // callbacks fired below result in mutations of refresh tokens.
  AccountInfo account_info =
      std::move(pending_token_available_state_->account_info);
  bool refresh_token_is_valid =
      pending_token_available_state_->refresh_token_is_valid;

  pending_token_available_state_.reset();

  for (auto& observer : observer_list_) {
    observer.OnRefreshTokenUpdatedForAccount(account_info,
                                             refresh_token_is_valid);
  }
}

void IdentityManager::OnRefreshTokenRevoked(const std::string& account_id) {
  // NOTE: It is possible for |pending_token_revoked_info_| to be null in the
  // corner case of tokens being revoked during initial startup (see
  // WillFireOnRefreshTokenRevoked() above).
  if (!pending_token_revoked_info_) {
    return;
  }

  DCHECK_EQ(pending_token_revoked_info_->account_id, account_id);

  // Move the state out of |pending_token_revoked_info_| in case any observer
  // callbacks fired below result in mutations of refresh tokens.
  AccountInfo account_info = pending_token_revoked_info_.value();
  pending_token_revoked_info_.reset();

  for (auto& observer : observer_list_) {
    observer.OnRefreshTokenRemovedForAccount(account_id);
  }
}

void IdentityManager::OnGaiaAccountsInCookieUpdated(
    const std::vector<gaia::ListedAccount>& accounts,
    const std::vector<gaia::ListedAccount>& signed_out_accounts,
    const GoogleServiceAuthError& error) {
  std::vector<AccountInfo> account_infos =
      ListedAccountsToAccountInfos(accounts);

  for (auto& observer : observer_list_) {
    observer.OnAccountsInCookieUpdated(account_infos);
  }
}

void IdentityManager::OnAccessTokenRequested(
    const std::string& account_id,
    const std::string& consumer_id,
    const OAuth2TokenService::ScopeSet& scopes) {
  // TODO(843510): Consider notifying observers asynchronously once there
  // are no direct clients of ProfileOAuth2TokenService.
  for (auto& observer : diagnostics_observer_list_) {
    observer.OnAccessTokenRequested(account_id, consumer_id, scopes);
  }
}

}  // namespace identity
