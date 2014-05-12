// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy.h"

#include <errno.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/services/linux_syscalls.h"
#include "sandbox/linux/services/thread_helpers.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox {

namespace {

// |pid| is the return value of a fork()-like call. This
// makes sure that if fork() succeeded the child exits
// and the parent waits for it.
void HandlePostForkReturn(pid_t pid) {
  const int kChildExitCode = 1;
  if (pid > 0) {
    int status = 0;
    PCHECK(pid == HANDLE_EINTR(waitpid(pid, &status, 0)));
    CHECK(WIFEXITED(status));
    CHECK_EQ(kChildExitCode, WEXITSTATUS(status));
  } else if (pid == 0) {
    _exit(kChildExitCode);
  }
}

// Check that HandlePostForkReturn works.
TEST(BaselinePolicy, HandlePostForkReturn) {
  pid_t pid = fork();
  HandlePostForkReturn(pid);
}

BPF_TEST_C(BaselinePolicy, FchmodErrno, BaselinePolicy) {
  int ret = fchmod(-1, 07777);
  BPF_ASSERT_EQ(-1, ret);
  // Without the sandbox, this would EBADF instead.
  BPF_ASSERT_EQ(EPERM, errno);
}

// TODO(jln): make this work with the sanitizers.
#if !defined(ADDRESS_SANITIZER) && !defined(THREAD_SANITIZER) && \
    !defined(MEMORY_SANITIZER)

BPF_TEST_C(BaselinePolicy, ForkErrno, BaselinePolicy) {
  errno = 0;
  pid_t pid = fork();
  const int fork_errno = errno;
  HandlePostForkReturn(pid);

  BPF_ASSERT_EQ(-1, pid);
  BPF_ASSERT_EQ(EPERM, fork_errno);
}

pid_t ForkX86Glibc() {
  return syscall(__NR_clone, CLONE_PARENT_SETTID | SIGCHLD);
}

BPF_TEST_C(BaselinePolicy, ForkX86Eperm, BaselinePolicy) {
  errno = 0;
  pid_t pid = ForkX86Glibc();
  const int fork_errno = errno;
  HandlePostForkReturn(pid);

  BPF_ASSERT_EQ(-1, pid);
  BPF_ASSERT_EQ(EPERM, fork_errno);
}

pid_t ForkARMGlibc() {
  return syscall(__NR_clone,
                 CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID | SIGCHLD);
}

BPF_TEST_C(BaselinePolicy, ForkArmEperm, BaselinePolicy) {
  errno = 0;
  pid_t pid = ForkARMGlibc();
  const int fork_errno = errno;
  HandlePostForkReturn(pid);

  BPF_ASSERT_EQ(-1, pid);
  BPF_ASSERT_EQ(EPERM, fork_errno);
}

BPF_TEST_C(BaselinePolicy, CreateThread, BaselinePolicy) {
  base::Thread thread("sandbox_tests");
  BPF_ASSERT(thread.Start());
}

BPF_DEATH_TEST_C(BaselinePolicy,
                 DisallowedCloneFlagCrashes,
                 DEATH_MESSAGE(GetCloneErrorMessageContentForTests()),
                 BaselinePolicy) {
  pid_t pid = syscall(__NR_clone, CLONE_THREAD | SIGCHLD);
  HandlePostForkReturn(pid);
}

#endif  // !defined(ADDRESS_SANITIZER) && !defined(THREAD_SANITIZER) &&
        // !defined(MEMORY_SANITIZER)

}  // namespace

}  // namespace sandbox
