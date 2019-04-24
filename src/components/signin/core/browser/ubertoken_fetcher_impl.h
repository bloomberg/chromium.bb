// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_UBERTOKEN_FETCHER_IMPL_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_UBERTOKEN_FETCHER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "components/signin/core/browser/ubertoken_fetcher.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/oauth2_token_service.h"

// Allow to retrieves an uber-auth token for the user. This class uses the
// |OAuth2TokenService| and considers that the user is already logged in. It
// will use the OAuth2 access token to generate the uber-auth token.
//
// This class should be used on a single thread, but it can be whichever thread
// that you like.
//
// This class can handle one request at a time.

class GoogleServiceAuthError;

namespace network {
class SharedURLLoaderFactory;
}

namespace signin {

using GaiaAuthFetcherFactory =
    base::RepeatingCallback<std::unique_ptr<GaiaAuthFetcher>(
        GaiaAuthConsumer*,
        scoped_refptr<network::SharedURLLoaderFactory>)>;

// Allows to retrieve an uber-auth token.
class UbertokenFetcherImpl : public UbertokenFetcher,
                             public GaiaAuthConsumer,
                             public OAuth2TokenService::Consumer {
 public:
  // Maximum number of retries to get the uber-auth token before giving up.
  static const int kMaxRetries;

  using CompletionCallback =
      base::OnceCallback<void(GoogleServiceAuthError error,
                              const std::string& token)>;

  // Constructs an instance and starts fetching the access token and ubertoken
  // sequencially for |account_id|.
  UbertokenFetcherImpl(
      const std::string& account_id,
      OAuth2TokenService* token_service,
      CompletionCallback ubertoken_callback,
      gaia::GaiaSource source,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      bool is_bound_to_channel_id = true);

  // Constructs an instance and starts fetching the ubertoken for |account_id|.
  UbertokenFetcherImpl(
      const std::string& account_id,
      const std::string& access_token,
      OAuth2TokenService* token_service,
      CompletionCallback ubertoken_callback,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GaiaAuthFetcherFactory factory,
      bool is_bound_to_channel_id = true);
  ~UbertokenFetcherImpl() override;

  // Overriden from GaiaAuthConsumer
  void OnUberAuthTokenSuccess(const std::string& token) override;
  void OnUberAuthTokenFailure(const GoogleServiceAuthError& error) override;

  // Overriden from OAuth2TokenService::Consumer:
  void OnGetTokenSuccess(
      const OAuth2TokenService::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

 private:
  // Request a login-scoped access token from the token service.
  void RequestAccessToken();

  // Exchanges an oauth2 access token for an uber-auth token.
  void ExchangeTokens();

  OAuth2TokenService* token_service_;
  CompletionCallback ubertoken_callback_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  bool is_bound_to_channel_id_;  // defaults to true
  GaiaAuthFetcherFactory gaia_auth_fetcher_factory_;
  std::unique_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;
  std::unique_ptr<OAuth2TokenService::Request> access_token_request_;
  std::string account_id_;
  std::string access_token_;
  int retry_number_;
  base::OneShotTimer retry_timer_;
  bool second_access_token_request_;

  DISALLOW_COPY_AND_ASSIGN(UbertokenFetcherImpl);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_UBERTOKEN_FETCHER_IMPL_H_
