// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

// The key under which the hosted-domain value is stored in the UserInfo
// response.
const char kGetHostedDomainKey[] = "hd";

typedef base::OnceCallback<void(const std::string&)> StringCallback;

// This class fetches an OAuth2 token scoped for the userinfo and DM services.
class CloudPolicyClientRegistrationHelper::IdentityManagerHelper {
 public:
  IdentityManagerHelper() = default;

  void FetchAccessToken(signin::IdentityManager* identity_manager,
                        const CoreAccountId& account_id,
                        StringCallback callback);

 private:
  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo token_info);

  StringCallback callback_;
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;
};

void CloudPolicyClientRegistrationHelper::IdentityManagerHelper::
    FetchAccessToken(signin::IdentityManager* identity_manager,
                     const CoreAccountId& account_id,
                     StringCallback callback) {
  DCHECK(!access_token_fetcher_);
  // The caller must supply a username.
  DCHECK(!account_id.empty());
  DCHECK(identity_manager->HasAccountWithRefreshToken(account_id));

  callback_ = std::move(callback);

  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
  scopes.insert(GaiaConstants::kOAuthWrapBridgeUserInfoScope);

  access_token_fetcher_ = identity_manager->CreateAccessTokenFetcherForAccount(
      account_id, "cloud_policy", scopes,
      base::BindOnce(&CloudPolicyClientRegistrationHelper::
                         IdentityManagerHelper::OnAccessTokenFetchComplete,
                     base::Unretained(this)),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

void CloudPolicyClientRegistrationHelper::IdentityManagerHelper::
    OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                               signin::AccessTokenInfo token_info) {
  DCHECK(access_token_fetcher_);
  access_token_fetcher_.reset();

  if (error.state() == GoogleServiceAuthError::NONE)
    std::move(callback_).Run(token_info.token);
  else
    std::move(callback_).Run("");
}

CloudPolicyClientRegistrationHelper::CloudPolicyClientRegistrationHelper(
    CloudPolicyClient* client,
    enterprise_management::DeviceRegisterRequest::Type registration_type)
    : client_(client), registration_type_(registration_type) {
  DCHECK(client_);
}

CloudPolicyClientRegistrationHelper::~CloudPolicyClientRegistrationHelper() {
  // Clean up any pending observers in case the browser is shutdown while
  // trying to register for policy.
  if (client_)
    client_->RemoveObserver(this);
}

void CloudPolicyClientRegistrationHelper::StartRegistration(
    signin::IdentityManager* identity_manager,
    const CoreAccountId& account_id,
    base::OnceClosure callback) {
  DVLOG(1) << "Starting registration process with account_id";
  DCHECK(!client_->is_registered());
  callback_ = std::move(callback);
  client_->AddObserver(this);

  identity_manager_helper_.reset(new IdentityManagerHelper());
  identity_manager_helper_->FetchAccessToken(
      identity_manager, account_id,
      base::BindOnce(&CloudPolicyClientRegistrationHelper::OnTokenFetched,
                     base::Unretained(this)));
}

void CloudPolicyClientRegistrationHelper::StartRegistrationWithEnrollmentToken(
    const std::string& token,
    const std::string& client_id,
    base::OnceClosure callback) {
  DVLOG(1) << "Starting registration process with enrollment token";
  DCHECK(!client_->is_registered());
  callback_ = std::move(callback);
  client_->AddObserver(this);
  client_->RegisterWithToken(token, client_id);
}

void CloudPolicyClientRegistrationHelper::OnTokenFetched(
    const std::string& access_token) {
  identity_manager_helper_.reset();

  if (access_token.empty()) {
    DLOG(WARNING) << "Could not fetch access token for "
                  << GaiaConstants::kDeviceManagementServiceOAuth;
    RequestCompleted();
    return;
  }

  // Cache the access token to be used after the GetUserInfo call.
  oauth_access_token_ = access_token;
  DVLOG(1) << "Fetched new scoped OAuth token:" << oauth_access_token_;
  // Now we've gotten our access token - contact GAIA to see if this is a
  // hosted domain.
  user_info_fetcher_.reset(
      new UserInfoFetcher(this, client_->GetURLLoaderFactory()));
  user_info_fetcher_->Start(oauth_access_token_);
}

void CloudPolicyClientRegistrationHelper::OnGetUserInfoFailure(
    const GoogleServiceAuthError& error) {
  DVLOG(1) << "Failed to fetch user info from GAIA: " << error.state();
  user_info_fetcher_.reset();
  RequestCompleted();
}

void CloudPolicyClientRegistrationHelper::OnGetUserInfoSuccess(
    const base::DictionaryValue* data) {
  user_info_fetcher_.reset();
  if (!data->HasKey(kGetHostedDomainKey)) {
    DVLOG(1) << "User not from a hosted domain - skipping registration";
    RequestCompleted();
    return;
  }
  DVLOG(1) << "Registering CloudPolicyClient for user from hosted domain";
  // The user is from a hosted domain, so it's OK to register the
  // CloudPolicyClient and make requests to DMServer.
  if (client_->is_registered()) {
    // Client should not be registered yet.
    NOTREACHED();
    RequestCompleted();
    return;
  }

  // Kick off registration of the CloudPolicyClient with our newly minted
  // oauth_access_token_.
  client_->Register(
      CloudPolicyClient::RegistrationParameters(
          registration_type_, enterprise_management::DeviceRegisterRequest::
                                  FLAVOR_USER_REGISTRATION),
      std::string() /* client_id */, oauth_access_token_);
}

void CloudPolicyClientRegistrationHelper::OnPolicyFetched(
    CloudPolicyClient* client) {
  // Ignored.
}

void CloudPolicyClientRegistrationHelper::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  DVLOG(1) << "Client registration succeeded";
  DCHECK_EQ(client, client_);
  DCHECK(client->is_registered());
  RequestCompleted();
}

void CloudPolicyClientRegistrationHelper::OnClientError(
    CloudPolicyClient* client) {
  DVLOG(1) << "Client registration failed";
  DCHECK_EQ(client, client_);
  RequestCompleted();
}

void CloudPolicyClientRegistrationHelper::RequestCompleted() {
  if (client_) {
    client_->RemoveObserver(this);
    // |client_| may be freed by the callback so clear it now.
    client_ = nullptr;
    std::move(callback_).Run();
  }
}

}  // namespace policy
