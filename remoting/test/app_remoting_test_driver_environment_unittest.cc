// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/app_remoting_test_driver_environment.h"

#include "remoting/test/fake_access_token_fetcher.h"
#include "remoting/test/mock_access_token_fetcher.h"
#include "remoting/test/refresh_token_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kAuthCodeValue[] = "4/892379827345jkefvkdfbv";
const char kRefreshTokenValue[] = "1/lkjalseLKJlsiJgr45jbv";
const char kUserNameValue[] = "remoting_user@gmail.com";
const char kDevEnvironmentValue[] = "dev";
}

namespace remoting {
namespace test {

using testing::_;

// Stubs out the file API and returns fake data so we can remove
// file system dependencies when testing the TestDriverEnvironment.
class FakeRefreshTokenStore : public RefreshTokenStore {
 public:
  FakeRefreshTokenStore() {
    // Set some success defaults.
    refresh_token_value = kRefreshTokenValue;
    refresh_token_write_attempted = false;
    refresh_token_write_succeeded = true;
  }
  ~FakeRefreshTokenStore() override {}

  std::string FetchRefreshToken() override {
    return refresh_token_value;
  };

  bool StoreRefreshToken(const std::string& refresh_token) override {
    // Record the information passed to us to write.
    refresh_token_write_attempted = true;
    refresh_token_value_written = refresh_token;

    return refresh_token_write_succeeded;
  };

  // Control members used to return specific data to the caller.
  std::string refresh_token_value;
  bool refresh_token_write_succeeded;

  // Verification members to observe the value of the data being written.
  bool refresh_token_write_attempted;
  std::string refresh_token_value_written;

  DISALLOW_COPY_AND_ASSIGN(FakeRefreshTokenStore);
};

TEST(AppRemotingTestDriverEnvironmentTest, InitializeObjectWithAuthCode) {
  MockAccessTokenFetcher mock_access_token_fetcher;

  mock_access_token_fetcher.SetAccessTokenFetcher(
      make_scoped_ptr(new FakeAccessTokenFetcher()));

  FakeRefreshTokenStore fake_token_store;

  EXPECT_CALL(mock_access_token_fetcher, GetAccessTokenFromAuthCode(_, _))
      .Times(1);

  EXPECT_CALL(mock_access_token_fetcher, GetAccessTokenFromRefreshToken(_, _))
      .Times(0);

  AppRemotingTestDriverEnvironment environment_object(
      kUserNameValue,
      kDevEnvironmentValue);

  environment_object.SetAccessTokenFetcherForTest(&mock_access_token_fetcher);

  environment_object.SetRefreshTokenStoreForTest(&fake_token_store);

  bool init_result = environment_object.Initialize(kAuthCodeValue);

  EXPECT_TRUE(init_result);
  EXPECT_TRUE(fake_token_store.refresh_token_write_attempted);
  EXPECT_STREQ(kFakeAccessTokenFetcherRefreshTokenValue,
               fake_token_store.refresh_token_value_written.c_str());
  EXPECT_STREQ(kUserNameValue, environment_object.user_name().c_str());
  EXPECT_STREQ(kFakeAccessTokenFetcherAccessTokenValue,
               environment_object.access_token().c_str());

  // Attempt to init again, we should not see any additional calls or errors.
  init_result = environment_object.Initialize(kAuthCodeValue);
  EXPECT_TRUE(init_result);
}

TEST(AppRemotingTestDriverEnvironmentTest, InitializeObjectWithAuthCodeFailed) {
  MockAccessTokenFetcher mock_access_token_fetcher;

  scoped_ptr<FakeAccessTokenFetcher> fake_access_token_fetcher(
      new FakeAccessTokenFetcher());

  fake_access_token_fetcher->set_fail_access_token_from_auth_code(true);

  mock_access_token_fetcher.SetAccessTokenFetcher(
      fake_access_token_fetcher.Pass());

  FakeRefreshTokenStore fake_token_store;

  EXPECT_CALL(mock_access_token_fetcher, GetAccessTokenFromAuthCode(_, _))
      .Times(1);

  EXPECT_CALL(mock_access_token_fetcher, GetAccessTokenFromRefreshToken(_, _))
      .Times(0);

  AppRemotingTestDriverEnvironment environment_object(
      kUserNameValue,
      kDevEnvironmentValue);

  environment_object.SetAccessTokenFetcherForTest(&mock_access_token_fetcher);

  environment_object.SetRefreshTokenStoreForTest(&fake_token_store);

  bool init_result = environment_object.Initialize(kAuthCodeValue);

  EXPECT_FALSE(init_result);
  EXPECT_FALSE(fake_token_store.refresh_token_write_attempted);
}

TEST(AppRemotingTestDriverEnvironmentTest, InitializeObjectWithRefreshToken) {
  MockAccessTokenFetcher mock_access_token_fetcher;

  mock_access_token_fetcher.SetAccessTokenFetcher(
      make_scoped_ptr(new FakeAccessTokenFetcher()));

  FakeRefreshTokenStore fake_token_store;

  EXPECT_CALL(mock_access_token_fetcher, GetAccessTokenFromRefreshToken(_, _))
      .Times(1);

  EXPECT_CALL(mock_access_token_fetcher, GetAccessTokenFromAuthCode(_, _))
      .Times(0);

  AppRemotingTestDriverEnvironment environment_object(
      kUserNameValue,
      kDevEnvironmentValue);

  environment_object.SetAccessTokenFetcherForTest(&mock_access_token_fetcher);

  environment_object.SetRefreshTokenStoreForTest(&fake_token_store);

  // Pass in an empty auth code since we are using a refresh token.
  bool init_result = environment_object.Initialize(std::string());

  EXPECT_TRUE(init_result);

  // We should not write the refresh token a second time if we read from the
  // disk originally.
  EXPECT_FALSE(fake_token_store.refresh_token_write_attempted);

  // Verify the object was initialized correctly.
  EXPECT_STREQ(kUserNameValue, environment_object.user_name().c_str());
  EXPECT_STREQ(kFakeAccessTokenFetcherAccessTokenValue,
               environment_object.access_token().c_str());

  // Attempt to init again, we should not see any additional calls or errors.
  init_result = environment_object.Initialize(std::string());
  EXPECT_TRUE(init_result);
}

TEST(AppRemotingTestDriverEnvironmentTest,
     InitializeObjectWithRefreshTokenFailed) {
  MockAccessTokenFetcher mock_access_token_fetcher;

  scoped_ptr<FakeAccessTokenFetcher> fake_access_token_fetcher(
      new FakeAccessTokenFetcher());

  fake_access_token_fetcher->set_fail_access_token_from_refresh_token(true);

  mock_access_token_fetcher.SetAccessTokenFetcher(
      fake_access_token_fetcher.Pass());

  FakeRefreshTokenStore fake_token_store;

  EXPECT_CALL(mock_access_token_fetcher, GetAccessTokenFromRefreshToken(_, _))
      .Times(1);

  EXPECT_CALL(mock_access_token_fetcher, GetAccessTokenFromAuthCode(_, _))
      .Times(0);

  AppRemotingTestDriverEnvironment environment_object(
      kUserNameValue,
      kDevEnvironmentValue);

  environment_object.SetAccessTokenFetcherForTest(&mock_access_token_fetcher);

  environment_object.SetRefreshTokenStoreForTest(&fake_token_store);

  // Pass in an empty auth code since we are using a refresh token.
  bool init_result = environment_object.Initialize(std::string());

  EXPECT_FALSE(init_result);
  EXPECT_FALSE(fake_token_store.refresh_token_write_attempted);
}

TEST(AppRemotingTestDriverEnvironmentTest,
     InitializeObjectNoAuthCodeOrRefreshToken) {
  MockAccessTokenFetcher mock_access_token_fetcher;

  mock_access_token_fetcher.SetAccessTokenFetcher(
      make_scoped_ptr(new FakeAccessTokenFetcher()));

  FakeRefreshTokenStore fake_token_store;

  // Neither method should be called in this scenario.
  EXPECT_CALL(mock_access_token_fetcher, GetAccessTokenFromAuthCode(_, _))
      .Times(0);

  EXPECT_CALL(mock_access_token_fetcher, GetAccessTokenFromRefreshToken(_, _))
      .Times(0);

  AppRemotingTestDriverEnvironment environment_object(
      kUserNameValue,
      kDevEnvironmentValue);

  environment_object.SetAccessTokenFetcherForTest(&mock_access_token_fetcher);

  environment_object.SetRefreshTokenStoreForTest(&fake_token_store);

  // Clear out the 'stored' refresh token value.
  fake_token_store.refresh_token_value = "";

  // Pass in an empty auth code.
  bool init_result = environment_object.Initialize(std::string());

  // With no auth code or refresh token, then the initialization should fail.
  EXPECT_FALSE(init_result);
  EXPECT_FALSE(fake_token_store.refresh_token_write_attempted);
}

TEST(AppRemotingTestDriverEnvironmentTest,
     InitializeObjectWithAuthCodeWriteFailed) {
  MockAccessTokenFetcher mock_access_token_fetcher;

  mock_access_token_fetcher.SetAccessTokenFetcher(
      make_scoped_ptr(new FakeAccessTokenFetcher()));

  FakeRefreshTokenStore fake_token_store;

  EXPECT_CALL(mock_access_token_fetcher, GetAccessTokenFromAuthCode(_, _))
      .Times(1);

  EXPECT_CALL(mock_access_token_fetcher, GetAccessTokenFromRefreshToken(_, _))
      .Times(0);

  AppRemotingTestDriverEnvironment environment_object(
      kUserNameValue,
      kDevEnvironmentValue);

  environment_object.SetAccessTokenFetcherForTest(&mock_access_token_fetcher);

  environment_object.SetRefreshTokenStoreForTest(&fake_token_store);

  // Simulate a failure writing the token to the disk.
  fake_token_store.refresh_token_write_succeeded = false;

  bool init_result = environment_object.Initialize(kAuthCodeValue);

  EXPECT_FALSE(init_result);
  EXPECT_TRUE(fake_token_store.refresh_token_write_attempted);
}

TEST(AppRemotingTestDriverEnvironmentTest,
     RefreshAccessTokenAfterUsingAuthCode) {
  MockAccessTokenFetcher mock_access_token_fetcher;

  mock_access_token_fetcher.SetAccessTokenFetcher(
      make_scoped_ptr(new FakeAccessTokenFetcher()));

  FakeRefreshTokenStore fake_token_store;

  {
    testing::Sequence call_sequence;

    EXPECT_CALL(mock_access_token_fetcher, GetAccessTokenFromAuthCode(_, _))
        .Times(1);

    EXPECT_CALL(mock_access_token_fetcher,
                GetAccessTokenFromRefreshToken(_, _)).Times(1);
  }

  AppRemotingTestDriverEnvironment environment_object(
      kUserNameValue,
      kDevEnvironmentValue);

  environment_object.SetAccessTokenFetcherForTest(&mock_access_token_fetcher);

  environment_object.SetRefreshTokenStoreForTest(&fake_token_store);

  bool init_result = environment_object.Initialize(kAuthCodeValue);

  EXPECT_TRUE(init_result);
  EXPECT_TRUE(fake_token_store.refresh_token_write_attempted);
  EXPECT_STREQ(kFakeAccessTokenFetcherRefreshTokenValue,
               fake_token_store.refresh_token_value_written.c_str());
  EXPECT_STREQ(kUserNameValue, environment_object.user_name().c_str());
  EXPECT_STREQ(kFakeAccessTokenFetcherAccessTokenValue,
               environment_object.access_token().c_str());

  // Attempt to init again, we should not see any additional calls or errors.
  bool refresh_result = environment_object.RefreshAccessToken();
  EXPECT_TRUE(refresh_result);
}

TEST(AppRemotingTestDriverEnvironmentTest, RefreshAccessTokenFailure) {
  MockAccessTokenFetcher mock_access_token_fetcher;

  // Use a raw pointer as we want to adjust behavior after we've handed off the
  // mock class.
  FakeAccessTokenFetcher* fake_access_token_fetcher =
      new FakeAccessTokenFetcher();
  mock_access_token_fetcher.SetAccessTokenFetcher(
      make_scoped_ptr(fake_access_token_fetcher));

  FakeRefreshTokenStore fake_token_store;

  {
    testing::Sequence call_sequence;

    // Mock is set up for this call to succeed.
    EXPECT_CALL(mock_access_token_fetcher, GetAccessTokenFromAuthCode(_, _))
        .Times(1);

    // Mock is set up for this call to fail.
    EXPECT_CALL(mock_access_token_fetcher,
                GetAccessTokenFromRefreshToken(_, _)).Times(1);
  }

  AppRemotingTestDriverEnvironment environment_object(
      kUserNameValue,
      kDevEnvironmentValue);

  environment_object.SetAccessTokenFetcherForTest(&mock_access_token_fetcher);

  environment_object.SetRefreshTokenStoreForTest(&fake_token_store);

  bool init_result = environment_object.Initialize(kAuthCodeValue);

  EXPECT_TRUE(init_result);
  EXPECT_TRUE(fake_token_store.refresh_token_write_attempted);
  EXPECT_STREQ(kFakeAccessTokenFetcherRefreshTokenValue,
               fake_token_store.refresh_token_value_written.c_str());
  EXPECT_STREQ(kUserNameValue, environment_object.user_name().c_str());
  EXPECT_STREQ(kFakeAccessTokenFetcherAccessTokenValue,
               environment_object.access_token().c_str());

  fake_access_token_fetcher->set_fail_access_token_from_refresh_token(true);

  bool refresh_result = environment_object.RefreshAccessToken();

  // We expect the refresh to have failed, the user name to remain valid,
  // and the access token to have been cleared.
  EXPECT_FALSE(refresh_result);
  EXPECT_STREQ(kUserNameValue, environment_object.user_name().c_str());
  EXPECT_STREQ("", environment_object.access_token().c_str());
}

}  // namespace test
}  // namespace remoting
