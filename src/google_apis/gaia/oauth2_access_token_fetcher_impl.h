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
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

class OAuth2AccessTokenFetcherImplTest;

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}

// Abstracts the details to get OAuth2 access token from OAuth2 refresh token.
// See general document about Oauth2 in https://tools.ietf.org/html/rfc6749.
//
// This class should be used on a single thread, but it can be whichever thread
// that you like. Also, do not reuse the same instance. Once Start() is called,
// the instance should not be reused.
//
// Usage:
// * Create an instance with a consumer.
// * Call Start()
// * The consumer passed in the constructor will be called on the same
//   thread Start was called with the results.
//
// This class can handle one request at a time. To parallelize requests,
// create multiple instances.  Prefer `GaiaAccessTokenFetcher` over this class
// while talking to Google's OAuth servers.
class OAuth2AccessTokenFetcherImpl : public OAuth2AccessTokenFetcher {
 public:
  // Enumerated constants for logging server responses on 400 errors, matching
  // RFC 6749.
  enum OAuth2ErrorCodesForHistogram {
    OAUTH2_ACCESS_ERROR_INVALID_REQUEST = 0,
    OAUTH2_ACCESS_ERROR_INVALID_CLIENT,
    OAUTH2_ACCESS_ERROR_INVALID_GRANT,
    OAUTH2_ACCESS_ERROR_UNAUTHORIZED_CLIENT,
    OAUTH2_ACCESS_ERROR_UNSUPPORTED_GRANT_TYPE,
    OAUTH2_ACCESS_ERROR_INVALID_SCOPE,
    OAUTH2_ACCESS_ERROR_UNKNOWN,
    OAUTH2_ACCESS_ERROR_COUNT,
  };

  OAuth2AccessTokenFetcherImpl(
      OAuth2AccessTokenConsumer* consumer,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& refresh_token,
      const std::string& auth_code = std::string());
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

  // Derived classes override these methods to record UMA histograms if needed.
  virtual void RecordResponseCodeUma(int error_value) const {}
  virtual void RecordBadRequestTypeUma(
      OAuth2ErrorCodesForHistogram access_error) const {}

  // Derived class must specify the GetAccessToken base URL to use.
  virtual GURL GetAccessTokenURL() const = 0;

  // Derived classes must specify a network annotation for the fetcher.
  virtual net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() const = 0;

  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  // Helper methods for the flow.
  void StartGetAccessToken();
  void EndGetAccessToken(std::unique_ptr<std::string> response_body);

  // Helper mehtods for reporting back results.
  void OnGetTokenSuccess(
      const OAuth2AccessTokenConsumer::TokenResponse& token_response);
  void OnGetTokenFailure(const GoogleServiceAuthError& error);

  static std::string MakeGetAccessTokenBody(
      const std::string& client_id,
      const std::string& client_secret,
      const std::string& refresh_token,
      const std::string& auth_code,
      const std::vector<std::string>& scopes);

  static bool ParseGetAccessTokenSuccessResponse(
      std::unique_ptr<std::string> response_body,
      OAuth2AccessTokenConsumer::TokenResponse* token_response);

  static bool ParseGetAccessTokenFailureResponse(
      std::unique_ptr<std::string> response_body,
      std::string* error);

  // State that is set during construction.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const std::string refresh_token_;
  const std::string auth_code_;
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
