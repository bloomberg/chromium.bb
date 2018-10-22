// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_FETCHER_IMPL_H_
#define GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_FETCHER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "url/gurl.h"

class OAuth2AccessTokenFetcherImplTest;

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}

// Abstracts the details to get OAuth2 access token from OAuth2 refresh token.
// See "Using the Refresh Token" section in:
// http://code.google.com/apis/accounts/docs/OAuth2WebServer.html
//
// This class should be used on a single thread, but it can be whichever thread
// that you like.
// Also, do not reuse the same instance. Once Start() is called, the instance
// should not be reused.
//
// Usage:
// * Create an instance with a consumer.
// * Call Start()
// * The consumer passed in the constructor will be called on the same
//   thread Start was called with the results.
//
// This class can handle one request at a time. To parallelize requests,
// create multiple instances.
class OAuth2AccessTokenFetcherImpl : public OAuth2AccessTokenFetcher {
 public:
  OAuth2AccessTokenFetcherImpl(
      OAuth2AccessTokenConsumer* consumer,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& refresh_token);
  ~OAuth2AccessTokenFetcherImpl() override;

  // Implementation of OAuth2AccessTokenFetcher
  void Start(const std::string& client_id,
             const std::string& client_secret,
             const std::vector<std::string>& scopes) override;

  void CancelRequest() override;

 private:
  enum State {
    INITIAL,
    GET_ACCESS_TOKEN_STARTED,
    GET_ACCESS_TOKEN_DONE,
    ERROR_STATE,
  };

  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  // Helper methods for the flow.
  void StartGetAccessToken();
  void EndGetAccessToken(std::unique_ptr<std::string> response_body);

  // Helper mehtods for reporting back results.
  void OnGetTokenSuccess(
      const OAuth2AccessTokenConsumer::TokenResponse& token_response);
  void OnGetTokenFailure(const GoogleServiceAuthError& error);

  // Other helpers.
  static GURL MakeGetAccessTokenUrl();
  static std::string MakeGetAccessTokenBody(
      const std::string& client_id,
      const std::string& client_secret,
      const std::string& refresh_token,
      const std::vector<std::string>& scopes);

  static bool ParseGetAccessTokenSuccessResponse(
      std::unique_ptr<std::string> response_body,
      std::string* access_token,
      int* expires_in,
      std::string* id_token);

  static bool ParseGetAccessTokenFailureResponse(
      std::unique_ptr<std::string> response_body,
      std::string* error);

  // State that is set during construction.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const std::string refresh_token_;
  State state_;

  // While a fetch is in progress.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  std::string client_id_;
  std::string client_secret_;
  std::vector<std::string> scopes_;

  friend class OAuth2AccessTokenFetcherImplTest;
  FRIEND_TEST_ALL_PREFIXES(OAuth2AccessTokenFetcherImplTest,
                           ParseGetAccessTokenResponseNoBody);
  FRIEND_TEST_ALL_PREFIXES(OAuth2AccessTokenFetcherImplTest,
                           MakeGetAccessTokenBodyNoScope);
  FRIEND_TEST_ALL_PREFIXES(OAuth2AccessTokenFetcherImplTest,
                           MakeGetAccessTokenBodyOneScope);
  FRIEND_TEST_ALL_PREFIXES(OAuth2AccessTokenFetcherImplTest,
                           MakeGetAccessTokenBodyMultipleScopes);
  FRIEND_TEST_ALL_PREFIXES(OAuth2AccessTokenFetcherImplTest,
                           ParseGetAccessTokenResponseBadJson);
  FRIEND_TEST_ALL_PREFIXES(OAuth2AccessTokenFetcherImplTest,
                           ParseGetAccessTokenResponseNoAccessToken);
  FRIEND_TEST_ALL_PREFIXES(OAuth2AccessTokenFetcherImplTest,
                           ParseGetAccessTokenResponseSuccess);
  FRIEND_TEST_ALL_PREFIXES(OAuth2AccessTokenFetcherImplTest,
                           ParseGetAccessTokenFailure);
  FRIEND_TEST_ALL_PREFIXES(OAuth2AccessTokenFetcherImplTest,
                           ParseGetAccessTokenFailureInvalidError);
  DISALLOW_COPY_AND_ASSIGN(OAuth2AccessTokenFetcherImpl);
};

#endif  // GOOGLE_APIS_GAIA_OAUTH2_ACCESS_TOKEN_FETCHER_IMPL_H_
