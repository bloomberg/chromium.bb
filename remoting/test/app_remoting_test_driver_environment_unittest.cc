// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/app_remoting_test_driver_environment.h"

#include "base/files/file_path.h"
#include "remoting/test/fake_access_token_fetcher.h"
#include "remoting/test/fake_remote_host_info_fetcher.h"
#include "remoting/test/mock_access_token_fetcher.h"
#include "remoting/test/refresh_token_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kAuthCodeValue[] = "4/892379827345jkefvkdfbv";
const char kRefreshTokenValue[] = "1/lkjalseLKJlsiJgr45jbv";
const char kUserNameValue[] = "remoting_user@gmail.com";
const char kTestApplicationId[] = "sadlkjlsjgadjfgoajdfgagb";
}

namespace remoting {
namespace test {

using testing::_;

// Stubs out the file API and returns fake data so we can remove
// file system dependencies when testing the TestDriverEnvironment.
class FakeRefreshTokenStore : public RefreshTokenStore {
 public:
  FakeRefreshTokenStore()
      : refresh_token_value_(kRefreshTokenValue),
        refresh_token_write_succeeded_(true),
        refresh_token_write_attempted_(false) {}
  ~FakeRefreshTokenStore() override {}

  std::string FetchRefreshToken() override { return refresh_token_value_; }

  bool StoreRefreshToken(const std::string& refresh_token) override {
    // Record the information passed to us to write.
    refresh_token_write_attempted_ = true;
    stored_refresh_token_value_ = refresh_token;

    return refresh_token_write_succeeded_;
  }

  bool refresh_token_write_attempted() const {
    return refresh_token_write_attempted_;
  }

  const std::string& stored_refresh_token_value() const {
    return stored_refresh_token_value_;
  }

  void set_refresh_token_value(const std::string& new_token_value) {
    refresh_token_value_ = new_token_value;
  }

  void set_refresh_token_write_succeeded(bool write_succeeded) {
    refresh_token_write_succeeded_ = write_succeeded;
  }

 private:
  // Control members used to return specific data to the caller.
  std::string refresh_token_value_;
  bool refresh_token_write_succeeded_;

  // Verification members to observe the value of the data being written.
  bool refresh_token_write_attempted_;
  std::string stored_refresh_token_value_;

  DISALLOW_COPY_AND_ASSIGN(FakeRefreshTokenStore);
};

class AppRemotingTestDriverEnvironmentTest : public ::testing::Test {
 public:
  AppRemotingTestDriverEnvironmentTest()
      : fake_access_token_fetcher_(nullptr),
        environment_object_(kUserNameValue,
                            base::FilePath(),  // refresh_token_file_path
                            kDeveloperEnvironment) {}
  ~AppRemotingTestDriverEnvironmentTest() override {}

  FakeAccessTokenFetcher* fake_access_token_fetcher() const {
    return fake_access_token_fetcher_;
  }

 protected:
  // testing::Test interface.
  void SetUp() override {
    scoped_ptr<FakeAccessTokenFetcher> fake_access_token_fetcher(
        new FakeAccessTokenFetcher());
    fake_access_token_fetcher_ = fake_access_token_fetcher.get();
    mock_access_token_fetcher_.SetAccessTokenFetcher(
        fake_access_token_fetcher.Pass());

    environment_object_.SetAccessTokenFetcherForTest(
        &mock_access_token_fetcher_);
    environment_object_.SetRefreshTokenStoreForTest(&fake_token_store_);
    environment_object_.SetRemoteHostInfoFetcherForTest(
        &fake_remote_host_info_fetcher_);
  }

  FakeRefreshTokenStore fake_token_store_;
  FakeRemoteHostInfoFetcher fake_remote_host_info_fetcher_;
  FakeAccessTokenFetcher* fake_access_token_fetcher_;
  MockAccessTokenFetcher mock_access_token_fetcher_;

  AppRemotingTestDriverEnvironment environment_object_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppRemotingTestDriverEnvironmentTest);
};

TEST_F(AppRemotingTestDriverEnvironmentTest, InitializeObjectWithAuthCode) {
  EXPECT_CALL(mock_access_token_fetcher_, GetAccessTokenFromAuthCode(_, _));

  EXPECT_CALL(mock_access_token_fetcher_, GetAccessTokenFromRefreshToken(_, _))
      .Times(0);

  EXPECT_TRUE(environment_object_.Initialize(kAuthCodeValue));
  EXPECT_TRUE(fake_token_store_.refresh_token_write_attempted());
  EXPECT_EQ(fake_token_store_.stored_refresh_token_value(),
            kFakeAccessTokenFetcherRefreshTokenValue);
  EXPECT_EQ(environment_object_.user_name(), kUserNameValue);
  EXPECT_EQ(environment_object_.access_token(),
            kFakeAccessTokenFetcherAccessTokenValue);

  // Attempt to init again, we should not see any additional calls or errors.
  EXPECT_TRUE(environment_object_.Initialize(kAuthCodeValue));
}

TEST_F(AppRemotingTestDriverEnvironmentTest,
       InitializeObjectWithAuthCodeFailed) {
  fake_access_token_fetcher()->set_fail_access_token_from_auth_code(true);

  EXPECT_CALL(mock_access_token_fetcher_, GetAccessTokenFromAuthCode(_, _));

  EXPECT_CALL(mock_access_token_fetcher_, GetAccessTokenFromRefreshToken(_, _))
      .Times(0);

  EXPECT_FALSE(environment_object_.Initialize(kAuthCodeValue));
  EXPECT_FALSE(fake_token_store_.refresh_token_write_attempted());
}

TEST_F(AppRemotingTestDriverEnvironmentTest, InitializeObjectWithRefreshToken) {
  EXPECT_CALL(mock_access_token_fetcher_, GetAccessTokenFromRefreshToken(_, _));

  EXPECT_CALL(mock_access_token_fetcher_, GetAccessTokenFromAuthCode(_, _))
      .Times(0);

  // Pass in an empty auth code since we are using a refresh token.
  EXPECT_TRUE(environment_object_.Initialize(std::string()));

  // We should not write the refresh token a second time if we read from the
  // disk originally.
  EXPECT_FALSE(fake_token_store_.refresh_token_write_attempted());

  // Verify the object was initialized correctly.
  EXPECT_EQ(environment_object_.user_name(), kUserNameValue);
  EXPECT_EQ(environment_object_.access_token(),
            kFakeAccessTokenFetcherAccessTokenValue);

  // Attempt to init again, we should not see any additional calls or errors.
  EXPECT_TRUE(environment_object_.Initialize(std::string()));
}

TEST_F(AppRemotingTestDriverEnvironmentTest,
       InitializeObjectWithRefreshTokenFailed) {
  fake_access_token_fetcher()->set_fail_access_token_from_refresh_token(true);

  EXPECT_CALL(mock_access_token_fetcher_, GetAccessTokenFromRefreshToken(_, _));

  EXPECT_CALL(mock_access_token_fetcher_, GetAccessTokenFromAuthCode(_, _))
      .Times(0);

  // Pass in an empty auth code since we are using a refresh token.
  EXPECT_FALSE(environment_object_.Initialize(std::string()));
  EXPECT_FALSE(fake_token_store_.refresh_token_write_attempted());
}

TEST_F(AppRemotingTestDriverEnvironmentTest,
       InitializeObjectNoAuthCodeOrRefreshToken) {
  // Neither method should be called in this scenario.
  EXPECT_CALL(mock_access_token_fetcher_, GetAccessTokenFromAuthCode(_, _))
      .Times(0);

  EXPECT_CALL(mock_access_token_fetcher_, GetAccessTokenFromRefreshToken(_, _))
      .Times(0);

  // Clear out the 'stored' refresh token value.
  fake_token_store_.set_refresh_token_value(std::string());

  // With no auth code or refresh token, then the initialization should fail.
  EXPECT_FALSE(environment_object_.Initialize(std::string()));
  EXPECT_FALSE(fake_token_store_.refresh_token_write_attempted());
}

TEST_F(AppRemotingTestDriverEnvironmentTest,
       InitializeObjectWithAuthCodeWriteFailed) {
  EXPECT_CALL(mock_access_token_fetcher_, GetAccessTokenFromAuthCode(_, _));

  EXPECT_CALL(mock_access_token_fetcher_, GetAccessTokenFromRefreshToken(_, _))
      .Times(0);

  // Simulate a failure writing the token to the disk.
  fake_token_store_.set_refresh_token_write_succeeded(false);

  EXPECT_FALSE(environment_object_.Initialize(kAuthCodeValue));
  EXPECT_TRUE(fake_token_store_.refresh_token_write_attempted());
}

TEST_F(AppRemotingTestDriverEnvironmentTest,
       RefreshAccessTokenAfterUsingAuthCode) {
  {
    testing::InSequence call_sequence;

    EXPECT_CALL(mock_access_token_fetcher_, GetAccessTokenFromAuthCode(_, _));

    EXPECT_CALL(mock_access_token_fetcher_,
                GetAccessTokenFromRefreshToken(_, _));
  }

  EXPECT_TRUE(environment_object_.Initialize(kAuthCodeValue));
  EXPECT_TRUE(fake_token_store_.refresh_token_write_attempted());
  EXPECT_EQ(fake_token_store_.stored_refresh_token_value(),
            kFakeAccessTokenFetcherRefreshTokenValue);
  EXPECT_EQ(environment_object_.user_name(), kUserNameValue);
  EXPECT_EQ(environment_object_.access_token(),
            kFakeAccessTokenFetcherAccessTokenValue);

  // Attempt to init again, we should not see any additional calls or errors.
  EXPECT_TRUE(environment_object_.RefreshAccessToken());
}

TEST_F(AppRemotingTestDriverEnvironmentTest, RefreshAccessTokenFailure) {
  {
    testing::InSequence call_sequence;

    // Mock is set up for this call to succeed.
    EXPECT_CALL(mock_access_token_fetcher_, GetAccessTokenFromAuthCode(_, _));

    // Mock is set up for this call to fail.
    EXPECT_CALL(mock_access_token_fetcher_,
                GetAccessTokenFromRefreshToken(_, _));
  }

  EXPECT_TRUE(environment_object_.Initialize(kAuthCodeValue));
  EXPECT_TRUE(fake_token_store_.refresh_token_write_attempted());
  EXPECT_EQ(fake_token_store_.stored_refresh_token_value(),
            kFakeAccessTokenFetcherRefreshTokenValue);
  EXPECT_EQ(environment_object_.user_name(), kUserNameValue);
  EXPECT_EQ(environment_object_.access_token(),
            kFakeAccessTokenFetcherAccessTokenValue);

  fake_access_token_fetcher()->set_fail_access_token_from_refresh_token(true);

  // We expect the refresh to have failed, the user name to remain valid,
  // and the access token to have been cleared.
  EXPECT_FALSE(environment_object_.RefreshAccessToken());
  EXPECT_TRUE(environment_object_.access_token().empty());
  EXPECT_EQ(environment_object_.user_name(), kUserNameValue);
}

TEST_F(AppRemotingTestDriverEnvironmentTest, GetRemoteHostInfoSuccess) {
  // Pass in an empty auth code since we are using a refresh token.
  EXPECT_TRUE(environment_object_.Initialize(std::string()));

  RemoteHostInfo remote_host_info;
  EXPECT_TRUE(environment_object_.GetRemoteHostInfoForApplicationId(
      kTestApplicationId, &remote_host_info));
  EXPECT_TRUE(remote_host_info.IsReadyForConnection());
}

TEST_F(AppRemotingTestDriverEnvironmentTest, GetRemoteHostInfoFailure) {
  // Pass in an empty auth code since we are using a refresh token.
  EXPECT_TRUE(environment_object_.Initialize(std::string()));

  fake_remote_host_info_fetcher_.set_fail_retrieve_remote_host_info(true);

  RemoteHostInfo remote_host_info;
  EXPECT_FALSE(environment_object_.GetRemoteHostInfoForApplicationId(
      kTestApplicationId, &remote_host_info));
}

TEST_F(AppRemotingTestDriverEnvironmentTest,
       GetRemoteHostInfoWithoutInitializing) {
  RemoteHostInfo remote_host_info;
  EXPECT_FALSE(environment_object_.GetRemoteHostInfoForApplicationId(
      kTestApplicationId, &remote_host_info));
}

}  // namespace test
}  // namespace remoting
