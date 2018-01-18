// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_IDENTITY_MANAGER_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_IDENTITY_MANAGER_H_

#include "base/observer_list.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "services/identity/public/cpp/primary_account_access_token_fetcher.h"

namespace identity {

// Primary client-side interface to the Identity Service, encapsulating a
// connection to a remote implementation of mojom::IdentityManager. See
// ./README.md for detailed documentation.
class IdentityManager : public SigninManagerBase::Observer,
                        public OAuth2TokenService::DiagnosticsObserver {
 public:
  class Observer {
   public:
    // Called when an account becomes the user's primary account.
    // This method is not called during a reauth.
    virtual void OnPrimaryAccountSet(const AccountInfo& primary_account_info) {}

    // Called when when the user moves from having a primary account to no
    // longer having a primary account.
    virtual void OnPrimaryAccountCleared(
        const AccountInfo& previous_primary_account_info) {}

    // TODO(blundell): Eventually we might need a callback for failure to log in
    // to the primary account.

   protected:
    virtual ~Observer() {}
  };

  // Observer interface for classes that want to monitor status of various
  // requests. Mostly useful in tests and debugging contexts (e.g., WebUI).
  class DiagnosticsObserver {
   public:
    // Called when receiving request for access token.
    virtual void OnAccessTokenRequested(
        const std::string& account_id,
        const std::string& consumer_id,
        const OAuth2TokenService::ScopeSet& scopes) {}

   protected:
    virtual ~DiagnosticsObserver() {}
  };

  IdentityManager(SigninManagerBase* signin_manager,
                  ProfileOAuth2TokenService* token_service);
  ~IdentityManager() override;

  // Provides access to the latest cached information of the user's primary
  // account.
  AccountInfo GetPrimaryAccountInfo();

  // Returns whether the primary account is available, according to the latest
  // cached information. Simple convenience wrapper over checking whether the
  // primary account info has a valid account ID.
  bool HasPrimaryAccount();

  // Creates a PrimaryAccountAccessTokenFetcher given the passed-in information.
  std::unique_ptr<PrimaryAccountAccessTokenFetcher>
  CreateAccessTokenFetcherForPrimaryAccount(
      const std::string& oauth_consumer_name,
      const OAuth2TokenService::ScopeSet& scopes,
      PrimaryAccountAccessTokenFetcher::TokenCallback callback,
      PrimaryAccountAccessTokenFetcher::Mode mode);

  // If an entry exists in the Identity Service's cache corresponding to the
  // given information, removes that entry; in this case, the next access token
  // request for |account_id| and |scopes| will fetch a new token from the
  // network. Otherwise, is a no-op.
  void RemoveAccessTokenFromCache(const AccountInfo& account_info,
                                  const OAuth2TokenService::ScopeSet& scopes,
                                  const std::string& access_token);

  // Methods to register or remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  void AddDiagnosticsObserver(DiagnosticsObserver* observer);
  void RemoveDiagnosticsObserver(DiagnosticsObserver* observer);

  // Sets the primary account info synchronously with both the IdentityManager
  // and its backing SigninManager/ProfileOAuth2TokenService instances. For use
  // only by tests. Even in testing contexts, use IdentityTestEnvironment if
  // possible (and IdentityTestEnvironment::MakePrimaryAccountAvailable()). This
  // method should be used directly only if the production code is using
  // IdentityManager, but it is not yet feasible to convert the test code to use
  // IdentityTestEnvironment. Any such usage should only be temporary, i.e.,
  // should be followed as quickly as possible by conversion of the test to
  // use IdentityTestEnvironment.
  void SetPrimaryAccountSynchronouslyForTests(std::string gaia_id,
                                              std::string email_address,
                                              std::string refresh_token);

 private:
  // SigninManagerBase::Observer:
  void GoogleSigninSucceeded(const AccountInfo& account_info) override;
  void GoogleSignedOut(const AccountInfo& account_info) override;

  // OAuth2TokenService::DiagnosticsObserver:
  void OnAccessTokenRequested(
      const std::string& account_id,
      const std::string& consumer_id,
      const OAuth2TokenService::ScopeSet& scopes) override;

  // Updates |primary_account_info_| and notifies observers. Invoked
  // asynchronously from GoogleSigninSucceeded() to mimic the effect of
  // receiving this call asynchronously from the Identity Service.
  void HandleGoogleSigninSucceeded(const AccountInfo& account_info);

  // Clears |primary_account_info_| and notifies observers. Invoked
  // asynchronously from GoogleSignedOut() to mimic the effect of
  // receiving this call asynchronously from the Identity Service.
  void HandleGoogleSignedOut(const AccountInfo& account_info);

  // Notifies diagnostics observers. Invoked asynchronously from
  // OnAccessTokenRequested() to mimic the effect of receiving this call
  // asynchronously from the Identity Service.
  void HandleOnAccessTokenRequested(const std::string& account_id,
                                    const std::string& consumer_id,
                                    const OAuth2TokenService::ScopeSet& scopes);

  // Backing signin classes. NOTE: We strive to limit synchronous access to
  // these classes in the IdentityManager implementation, as all such
  // synchronous access will become impossible when IdentityManager is backed by
  // the Identity Service.
  SigninManagerBase* signin_manager_;
  ProfileOAuth2TokenService* token_service_;

  // The latest (cached) value of the primary account.
  AccountInfo primary_account_info_;

  // Lists of observers.
  // Makes sure lists are empty on destruction.
  base::ObserverList<Observer, true> observer_list_;
  base::ObserverList<DiagnosticsObserver, true> diagnostics_observer_list_;

  DISALLOW_COPY_AND_ASSIGN(IdentityManager);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_IDENTITY_MANAGER_H_
