// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/test/test_suite.h"
#include "base/test/test_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace switches {
const char kSingleProcessTestsSwitchName[] = "single-process-tests";
const char kUserNameSwitchName[] = "username";
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

  std::string username =
    command_line->GetSwitchValueASCII(switches::kUserNameSwitchName);

  // Verify that the username is passed in as an argument.
  if (username.empty()) {
    LOG(ERROR) << "No username passed in, can't authenticate without that!";
    return -1;
  }
  DVLOG(1) << "Running chromoting tests as: " << username;

  return 0;
}

