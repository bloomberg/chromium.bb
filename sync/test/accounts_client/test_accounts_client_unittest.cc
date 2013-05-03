// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "sync/test/accounts_client/test_accounts_client.cc"
#include "sync/test/accounts_client/test_accounts_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;
using std::vector;
using testing::_;
using testing::HasSubstr;
using testing::Return;

namespace {
static const string kServer = "https://test-account-service";
static const string kUsername = "foobar@baz.com";
static const string kAccountSpace = "test_account_space";
static const string kSessionId = "1234-ABCD";
static const string kExpirationTime = "12:00";
} // namespace

static AccountSession CreateValidAccountSession() {
  AccountSession session;
  session.username = kUsername;
  session.account_space = kAccountSpace;
  session.session_id = kSessionId;
  session.expiration_time = kExpirationTime;
  return session;
}

class NoNetworkTestAccountsClient : public TestAccountsClient {
 public:
  NoNetworkTestAccountsClient(const string& server,
                              const string& account_space,
                              vector<string> usernames)
      : TestAccountsClient(server, account_space, usernames) {}
  MOCK_METHOD2(SendRequest,
               string(const string&, const string&));
};

TEST(TestAccountsClientTest, ClaimAccountError) {
  vector<string> usernames;
  NoNetworkTestAccountsClient client(kServer, kAccountSpace, usernames);
  string error_response = "error!!!";
  EXPECT_CALL(client, SendRequest(kClaimPath, _))
      .WillOnce(Return(error_response));
  AccountSession session = client.ClaimAccount();
  EXPECT_EQ(error_response, session.error);
}

TEST(TestAccountsClientTest, ClaimAccountSuccess) {
  vector<string> usernames;
  usernames.push_back("foo0@gmail.com");
  usernames.push_back("foo1@gmail.com");
  usernames.push_back("foo2@gmail.com");
  NoNetworkTestAccountsClient client(kServer, kAccountSpace, usernames);
  string success_response;
  base::StringAppendF(&success_response, "{ \"username\":\"%s\",",
                      kUsername.c_str());
  base::StringAppendF(&success_response, "\"account_space\":\"%s\",",
                      kAccountSpace.c_str());
  base::StringAppendF(&success_response, "\"session_id\":\"%s\",",
                      kSessionId.c_str());
  base::StringAppendF(&success_response, "\"expiration_time\":\"%s\"}",
                      kExpirationTime.c_str());
  EXPECT_CALL(client, SendRequest(kClaimPath, _))
      .WillOnce(Return(success_response));
  AccountSession session = client.ClaimAccount();
  EXPECT_EQ(kUsername, session.username);
  EXPECT_EQ(kAccountSpace, session.account_space);
  EXPECT_EQ(kSessionId, session.session_id);
  EXPECT_EQ(kExpirationTime, session.expiration_time);
}

TEST(TestAccountsClientTest, ReleaseAccountEmptySession) {
  vector<string> usernames;
  NoNetworkTestAccountsClient client(kServer, kAccountSpace, usernames);
  AccountSession session;
  // No expectation for SendRequest is made because no network call should be
  // performed in this scenario.
  client.ReleaseAccount(session);
}

TEST(TestAccountsClientTest, ReleaseAccountSuccess) {
  vector<string> usernames;
  NoNetworkTestAccountsClient client(kServer, kAccountSpace, usernames);
  EXPECT_CALL(client, SendRequest(kReleasePath, _))
      .WillOnce(Return(""));
  AccountSession session = CreateValidAccountSession();
  client.ReleaseAccount(session);
}
