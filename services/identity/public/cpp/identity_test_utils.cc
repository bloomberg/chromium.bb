// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/identity_test_utils.h"

#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace identity {

namespace {

class OneShotIdentityManagerObserver : public IdentityManager::Observer {
 public:
  OneShotIdentityManagerObserver(IdentityManager* identity_manager,
                                 base::OnceClosure done_closure);
  ~OneShotIdentityManagerObserver() override;

 private:
  // IdentityManager::Observer:
  void OnPrimaryAccountSet(const AccountInfo& primary_account_info) override;

  void OnPrimaryAccountCleared(
      const AccountInfo& previous_primary_account_info) override;

  IdentityManager* identity_manager_;
  base::OnceClosure done_closure_;

  DISALLOW_COPY_AND_ASSIGN(OneShotIdentityManagerObserver);
};

OneShotIdentityManagerObserver::OneShotIdentityManagerObserver(
    IdentityManager* identity_manager,
    base::OnceClosure done_closure)
    : identity_manager_(identity_manager),
      done_closure_(std::move(done_closure)) {
  identity_manager_->AddObserver(this);
}

OneShotIdentityManagerObserver::~OneShotIdentityManagerObserver() {
  identity_manager_->RemoveObserver(this);
}

void OneShotIdentityManagerObserver::OnPrimaryAccountSet(
    const AccountInfo& primary_account_info) {
  DCHECK(done_closure_);
  std::move(done_closure_).Run();
}

void OneShotIdentityManagerObserver::OnPrimaryAccountCleared(
    const AccountInfo& previous_primary_account_info) {
  DCHECK(done_closure_);
  std::move(done_closure_).Run();
}

}  // namespace

std::string SetPrimaryAccount(SigninManagerBase* signin_manager,
                              IdentityManager* identity_manager,
                              const std::string& email) {
  DCHECK(!signin_manager->IsAuthenticated());
  DCHECK(!identity_manager->HasPrimaryAccount());
  std::string gaia_id = "gaia_id_for_" + email;

#if defined(OS_CHROMEOS)
  // ChromeOS has no real notion of signin, so just plumb the information
  // through (note: supply an empty string as the refresh token so that no
  // refresh token is set).
  identity_manager->SetPrimaryAccountSynchronously(gaia_id, email,
                                                   /*refresh_token=*/"");
#else

  base::RunLoop run_loop;
  OneShotIdentityManagerObserver signin_observer(identity_manager,
                                                 run_loop.QuitClosure());

  SigninManager* real_signin_manager =
      SigninManager::FromSigninManagerBase(signin_manager);
  // Note: It's important to pass base::DoNothing() (rather than a null
  // callback) to make this work with both SigninManager and FakeSigninManager.
  // If we would pass a null callback, then SigninManager would call
  // CompletePendingSignin directly, but FakeSigninManager never does that.
  // Note: pass an empty string as the refresh token so that no refresh token is
  // set.
  real_signin_manager->StartSignInWithRefreshToken(
      /*refresh_token=*/"", gaia_id, email, /*password=*/"",
      /*oauth_fetched_callback=*/base::DoNothing());
  real_signin_manager->CompletePendingSignin();

  run_loop.Run();
#endif

  DCHECK(signin_manager->IsAuthenticated());
  DCHECK(identity_manager->HasPrimaryAccount());
  return identity_manager->GetPrimaryAccountInfo().account_id;
}

void SetRefreshTokenForAccount(ProfileOAuth2TokenService* token_service,
                               IdentityManager* identity_manager,
                               const std::string& account_id) {
  std::string refresh_token = "refresh_token_for_" + account_id;
  token_service->UpdateCredentials(account_id, refresh_token);
}

void SetInvalidRefreshTokenForAccount(ProfileOAuth2TokenService* token_service,
                                      IdentityManager* identity_manager,
                                      const std::string& account_id) {
  token_service->UpdateCredentials(
      account_id, OAuth2TokenServiceDelegate::kInvalidRefreshToken);
}

void RemoveRefreshTokenForAccount(ProfileOAuth2TokenService* token_service,
                                  IdentityManager* identity_manager,
                                  const std::string& account_id) {
  token_service->RevokeCredentials(account_id);
}

std::string MakePrimaryAccountAvailable(
    SigninManagerBase* signin_manager,
    ProfileOAuth2TokenService* token_service,
    IdentityManager* identity_manager,
    const std::string& email) {
  std::string account_id =
      SetPrimaryAccount(signin_manager, identity_manager, email);
  SetRefreshTokenForAccount(token_service, identity_manager, account_id);
  return account_id;
}

void ClearPrimaryAccount(SigninManagerForTest* signin_manager,
                         IdentityManager* identity_manager) {
#if defined(OS_CHROMEOS)
  // TODO(blundell): If we ever need this functionality on ChromeOS (which seems
  // unlikely), plumb this through to just clear the primary account info
  // synchronously with IdentityManager.
  NOTREACHED();
#else
  base::RunLoop run_loop;
  OneShotIdentityManagerObserver signout_observer(identity_manager,
                                                  run_loop.QuitClosure());

  signin_manager->ForceSignOut();

  run_loop.Run();
#endif
}

}  // namespace identity
