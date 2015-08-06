// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/chromoting_test_driver_environment.h"

#include "base/files/file_path.h"
#include "remoting/test/fake_access_token_fetcher.h"
#include "remoting/test/fake_host_list_fetcher.h"
#include "remoting/test/fake_refresh_token_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kAuthCodeValue[] = "4/892379827345jkefvkdfbv";
const char kUserNameValue[] = "remoting_user@gmail.com";
const char kHostNameValue[] = "fake_host_name";
const char kFakeHostNameValue[] = "fake_host_name";
const char kFakeHostIdValue[] = "fake_host_id";
const char kFakeHostJidValue[] = "fake_host_jid";
const char kFakeHostOfflineReasonValue[] = "fake_offline_reason";
const char kFakeHostPublicKeyValue[] = "fake_public_key";
const char kFakeHostFirstTokenUrlValue[] = "token_url_1";
const char kFakeHostSecondTokenUrlValue[] = "token_url_2";
const char kFakeHostThirdTokenUrlValue[] = "token_url_3";
const unsigned int kExpectedHostListSize = 1;
}  // namespace

namespace remoting {
namespace test {

class ChromotingTestDriverEnvironmentTest : public ::testing::Test {
 public:
  ChromotingTestDriverEnvironmentTest();
  ~ChromotingTestDriverEnvironmentTest() override;

 protected:
  // testing::Test interface.
  void SetUp() override;

  FakeAccessTokenFetcher fake_access_token_fetcher_;
  FakeRefreshTokenStore fake_token_store_;
  FakeHostListFetcher fake_host_list_fetcher_;

  scoped_ptr<ChromotingTestDriverEnvironment> environment_object_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromotingTestDriverEnvironmentTest);
};

ChromotingTestDriverEnvironmentTest::ChromotingTestDriverEnvironmentTest() {
}

ChromotingTestDriverEnvironmentTest::~ChromotingTestDriverEnvironmentTest() {
}

void ChromotingTestDriverEnvironmentTest::SetUp() {
  ChromotingTestDriverEnvironment::EnvironmentOptions options;
  options.user_name = kUserNameValue;
  options.host_name = kHostNameValue;

  environment_object_.reset(new ChromotingTestDriverEnvironment(options));

  std::vector<HostInfo> fake_host_list;
  HostInfo fake_host;
  fake_host.host_id = kFakeHostIdValue;
  fake_host.host_jid = kFakeHostJidValue;
  fake_host.host_name = kFakeHostNameValue;
  fake_host.offline_reason = kFakeHostOfflineReasonValue;
  fake_host.public_key = kFakeHostPublicKeyValue;
  fake_host.status = kHostStatusOnline;
  fake_host.token_url_patterns.push_back(kFakeHostFirstTokenUrlValue);
  fake_host.token_url_patterns.push_back(kFakeHostSecondTokenUrlValue);
  fake_host.token_url_patterns.push_back(kFakeHostThirdTokenUrlValue);
  fake_host_list.push_back(fake_host);

  fake_host_list_fetcher_.set_retrieved_host_list(fake_host_list);

  environment_object_->SetAccessTokenFetcherForTest(
      &fake_access_token_fetcher_);
  environment_object_->SetRefreshTokenStoreForTest(&fake_token_store_);
  environment_object_->SetHostListFetcherForTest(&fake_host_list_fetcher_);
}

TEST_F(ChromotingTestDriverEnvironmentTest, InitializeObjectWithAuthCode) {
  // Pass in an auth code to initialize the environment.
  EXPECT_TRUE(environment_object_->Initialize(kAuthCodeValue));

  EXPECT_TRUE(fake_token_store_.refresh_token_write_attempted());
  EXPECT_EQ(fake_token_store_.stored_refresh_token_value(),
            kFakeAccessTokenFetcherRefreshTokenValue);

  EXPECT_EQ(environment_object_->user_name(), kUserNameValue);
  EXPECT_EQ(environment_object_->host_name(), kHostNameValue);
  EXPECT_EQ(environment_object_->access_token(),
            kFakeAccessTokenFetcherAccessTokenValue);

  // Should only have one host in the list.
  EXPECT_EQ(environment_object_->host_list().size(), kExpectedHostListSize);
  HostInfo fake_host = environment_object_->host_list().at(0);
  EXPECT_EQ(fake_host.host_id, kFakeHostIdValue);
  EXPECT_EQ(fake_host.host_jid, kFakeHostJidValue);
  EXPECT_EQ(fake_host.host_name, kFakeHostNameValue);
  EXPECT_EQ(fake_host.public_key, kFakeHostPublicKeyValue);
  EXPECT_EQ(fake_host.token_url_patterns.at(0), kFakeHostFirstTokenUrlValue);
  EXPECT_EQ(fake_host.token_url_patterns.at(1), kFakeHostSecondTokenUrlValue);
  EXPECT_EQ(fake_host.token_url_patterns.at(2), kFakeHostThirdTokenUrlValue);
}

TEST_F(ChromotingTestDriverEnvironmentTest,
       InitializeObjectWithAuthCodeFailed) {
  fake_access_token_fetcher_.set_fail_access_token_from_auth_code(true);

  EXPECT_FALSE(environment_object_->Initialize(kAuthCodeValue));
  EXPECT_FALSE(fake_token_store_.refresh_token_write_attempted());
}

TEST_F(ChromotingTestDriverEnvironmentTest, InitializeObjectWithRefreshToken) {
  // Pass in an empty auth code since we are using a refresh token.
  EXPECT_TRUE(environment_object_->Initialize(std::string()));

  // We should not write the refresh token a second time if we read from the
  // disk originally.
  EXPECT_FALSE(fake_token_store_.refresh_token_write_attempted());

  // Verify the object was initialized correctly.
  EXPECT_EQ(environment_object_->user_name(), kUserNameValue);
  EXPECT_EQ(environment_object_->host_name(), kHostNameValue);
  EXPECT_EQ(environment_object_->access_token(),
            kFakeAccessTokenFetcherAccessTokenValue);

  // Should only have one host in the list.
  EXPECT_EQ(environment_object_->host_list().size(), kExpectedHostListSize);
  HostInfo fake_host = environment_object_->host_list().at(0);
  EXPECT_EQ(fake_host.host_id, kFakeHostIdValue);
  EXPECT_EQ(fake_host.host_jid, kFakeHostJidValue);
  EXPECT_EQ(fake_host.host_name, kFakeHostNameValue);
  EXPECT_EQ(fake_host.public_key, kFakeHostPublicKeyValue);
  EXPECT_EQ(fake_host.token_url_patterns.at(0), kFakeHostFirstTokenUrlValue);
  EXPECT_EQ(fake_host.token_url_patterns.at(1), kFakeHostSecondTokenUrlValue);
  EXPECT_EQ(fake_host.token_url_patterns.at(2), kFakeHostThirdTokenUrlValue);
}

TEST_F(ChromotingTestDriverEnvironmentTest,
       InitializeObjectWithRefreshTokenFailed) {
  fake_access_token_fetcher_.set_fail_access_token_from_refresh_token(true);

  // Pass in an empty auth code since we are using a refresh token.
  EXPECT_FALSE(environment_object_->Initialize(std::string()));
  EXPECT_FALSE(fake_token_store_.refresh_token_write_attempted());
}

TEST_F(ChromotingTestDriverEnvironmentTest, TearDownAfterInitializeSucceeds) {
  // Pass in an empty auth code since we are using a refresh token.
  EXPECT_TRUE(environment_object_->Initialize(std::string()));

  // Note: We are using a static cast here because the TearDown() method is
  //       private as it is an interface method that we only want to call
  //       directly in tests or by the GTEST framework.
  static_cast<testing::Environment*>(environment_object_.get())->TearDown();
}

TEST_F(ChromotingTestDriverEnvironmentTest,
       InitializeObjectNoAuthCodeOrRefreshToken) {
  // Clear out the 'stored' refresh token value.
  fake_token_store_.set_refresh_token_value(std::string());

  // With no auth code or refresh token, then the initialization should fail.
  EXPECT_FALSE(environment_object_->Initialize(std::string()));
  EXPECT_FALSE(fake_token_store_.refresh_token_write_attempted());
}

TEST_F(ChromotingTestDriverEnvironmentTest,
       InitializeObjectWithAuthCodeWriteFailed) {
  // Simulate a failure writing the token to the disk.
  fake_token_store_.set_refresh_token_write_succeeded(false);

  EXPECT_FALSE(environment_object_->Initialize(kAuthCodeValue));
  EXPECT_TRUE(fake_token_store_.refresh_token_write_attempted());
}

TEST_F(ChromotingTestDriverEnvironmentTest, HostListEmptyFromDirectory) {
  // Set the host list fetcher to return an empty list.
  fake_host_list_fetcher_.set_retrieved_host_list(std::vector<HostInfo>());

  EXPECT_FALSE(environment_object_->Initialize(kAuthCodeValue));
  EXPECT_TRUE(fake_token_store_.refresh_token_write_attempted());
}

}  // namespace test
}  // namespace remoting
