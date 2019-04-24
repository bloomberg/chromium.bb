// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_IDENTITY_ACCESSOR_IMPL_H_
#define SERVICES_IDENTITY_IDENTITY_ACCESSOR_IMPL_H_

#include <memory>

#include "base/callback_list.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/identity/public/cpp/account_state.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/scope_set.h"
#include "services/identity/public/mojom/identity_accessor.mojom.h"

class AccountTrackerService;

namespace identity {

class IdentityAccessorImpl : public mojom::IdentityAccessor,
                             public IdentityManager::Observer {
 public:
  static void Create(mojom::IdentityAccessorRequest request,
                     IdentityManager* identity_manager,
                     AccountTrackerService* account_tracker);

  IdentityAccessorImpl(mojom::IdentityAccessorRequest request,
                       IdentityManager* identity_manager,
                       AccountTrackerService* account_tracker);
  ~IdentityAccessorImpl() override;

 private:
  // Makes an access token request to the IdentityManager on behalf of a
  // given consumer that has made the request to the Identity Service.
  class AccessTokenRequest {
   public:
    AccessTokenRequest(const std::string& account_id,
                       const ScopeSet& scopes,
                       const std::string& consumer_id,
                       GetAccessTokenCallback consumer_callback,
                       IdentityAccessorImpl* manager);
    ~AccessTokenRequest();

   private:
    // Invoked after access token request completes (successful or not).
    // Completes the pending access token request by calling back the consumer.
    void OnTokenRequestCompleted(GoogleServiceAuthError error,
                                 AccessTokenInfo access_token_info);

    std::unique_ptr<AccessTokenFetcher> access_token_fetcher_;
    GetAccessTokenCallback consumer_callback_;
    IdentityAccessorImpl* manager_;
  };
  using AccessTokenRequests =
      std::map<AccessTokenRequest*, std::unique_ptr<AccessTokenRequest>>;

  // mojom::IdentityAccessor:
  void GetPrimaryAccountInfo(GetPrimaryAccountInfoCallback callback) override;
  void GetPrimaryAccountWhenAvailable(
      GetPrimaryAccountWhenAvailableCallback callback) override;
  void GetAccountInfoFromGaiaId(
      const std::string& gaia_id,
      GetAccountInfoFromGaiaIdCallback callback) override;
  void GetAccessToken(const std::string& account_id,
                      const ScopeSet& scopes,
                      const std::string& consumer_id,
                      GetAccessTokenCallback callback) override;

  // IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnPrimaryAccountSet(const CoreAccountInfo& account_info) override;

  // Notified when there is a change in the state of the account
  // corresponding to |account_id|.
  void OnAccountStateChange(const std::string& account_id);

  // Deletes |request|.
  void AccessTokenRequestCompleted(AccessTokenRequest* request);

  // Gets the current state of the account represented by |account_info|.
  AccountState GetStateOfAccount(const AccountInfo& account_info);

  // Called when |binding_| hits a connection error. Destroys this instance,
  // since it's no longer needed.
  void OnConnectionError();

  mojo::Binding<mojom::IdentityAccessor> binding_;
  IdentityManager* identity_manager_;
  AccountTrackerService* account_tracker_;

  // The set of pending requests for access tokens.
  AccessTokenRequests access_token_requests_;

  // List of callbacks that will be notified when the primary account is
  // available.
  std::vector<GetPrimaryAccountWhenAvailableCallback>
      primary_account_available_callbacks_;
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_IDENTITY_ACCESSOR_IMPL_H_
