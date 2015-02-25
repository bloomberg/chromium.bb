// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/app_remoting_test_driver_environment.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"

namespace remoting {
namespace test {

AppRemotingTestDriverEnvironment* AppRemotingSharedData;

AppRemotingTestDriverEnvironment::AppRemotingTestDriverEnvironment(
    const std::string& user_name,
    const std::string& service_environment)
    : user_name_(user_name),
      service_environment_(service_environment),
      test_access_token_fetcher_(nullptr),
      test_refresh_token_store_(nullptr) {
  DCHECK(!user_name_.empty());
  DCHECK(!service_environment.empty());
}

AppRemotingTestDriverEnvironment::~AppRemotingTestDriverEnvironment() {
}

bool AppRemotingTestDriverEnvironment::Initialize(
    const std::string& auth_code) {
  if (!access_token_.empty()) {
    return true;
  }

  // If a unit test has set |test_refresh_token_store_| then we should use it
  // below.  Note that we do not want to destroy the test object.
  scoped_ptr<RefreshTokenStore> temporary_refresh_token_store;
  RefreshTokenStore* refresh_token_store = test_refresh_token_store_;
  if (!refresh_token_store) {
    temporary_refresh_token_store = RefreshTokenStore::OnDisk(user_name_);
    refresh_token_store = temporary_refresh_token_store.get();
  }

  // Check to see if we have a refresh token stored for this user.
  refresh_token_ = refresh_token_store->FetchRefreshToken();
  if (refresh_token_.empty()) {
    // This isn't necessarily an error as this might be a first run scenario.
    DVLOG(1) << "No refresh token stored for " << user_name_;

    if (auth_code.empty()) {
      // No token and no Auth code means no service connectivity, bail!
      LOG(ERROR) << "Cannot retrieve an access token without a stored refresh"
                 << " token on disk or an auth_code passed into the tool";
      return false;
    }
  }

  if (!RetrieveAccessToken(auth_code)) {
    // If we cannot retrieve an access token, then nothing is going to work and
    // we should let the caller know that our object is not ready to be used.
    return false;
  }

  return true;
}

bool AppRemotingTestDriverEnvironment::RefreshAccessToken() {
  DCHECK(!refresh_token_.empty());

  // Empty auth code is used when refreshing.
  return RetrieveAccessToken(std::string());
}

void AppRemotingTestDriverEnvironment::SetAccessTokenFetcherForTest(
    AccessTokenFetcher* access_token_fetcher) {
  DCHECK(access_token_fetcher);

  test_access_token_fetcher_ = access_token_fetcher;
}

void AppRemotingTestDriverEnvironment::SetRefreshTokenStoreForTest(
    RefreshTokenStore* refresh_token_store) {
  DCHECK(refresh_token_store);

  test_refresh_token_store_ = refresh_token_store;
}

bool AppRemotingTestDriverEnvironment::RetrieveAccessToken(
    const std::string& auth_code) {
  scoped_ptr<base::MessageLoopForIO> message_loop;

  if (!base::MessageLoop::current()) {
    // Create a temporary message loop if the current thread does not already
    // have one so we can use its task runner for our network request.
    message_loop.reset(new base::MessageLoopForIO);
  }

  base::RunLoop run_loop;

  access_token_.clear();

  AccessTokenCallback access_token_callback =
      base::Bind(&AppRemotingTestDriverEnvironment::OnAccessTokenRetrieved,
                 base::Unretained(this),
                 run_loop.QuitClosure());

  // If a unit test has set |test_access_token_fetcher_| then we should use it
  // below.  Note that we do not want to destroy the test object at the end of
  // the function which is why we have the dance below.
  scoped_ptr<AccessTokenFetcher> temporary_access_token_fetcher;
  AccessTokenFetcher* access_token_fetcher = test_access_token_fetcher_;
  if (!access_token_fetcher) {
    temporary_access_token_fetcher.reset(new AccessTokenFetcher());
    access_token_fetcher = temporary_access_token_fetcher.get();
  }

  if (!auth_code.empty()) {
    // If the user passed in an authcode, then use it to retrieve an
    // updated access/refresh token.
    access_token_fetcher->GetAccessTokenFromAuthCode(
        auth_code,
        access_token_callback);
  } else {
    DCHECK(!refresh_token_.empty());

    access_token_fetcher->GetAccessTokenFromRefreshToken(
        refresh_token_,
        access_token_callback);
  }

  run_loop.Run();

  // If we were using an auth_code and received a valid refresh token,
  // then we want to store it locally.  If we had an auth code and did not
  // receive a refresh token, then we should let the user know and exit.
  if (!auth_code.empty()) {
    if (!refresh_token_.empty()) {
      // If a unit test has set |test_refresh_token_store_| then we should use
      // it below.  Note that we do not want to destroy the test object.
      scoped_ptr<RefreshTokenStore> temporary_refresh_token_store;
      RefreshTokenStore* refresh_token_store = test_refresh_token_store_;
      if (!refresh_token_store) {
        temporary_refresh_token_store = RefreshTokenStore::OnDisk(user_name_);
        refresh_token_store = temporary_refresh_token_store.get();
      }

      if (!refresh_token_store->StoreRefreshToken(refresh_token_)) {
        // If we failed to persist the refresh token, then we should let the
        // user sort out the issue before continuing.
        return false;
      }
    } else {
      LOG(ERROR) << "Failed to use AUTH CODE to retrieve a refresh token.\n"
                 << "Was the one-time use AUTH CODE used more than once?";
      return false;
    }
  }

  if (access_token_.empty()) {
    LOG(ERROR) << "Failed to retrieve access token.";
    return false;
  }

  return true;
}

void AppRemotingTestDriverEnvironment::OnAccessTokenRetrieved(
    base::Closure done_closure,
    const std::string& access_token,
    const std::string& refresh_token) {
  access_token_ = access_token;
  refresh_token_ = refresh_token;

  done_closure.Run();
}

}  // namespace test
}  // namespace remoting
