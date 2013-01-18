// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

#include "base/file_util.h"
#include "base/third_party/valgrind/valgrind.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace {
  std::string TestFailedMessage(const std::string& msg) {
    return msg.empty() ? "" : "Actual test failure: " + msg;
  }
}

namespace sandbox {

static const int kExpectedValue = 42;
static const int kIgnoreThisTest = 43;
static const int kExitWithAssertionFailure = 1;
static const int kExitForTimeout = 2;

static void SigAlrmHandler(int) {
    const char failure_message[] = "Timeout reached!\n";
    // Make sure that we never block here.
    if (!fcntl(2, F_SETFL,  O_NONBLOCK)) {
      if (write(2, failure_message, sizeof(failure_message) - 1) < 0) {
      }
    }
    _exit(kExitForTimeout);
}

// Set a timeout with a handler that will automatically fail the
// test.
static void SetProcessTimeout(int time_in_seconds) {
  struct sigaction act = {};
  act.sa_handler = SigAlrmHandler;
  SANDBOX_ASSERT(sigemptyset(&act.sa_mask) == 0);
  act.sa_flags = 0;

  struct sigaction old_act;
  SANDBOX_ASSERT(sigaction(SIGALRM, &act, &old_act) == 0);

  // We don't implemenet signal chaining, so make sure that nothing else
  // is expecting to handle SIGALRM.
  SANDBOX_ASSERT((old_act.sa_flags & SA_SIGINFO) == 0);
  SANDBOX_ASSERT(old_act.sa_handler == SIG_DFL);
  sigset_t sigalrm_set;
  SANDBOX_ASSERT(sigemptyset(&sigalrm_set) == 0);
  SANDBOX_ASSERT(sigaddset(&sigalrm_set, SIGALRM) == 0);
  SANDBOX_ASSERT(sigprocmask(SIG_UNBLOCK, &sigalrm_set, NULL) == 0);
  SANDBOX_ASSERT(alarm(time_in_seconds) == 0);  // There should be no previous
                                                // alarm.
}

void UnitTests::RunTestInProcess(UnitTests::Test test, void *arg,
                                 DeathCheck death, const void *death_aux) {
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

    // Don't set a timeout if running on Valgrind, since it's generally much
    // slower.
    if (!RUNNING_ON_VALGRIND) {
      SetProcessTimeout(10 /* seconds */);
    }

    // Disable core files. They are not very useful for our individual test
    // cases.
    struct rlimit no_core = { 0 };
    setrlimit(RLIMIT_CORE, &no_core);

    test(arg);
    _exit(kExpectedValue);
  }

  (void)HANDLE_EINTR(close(fds[1]));
  std::vector<char> msg_buf;
  ssize_t rc;
  do {
    const unsigned int kCapacity = 256;
    size_t len = msg_buf.size();
    msg_buf.resize(len + kCapacity);
    rc = HANDLE_EINTR(read(fds[0], &msg_buf[len], kCapacity));
    msg_buf.resize(len + std::max(rc, static_cast<ssize_t>(0)));
  } while (rc > 0);
  (void)HANDLE_EINTR(close(fds[0]));
  std::string msg(msg_buf.begin(), msg_buf.end());

  int status = 0;
  int waitpid_returned = HANDLE_EINTR(waitpid(pid, &status, 0));
  ASSERT_EQ(pid, waitpid_returned) << TestFailedMessage(msg);

  // At run-time, we sometimes decide that a test shouldn't actually
  // run (e.g. when testing sandbox features on a kernel that doesn't
  // have sandboxing support). When that happens, don't attempt to
  // call the "death" function, as it might be looking for a
  // death-test condition that would never have triggered.
  if (!WIFEXITED(status) || WEXITSTATUS(status) != kIgnoreThisTest ||
      !msg.empty()) {
    // We use gtest's ASSERT_XXX() macros instead of the DeathCheck
    // functions.  This means, on failure, "return" is called. This
    // only works correctly, if the call of the "death" callback is
    // the very last thing in our function.
    death(status, msg, death_aux);
  }
}

void UnitTests::DeathSuccess(int status, const std::string& msg,
                             const void *) {
  std::string details(TestFailedMessage(msg));

  bool subprocess_terminated_normally = WIFEXITED(status);
  ASSERT_TRUE(subprocess_terminated_normally) << details;
  int subprocess_exit_status = WEXITSTATUS(status);
  ASSERT_EQ(kExpectedValue, subprocess_exit_status) << details;
  bool subprocess_exited_but_printed_messages = !msg.empty();
  EXPECT_FALSE(subprocess_exited_but_printed_messages) << details;
}

void UnitTests::DeathMessage(int status, const std::string& msg,
                             const void *aux) {
  std::string details(TestFailedMessage(msg));
  const char *expected_msg = static_cast<const char *>(aux);

  bool subprocess_terminated_normally = WIFEXITED(status);
  ASSERT_TRUE(subprocess_terminated_normally) << details;
  int subprocess_exit_status = WEXITSTATUS(status);
  ASSERT_EQ(kExitWithAssertionFailure, subprocess_exit_status) << details;
  bool subprocess_exited_without_matching_message =
    msg.find(expected_msg) == std::string::npos;
  EXPECT_FALSE(subprocess_exited_without_matching_message) << details;
}

void UnitTests::DeathExitCode(int status, const std::string& msg,
                              const void *aux) {
  int expected_exit_code = static_cast<int>(reinterpret_cast<intptr_t>(aux));
  std::string details(TestFailedMessage(msg));

  bool subprocess_terminated_normally = WIFEXITED(status);
  ASSERT_TRUE(subprocess_terminated_normally) << details;
  int subprocess_exit_status = WEXITSTATUS(status);
  ASSERT_EQ(subprocess_exit_status, expected_exit_code) << details;
}

void UnitTests::DeathBySignal(int status, const std::string& msg,
                              const void *aux) {
  int expected_signo = static_cast<int>(reinterpret_cast<intptr_t>(aux));
  std::string details(TestFailedMessage(msg));

  bool subprocess_terminated_by_signal = WIFSIGNALED(status);
  ASSERT_TRUE(subprocess_terminated_by_signal) << details;
  int subprocess_signal_number = WTERMSIG(status);
  ASSERT_EQ(subprocess_signal_number, expected_signo) << details;
}

void UnitTests::AssertionFailure(const char *expr, const char *file,
                                 int line) {
  fprintf(stderr, "%s:%d:%s", file, line, expr);
  fflush(stderr);
  _exit(kExitWithAssertionFailure);
}

void UnitTests::IgnoreThisTest() {
  fflush(stderr);
  _exit(kIgnoreThisTest);
}

}  // namespace
