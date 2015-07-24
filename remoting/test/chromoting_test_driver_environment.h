// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_CHROMOTING_TEST_DRIVER_ENVIRONMENT_H_
#define REMOTING_TEST_CHROMOTING_TEST_DRIVER_ENVIRONMENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class MessageLoopForIO;
}

namespace remoting {
namespace test {

class AccessTokenFetcher;
class RefreshTokenStore;
class HostListFetcher;
struct HostInfo;

// Globally accessible to all test fixtures and cases and has its
// lifetime managed by the GTest framework. It is responsible for managing
// access tokens and retrieving the host list.
class ChromotingTestDriverEnvironment : public testing::Environment {
 public:
  struct EnvironmentOptions {
    EnvironmentOptions();
    ~EnvironmentOptions();

    std::string user_name;
    std::string host_name;
    base::FilePath refresh_token_file_path;
  };

  explicit ChromotingTestDriverEnvironment(const EnvironmentOptions& options);
  ~ChromotingTestDriverEnvironment() override;

  // Returns false if a valid access token cannot be retrieved.
  bool Initialize(const std::string& auth_code);

  // Retrieves connection information for all known hosts and displays
  // their availability to STDOUT.
  void DisplayHostList();

  // Used to set fake/mock objects for ChromotingTestDriverEnvironment tests.
  // The caller retains ownership of the supplied objects, and must ensure that
  // they remain valid until the ChromotingTestDriverEnvironment instance has
  // been destroyed.
  void SetAccessTokenFetcherForTest(AccessTokenFetcher* access_token_fetcher);
  void SetRefreshTokenStoreForTest(RefreshTokenStore* refresh_token_store);
  void SetHostListFetcherForTest(HostListFetcher* host_list_fetcher);

  // Accessors for fields used by tests.
  const std::string& access_token() const { return access_token_; }
  const std::string& host_name() const { return host_name_; }
  const std::string& user_name() const { return user_name_; }
  const std::vector<HostInfo>& host_list() const { return host_list_; }

 private:
  // testing::Environment interface.
  void TearDown() override;

  // Used to retrieve an access token.  If |auth_code| is empty, then the stored
  // refresh_token will be used instead of |auth_code|.
  // Returns true if a new, valid access token has been retrieved.
  bool RetrieveAccessToken(const std::string& auth_code);

  // Called after the access token fetcher completes.
  // The tokens will be empty on failure.
  void OnAccessTokenRetrieved(base::Closure done_closure,
                              const std::string& retrieved_access_token,
                              const std::string& retrieved_refresh_token);

  // Used to retrieve a host list from the directory service.
  // Returns true if the request was successful and |host_list_| is valid.
  bool RetrieveHostList();

  // Called after the host info fetcher completes.
  void OnHostListRetrieved(base::Closure done_closure,
                           const std::vector<HostInfo>& retrieved_host_list);

  // Used for authenticating with the directory service.
  std::string access_token_;

  // Used to retrieve an access token.
  std::string refresh_token_;

  // Used to find remote host in host list.
  std::string host_name_;

  // The test account for a test case.
  std::string user_name_;

  // Path to a JSON file containing refresh tokens.
  base::FilePath refresh_token_file_path_;

  // List of remote hosts for the specified user/test-account.
  std::vector<HostInfo> host_list_;

  // Access token fetcher used by TestDriverEnvironment tests.
  remoting::test::AccessTokenFetcher* test_access_token_fetcher_;

  // RefreshTokenStore used by TestDriverEnvironment tests.
  remoting::test::RefreshTokenStore* test_refresh_token_store_;

  // HostListFetcher used by TestDriverEnvironment tests.
  remoting::test::HostListFetcher* test_host_list_fetcher_;

  // Used for running network request tasks.
  scoped_ptr<base::MessageLoopForIO> message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingTestDriverEnvironment);
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_CHROMOTING_TEST_DRIVER_ENVIRONMENT_H_
