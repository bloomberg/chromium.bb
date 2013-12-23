// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "sync/test/accounts_client/test_accounts_client.cc"
#include "sync/test/accounts_client/test_accounts_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using std::string;
using std::vector;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;

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
  MOCK_METHOD2(SendRequest, bool(const GURL&, string*));
};

TEST(TestAccountsClientTest, ClaimAccountError) {
  vector<string> usernames;
  NoNetworkTestAccountsClient client(kServer, kAccountSpace, usernames);
  EXPECT_CALL(client, SendRequest(_, _))
      .WillOnce(Return(false));
  AccountSession session;
  EXPECT_FALSE(client.ClaimAccount(&session));
}

TEST(TestAccountsClientTest, ClaimAccountSuccess) {
  vector<string> usernames;
  usernames.push_back("foo0@gmail.com");
  usernames.push_back("foo1@gmail.com");
  usernames.push_back("foo2@gmail.com");
  NoNetworkTestAccountsClient client(kServer, kAccountSpace, usernames);

  base::DictionaryValue success_dict;
  success_dict.Set("username", new base::StringValue(kUsername));
  success_dict.Set("account_space", new base::StringValue(kAccountSpace));
  success_dict.Set("session_id", new base::StringValue(kSessionId));
  success_dict.Set("expiration_time", new base::StringValue(kExpirationTime));

  string success_response;
  base::JSONWriter::Write(&success_dict, &success_response);
  EXPECT_CALL(client, SendRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(success_response), Return(true)));

  AccountSession session;
  EXPECT_TRUE(client.ClaimAccount(&session));
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
  EXPECT_CALL(client, SendRequest(_, _))
      .WillOnce(Return(true));
  AccountSession session = CreateValidAccountSession();
  client.ReleaseAccount(session);
}
