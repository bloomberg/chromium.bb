// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/syscall_wrappers.h"

#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/third_party/valgrind/valgrind.h"
#include "build/build_config.h"
#include "sandbox/linux/tests/test_utils.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

TEST(SyscallWrappers, BasicSyscalls) {
  EXPECT_EQ(getpid(), sys_getpid());
}

TEST(SyscallWrappers, CloneBasic) {
  pid_t child = sys_clone(SIGCHLD);
  TestUtils::HandlePostForkReturn(child);
  EXPECT_LT(0, child);
}

TEST(SyscallWrappers, CloneParentSettid) {
  pid_t ptid = 0;
  pid_t child = sys_clone(CLONE_PARENT_SETTID | SIGCHLD, nullptr, &ptid,
                          nullptr, nullptr);
  TestUtils::HandlePostForkReturn(child);
  EXPECT_LT(0, child);
  EXPECT_EQ(child, ptid);
}

TEST(SyscallWrappers, CloneChildSettid) {
  pid_t ctid = 0;
  pid_t pid =
      sys_clone(CLONE_CHILD_SETTID | SIGCHLD, nullptr, nullptr, &ctid, nullptr);

  const int kSuccessExit = 0;
  if (0 == pid) {
    // In child.
    if (sys_getpid() == ctid)
      _exit(kSuccessExit);
    _exit(1);
  }

  ASSERT_NE(-1, pid);
  int status = 0;
  ASSERT_EQ(pid, HANDLE_EINTR(waitpid(pid, &status, 0)));
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(kSuccessExit, WEXITSTATUS(status));
}

}  // namespace

}  // namespace sandbox
