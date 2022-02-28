// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager.h"

#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"

SigninManager::SigninManager(PrefService* prefs,
                             signin::IdentityManager* identity_manager)
    : prefs_(prefs), identity_manager_(identity_manager) {
  signin_allowed_.Init(
      prefs::kSigninAllowed, prefs_,
      base::BindRepeating(&SigninManager::OnSigninAllowedPrefChanged,
                          base::Unretained(this)));

  UpdateUnconsentedPrimaryAccount();
  identity_manager_->AddObserver(this);
}

SigninManager::~SigninManager() {
  identity_manager_->RemoveObserver(this);
}

void SigninManager::UpdateUnconsentedPrimaryAccount() {
  // Only update the unconsented primary account only after accounts are loaded.
  if (!identity_manager_->AreRefreshTokensLoaded()) {
    return;
  }

  absl::optional<CoreAccountInfo> account =
      ComputeUnconsentedPrimaryAccountInfo();

  DCHECK(!account || !account->IsEmpty());
  if (account) {
    if (identity_manager_->GetPrimaryAccountInfo(
            signin::ConsentLevel::kSignin) != account) {
      DCHECK(
          !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync));
      identity_manager_->GetPrimaryAccountMutator()->SetPrimaryAccount(
          account->account_id, signin::ConsentLevel::kSignin);
    }
  } else if (identity_manager_->HasPrimaryAccount(
                 signin::ConsentLevel::kSignin)) {
    DCHECK(!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync));
    identity_manager_->GetPrimaryAccountMutator()->ClearPrimaryAccount(
        signin_metrics::USER_DELETED_ACCOUNT_COOKIES,
        signin_metrics::SignoutDelete::kIgnoreMetric);
  }
}

absl::optional<CoreAccountInfo>
SigninManager::ComputeUnconsentedPrimaryAccountInfo() const {
  DCHECK(identity_manager_->AreRefreshTokensLoaded());

  // UPA is equal to the primary account with sync consent if it exists.
  if (identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    return identity_manager_->GetPrimaryAccountInfo(
        signin::ConsentLevel::kSync);
  }

  // Clearing the primary sync account when sign-in is not allowed is handled
  // by PrimaryAccountPolicyManager. That flow is extremely hard to follow
  // especially for the case when the user is syncing with a managed account
  // as in that case the whole profile needs to be deleted.
  //
  // It was considered simpler to keep the logic to update the unconsented
  // primary account in a single place.
  if (!signin_allowed_.GetValue())
    return absl::nullopt;

  signin::AccountsInCookieJarInfo cookie_info =
      identity_manager_->GetAccountsInCookieJar();

  std::vector<gaia::ListedAccount> cookie_accounts =
      cookie_info.signed_in_accounts;

  // Fresh cookies and loaded tokens are needed to compute the UPA.
  if (cookie_info.accounts_are_fresh) {
    // Cookies are fresh and tokens are loaded, UPA is the first account
    // in cookies if it exists and has a refresh token.
    if (cookie_accounts.empty()) {
      // Cookies are empty, the UPA is empty.
      return absl::nullopt;
    }

    AccountInfo account_info =
        identity_manager_->FindExtendedAccountInfoByAccountId(
            cookie_accounts[0].id);

    // Verify the first account in cookies has a refresh token that is valid.
    bool error_state =
        account_info.IsEmpty() ||
        identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
            account_info.account_id);

    return error_state ? absl::nullopt
                       : absl::make_optional<CoreAccountInfo>(account_info);
  }

  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin))
    return absl::nullopt;

  // If cookies or tokens are not loaded, it is not possible to fully compute
  // the unconsented primary account. However, if the current unconsented
  // primary account is no longer valid, it has to be removed.
  CoreAccountId current_account =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

  if (!identity_manager_->HasAccountWithRefreshToken(current_account)) {
    // Tokens are loaded, but the current UPA doesn't have a refresh token.
    // Clear the current UPA.
    return absl::nullopt;
  }

  if (cookie_info.accounts_are_fresh) {
    if (cookie_accounts.empty() || cookie_accounts[0].id != current_account) {
      // The current UPA is not the first in fresh cookies. It needs to be
      // cleared.
      return absl::nullopt;
    }
  }

  // No indication that the current UPA is invalid, return current UPA.
  return identity_manager_->GetPrimaryAccountInfo(
      signin::ConsentLevel::kSignin);
}

// signin::IdentityManager::Observer implementation.
void SigninManager::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  // This is needed for the case where the user chooses to start syncing
  // with an account that is different from the unconsented primary account
  // (not the first in cookies) but then cancels. In that case, the tokens stay
  // the same. In all the other cases, either the token will be revoked which
  // will trigger an update for the unconsented primary account or the
  // primary account stays the same but the sync consent is revoked.
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSync) !=
      signin::PrimaryAccountChangeEvent::Type::kCleared) {
    return;
  }

  // It is important to update the primary account after all observers process
  // the current OnPrimaryAccountChanged() as all observers should see the same
  // value for the unconsented primary account. Schedule the potential update
  // on the next run loop.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SigninManager::UpdateUnconsentedPrimaryAccount,
                                weak_ptr_factory_.GetWeakPtr()));
}

void SigninManager::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  UpdateUnconsentedPrimaryAccount();
}

void SigninManager::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  UpdateUnconsentedPrimaryAccount();
}

void SigninManager::OnRefreshTokensLoaded() {
  UpdateUnconsentedPrimaryAccount();
}

void SigninManager::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  UpdateUnconsentedPrimaryAccount();
}

void SigninManager::OnAccountsCookieDeletedByUserAction() {
  UpdateUnconsentedPrimaryAccount();
}

void SigninManager::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error) {
  CoreAccountInfo current_account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  bool should_update = false;
  if (error == GoogleServiceAuthError::AuthErrorNone()) {
    should_update = current_account.IsEmpty();
  } else {
    // In error state, update if the account in error is the current UPA.
    should_update = (account_info == current_account);
  }

  if (should_update)
    UpdateUnconsentedPrimaryAccount();
}

void SigninManager::OnSigninAllowedPrefChanged() {
  UpdateUnconsentedPrimaryAccount();
}
