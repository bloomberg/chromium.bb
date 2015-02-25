// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_APP_REMOTING_TEST_DRIVER_ENVIRONMENT_H_
#define REMOTING_TEST_APP_REMOTING_TEST_DRIVER_ENVIRONMENT_H_

#include "base/memory/scoped_ptr.h"
#include "remoting/test/access_token_fetcher.h"
#include "remoting/test/refresh_token_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace test {

// Globally accessible to all test fixtures and cases and has its
// lifetime managed by the GTest framework.  It is responsible for managing
// access tokens and caching remote host connection information.
// TODO(joedow) Add remote host connection functionality.
class AppRemotingTestDriverEnvironment : public testing::Environment {
 public:
  AppRemotingTestDriverEnvironment(
      const std::string& user_name,
      const std::string& service_environment);
  ~AppRemotingTestDriverEnvironment() override;

  // Returns false if a valid access token cannot be retrieved.
  bool Initialize(const std::string& auth_code);

  // Synchronously request a new access token using |refresh_token_|.
  // Returns true if a valid access token has been retrieved.
  bool RefreshAccessToken();

  // Used to set fake/mock objects for AppRemotingTestDriverEnvironment tests.
  void SetAccessTokenFetcherForTest(AccessTokenFetcher* access_token_fetcher);
  void SetRefreshTokenStoreForTest(RefreshTokenStore* refresh_token_store);

  // Accessors for fields used by tests.
  const std::string& access_token() const { return access_token_; }
  const std::string& user_name() const { return user_name_; }

 private:
  // Used to retrieve an access token.  If |auth_code| is empty, then the stored
  // refresh_token will be used instead of |auth_code|.
  // Returns true if a new, valid access token has been retrieved.
  bool RetrieveAccessToken(const std::string& auth_code);

  // Called after the access token fetcher completes.
  // The tokens will be empty on failure.
  void OnAccessTokenRetrieved(
    base::Closure done_closure,
    const std::string& access_token,
    const std::string& refresh_token);

  // Used for authenticating with the app remoting service API.
  std::string access_token_;

  // Used to retrieve an access token.
  std::string refresh_token_;

  // Used for authentication.
  std::string user_name_;

  // Service API to target when retrieving remote host connection information.
  // TODO(joedow): Turn this into an enum when remote_host code is added.
  std::string service_environment_;

  // Access token fetcher used by TestDriverEnvironment tests.
  remoting::test::AccessTokenFetcher* test_access_token_fetcher_;

  // RefreshTokenStore used by TestDriverEnvironment tests.
  remoting::test::RefreshTokenStore* test_refresh_token_store_;

  DISALLOW_COPY_AND_ASSIGN(AppRemotingTestDriverEnvironment);
};

// Unfortunately a global var is how the GTEST framework handles sharing data
// between tests and keeping long-lived objects around.  Used to share auth
// tokens and remote host connection information across tests.
extern AppRemotingTestDriverEnvironment* AppRemotingSharedData;

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_APP_REMOTING_TEST_DRIVER_ENVIRONMENT_H_
