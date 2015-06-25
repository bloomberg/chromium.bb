// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/test/test_suite.h"
#include "base/test/test_switches.h"
#include "remoting/test/refresh_token_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace switches {
const char kSingleProcessTestsSwitchName[] = "single-process-tests";
const char kUserNameSwitchName[] = "username";
const char kHostNameSwitchName[] = "hostname";
const char kRefreshTokenPathSwitchName[] = "refresh-token-path";
}

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  base::TestSuite test_suite(argc, argv);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  DCHECK(command_line);

  // Do not retry if tests fails.
  command_line->AppendSwitchASCII(switches::kTestLauncherRetryLimit, "0");

  // Different tests may require access to the same host if run in parallel.
  // To avoid shared resource contention, tests will be run one at a time.
  command_line->AppendSwitch(switches::kSingleProcessTestsSwitchName);

  // The username is used to run the tests and determines which refresh token to
  // select in the refresh token file.
  std::string username =
      command_line->GetSwitchValueASCII(switches::kUserNameSwitchName);

  if (username.empty()) {
    LOG(ERROR) << "No username passed in, can't authenticate or run tests!";
    return -1;
  }
  DVLOG(1) << "Running chromoting tests as: " << username;

  base::FilePath refresh_token_path =
     command_line->GetSwitchValuePath(switches::kRefreshTokenPathSwitchName);

  // The hostname determines which host to initiate a session with from the list
  // returned from the Directory service.
  std::string hostname =
      command_line->GetSwitchValueASCII(switches::kHostNameSwitchName);

  if (hostname.empty()) {
    LOG(ERROR) << "No hostname passed in, connect to host requires hostname!";
    return -1;
  }
  DVLOG(1) << "Chromoting tests will connect to: " << hostname;

  // TODO(TonyChun): Move this logic into a shared environment class.
  scoped_ptr<remoting::test::RefreshTokenStore> refresh_token_store =
      remoting::test::RefreshTokenStore::OnDisk(username, refresh_token_path);

  std::string refresh_token = refresh_token_store->FetchRefreshToken();

  if (refresh_token.empty()) {
    // RefreshTokenStore already logs which specific error occured.
    return -1;
  }

  return 0;
}