// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <sys/resource.h>
#include <sys/time.h>

#include "base/file_util.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox {

static const int kExpectedValue = 42;

void UnitTests::RunTestInProcess(UnitTests::Test test, void *arg) {
  // Runs a test in a sub-process. This is necessary for most of the code
  // in the BPF sandbox, as it potentially makes global state changes and as
  // it also tends to raise fatal errors, if the code has been used in an
  // insecure manner.
  int fds[2];
  ASSERT_EQ(0, pipe(fds));

  pid_t pid;
  ASSERT_LE(0, (pid = fork()));
  if (!pid) {
    // In child process
    // Redirect stderr to our pipe. This way, we can capture all error
    // messages, if we decide we want to do so in our tests.
    SANDBOX_ASSERT(dup2(fds[1], 2) == 2);
    SANDBOX_ASSERT(!close(fds[0]));
    SANDBOX_ASSERT(!close(fds[1]));

    // Disable core files. They are not very useful for our individual test
    // cases.
    struct rlimit no_core = { 0 };
    setrlimit(RLIMIT_CORE, &no_core);

    test(arg);
    _exit(kExpectedValue);
  }

  (void)HANDLE_EINTR(close(fds[1]));
  std::vector<char> msg;
  ssize_t rc;
  do {
    const unsigned int kCapacity = 256;
    size_t len = msg.size();
    msg.resize(len + kCapacity);
    rc = HANDLE_EINTR(read(fds[0], &msg[len], kCapacity));
    msg.resize(len + std::max(rc, static_cast<ssize_t>(0)));
  } while (rc > 0);
  std::string details;
  if (!msg.empty()) {
    details = "Actual test failure: " + std::string(msg.begin(), msg.end());
  }
  (void)HANDLE_EINTR(close(fds[0]));

  int status = 0;
  int waitpid_returned = HANDLE_EINTR(waitpid(pid, &status, 0));
  ASSERT_EQ(pid, waitpid_returned) << details;
  bool subprocess_terminated_normally = WIFEXITED(status);
  ASSERT_TRUE(subprocess_terminated_normally) << details;
  int subprocess_exit_status = WEXITSTATUS(status);
  ASSERT_EQ(kExpectedValue, subprocess_exit_status) << details;
  bool subprocess_exited_but_printed_messages = !msg.empty();
  EXPECT_FALSE(subprocess_exited_but_printed_messages) << details;
}

void UnitTests::AssertionFailure(const char *expr, const char *file,
                                 int line) {
  fprintf(stderr, "%s:%d:%s", file, line, expr);
  fflush(stderr);
  _exit(1);
}

}  // namespace
