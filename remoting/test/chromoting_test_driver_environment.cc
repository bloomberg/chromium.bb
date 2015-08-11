// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/chromoting_test_driver_environment.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "remoting/test/access_token_fetcher.h"
#include "remoting/test/host_list_fetcher.h"
#include "remoting/test/refresh_token_store.h"

namespace remoting {
namespace test {

ChromotingTestDriverEnvironment* g_chromoting_shared_data = nullptr;

ChromotingTestDriverEnvironment::EnvironmentOptions::EnvironmentOptions() {
}

ChromotingTestDriverEnvironment::EnvironmentOptions::~EnvironmentOptions() {
}

ChromotingTestDriverEnvironment::ChromotingTestDriverEnvironment(
    const EnvironmentOptions& options)
    : host_name_(options.host_name),
      user_name_(options.user_name),
      pin_(options.pin),
      refresh_token_file_path_(options.refresh_token_file_path),
      test_access_token_fetcher_(nullptr),
      test_refresh_token_store_(nullptr),
      test_host_list_fetcher_(nullptr) {
  DCHECK(!user_name_.empty());
  DCHECK(!host_name_.empty());
}

ChromotingTestDriverEnvironment::~ChromotingTestDriverEnvironment() {
}

bool ChromotingTestDriverEnvironment::Initialize(
    const std::string& auth_code) {
  if (!access_token_.empty()) {
    return true;
  }

  if (!base::MessageLoop::current()) {
    message_loop_.reset(new base::MessageLoopForIO);
  }

  // If a unit test has set |test_refresh_token_store_| then we should use it
  // below.  Note that we do not want to destroy the test object.
  scoped_ptr<RefreshTokenStore> temporary_refresh_token_store;
  RefreshTokenStore* refresh_token_store = test_refresh_token_store_;
  if (!refresh_token_store) {
    temporary_refresh_token_store =
        RefreshTokenStore::OnDisk(user_name_, refresh_token_file_path_);
    refresh_token_store = temporary_refresh_token_store.get();
  }

  // Check to see if we have a refresh token stored for this user.
  refresh_token_ = refresh_token_store->FetchRefreshToken();
  if (refresh_token_.empty()) {
    // This isn't necessarily an error as this might be a first run scenario.
    VLOG(2) << "No refresh token stored for " << user_name_;

    if (auth_code.empty()) {
      // No token and no Auth code means no service connectivity, bail!
      LOG(ERROR) << "Cannot retrieve an access token without a stored refresh"
                 << " token on disk or an auth_code passed into the tool";
      return false;
    }
  }

  if (!RetrieveAccessToken(auth_code) || !RetrieveHostList()) {
    // If we cannot retrieve an access token or a host list, then nothing is
    // going to work. We should let the caller know that our object is not ready
    // to be used.
    return false;
  }

  return true;
}

void ChromotingTestDriverEnvironment::DisplayHostList() {
  const char kHostAvailabilityFormatString[] = "%-45s%-15s%-35s";

  LOG(INFO) << base::StringPrintf(kHostAvailabilityFormatString,
                                  "Host Name", "Host Status", "Host JID");
  LOG(INFO) << base::StringPrintf(kHostAvailabilityFormatString,
                                  "---------", "-----------", "--------");

  std::string status;
  for (const HostInfo& host_info : host_list_) {
    HostStatus host_status = host_info.status;
    if (host_status == kHostStatusOnline) {
      status = "ONLINE";
    } else if (host_status == kHostStatusOffline) {
      status = "OFFLINE";
    } else {
      status = "UNKNOWN";
    }

    LOG(INFO) << base::StringPrintf(
        kHostAvailabilityFormatString, host_info.host_name.c_str(),
        status.c_str(), host_info.host_jid.c_str());
  }
}

void ChromotingTestDriverEnvironment::SetAccessTokenFetcherForTest(
    AccessTokenFetcher* access_token_fetcher) {
  DCHECK(access_token_fetcher);

  test_access_token_fetcher_ = access_token_fetcher;
}

void ChromotingTestDriverEnvironment::SetRefreshTokenStoreForTest(
    RefreshTokenStore* refresh_token_store) {
  DCHECK(refresh_token_store);

  test_refresh_token_store_ = refresh_token_store;
}

void ChromotingTestDriverEnvironment::SetHostListFetcherForTest(
    HostListFetcher* host_list_fetcher) {
  DCHECK(host_list_fetcher);

  test_host_list_fetcher_ = host_list_fetcher;
}

void ChromotingTestDriverEnvironment::TearDown() {
  // Letting the MessageLoop tear down during the test destructor results in
  // errors after test completion, when the MessageLoop dtor touches the
  // registered AtExitManager. The AtExitManager is torn down before the test
  // destructor is executed, so we tear down the MessageLoop here, while it is
  // still valid.
  message_loop_.reset();
}

bool ChromotingTestDriverEnvironment::RetrieveAccessToken(
    const std::string& auth_code) {
  base::RunLoop run_loop;

  access_token_.clear();

  AccessTokenCallback access_token_callback =
      base::Bind(&ChromotingTestDriverEnvironment::OnAccessTokenRetrieved,
                 base::Unretained(this), run_loop.QuitClosure());

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
    access_token_fetcher->GetAccessTokenFromAuthCode(auth_code,
                                                     access_token_callback);
  } else {
    DCHECK(!refresh_token_.empty());

    access_token_fetcher->GetAccessTokenFromRefreshToken(refresh_token_,
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
        temporary_refresh_token_store =
            RefreshTokenStore::OnDisk(user_name_, refresh_token_file_path_);
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

void ChromotingTestDriverEnvironment::OnAccessTokenRetrieved(
    base::Closure done_closure,
    const std::string& retrieved_access_token,
    const std::string& retrieved_refresh_token) {
  VLOG(1) << "OnAccessTokenRetrieved() Called";
  VLOG(1) << "Access Token: " << retrieved_access_token;

  access_token_ = retrieved_access_token;
  refresh_token_ = retrieved_refresh_token;

  done_closure.Run();
}

bool ChromotingTestDriverEnvironment::RetrieveHostList() {
  base::RunLoop run_loop;

  host_list_.clear();
  host_info_ = HostInfo();

  // If a unit test has set |test_host_list_fetcher_| then we should use it
  // below.  Note that we do not want to destroy the test object at the end of
  // the function which is why we have the dance below.
  scoped_ptr<HostListFetcher> temporary_host_list_fetcher;
  HostListFetcher* host_list_fetcher = test_host_list_fetcher_;
  if (!host_list_fetcher) {
    temporary_host_list_fetcher.reset(new HostListFetcher());
    host_list_fetcher = temporary_host_list_fetcher.get();
  }

  remoting::test::HostListFetcher::HostlistCallback host_list_callback =
      base::Bind(&ChromotingTestDriverEnvironment::OnHostListRetrieved,
                 base::Unretained(this), run_loop.QuitClosure());

  host_list_fetcher->RetrieveHostlist(access_token_, host_list_callback);

  run_loop.Run();

  if (host_list_.empty()) {
    // Note: Access token may have expired, but it is unlikely.
    LOG(ERROR) << "Retrieved host list is empty.\n"
               << "Does the account have hosts set up?";
    return false;
  }

  // If the host or command line parameters are not setup correctly, we want to
  // let the user fix the issue before continuing.
  bool found_host_name = false;
  auto host_info_iter = std::find_if(host_list_.begin(), host_list_.end(),
      [this, &found_host_name](const remoting::test::HostInfo& host_info) {
          if (host_info.host_name == host_name_) {
            found_host_name = true;
            return host_info.IsReadyForConnection();
          }
          return false;
      });
  if (host_info_iter == host_list_.end()) {
    if (found_host_name) {
      LOG(ERROR) << this->host_name_ << " is not ready to connect.";
    } else {
      LOG(ERROR) << this->host_name_ << " was not found in the host list.";
    }
    DisplayHostList();
    return false;
  }

  host_info_ = *host_info_iter;

  return true;
}

void ChromotingTestDriverEnvironment::OnHostListRetrieved(
    base::Closure done_closure,
    const std::vector<HostInfo>& retrieved_host_list) {
  VLOG(1) << "OnHostListRetrieved() Called";

  host_list_ = retrieved_host_list;

  done_closure.Run();
}

}  // namespace test
}  // namespace remoting
