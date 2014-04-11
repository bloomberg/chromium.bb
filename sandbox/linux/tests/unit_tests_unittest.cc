// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox {

namespace {

// Let's not use any of the "magic" values used internally in unit_tests.cc,
// such as kExpectedValue.
const int kExpectedExitCode = 100;

SANDBOX_DEATH_TEST(UnitTests,
                   DeathExitCode,
                   DEATH_EXIT_CODE(kExpectedExitCode)) {
  exit(kExpectedExitCode);
}

const int kExpectedSignalNumber = SIGKILL;

SANDBOX_DEATH_TEST(UnitTests,
                   DeathBySignal,
                   DEATH_BY_SIGNAL(kExpectedSignalNumber)) {
  raise(kExpectedSignalNumber);
}

SANDBOX_TEST_ALLOW_NOISE(UnitTests, NoisyTest) {
  LOG(ERROR) << "The cow says moo!";
}

// Test that a subprocess can be forked() and can use exit(3) instead of
// _exit(2).
TEST(UnitTests, SubProcessCanExit) {
  pid_t child = fork();
  ASSERT_NE(-1, child);

  if (!child) {
    exit(kExpectedExitCode);
  }

  int status = 0;
  pid_t waitpid_ret = HANDLE_EINTR(waitpid(child, &status, 0));
  EXPECT_EQ(child, waitpid_ret);
  EXPECT_TRUE(WIFEXITED(status));
  EXPECT_EQ(kExpectedExitCode, WEXITSTATUS(status));
}

}  // namespace

}  // namespace sandbox
