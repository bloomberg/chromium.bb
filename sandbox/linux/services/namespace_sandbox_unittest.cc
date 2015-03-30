// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/namespace_sandbox.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/test/multiprocess_test.h"
#include "sandbox/linux/services/credentials.h"
#include "sandbox/linux/services/namespace_utils.h"
#include "sandbox/linux/services/proc_util.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace sandbox {

namespace {

bool RootDirectoryIsEmpty() {
  base::FilePath root("/");
  int file_type =
      base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES;
  base::FileEnumerator enumerator_before(root, false, file_type);
  return enumerator_before.Next().empty();
}

class NamespaceSandboxTest : public base::MultiProcessTest {
 public:
  void TestProc(const std::string& procname) {
    if (!Credentials::CanCreateProcessInNewUserNS()) {
      return;
    }

    base::FileHandleMappingVector fds_to_remap = {
        std::make_pair(STDOUT_FILENO, STDOUT_FILENO),
        std::make_pair(STDERR_FILENO, STDERR_FILENO),
    };
    base::LaunchOptions launch_options;
    launch_options.fds_to_remap = &fds_to_remap;

    base::Process process =
        NamespaceSandbox::LaunchProcess(MakeCmdLine(procname), launch_options);
    ASSERT_TRUE(process.IsValid());

    const int kDummyExitCode = 42;
    int exit_code = kDummyExitCode;
    EXPECT_TRUE(process.WaitForExit(&exit_code));
    EXPECT_EQ(0, exit_code);
  }
};

MULTIPROCESS_TEST_MAIN(SimpleChildProcess) {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  bool in_user_ns = NamespaceSandbox::InNewUserNamespace();
  bool in_pid_ns = NamespaceSandbox::InNewPidNamespace();
  bool in_net_ns = NamespaceSandbox::InNewNetNamespace();
  CHECK(in_user_ns);
  CHECK_EQ(in_pid_ns,
           NamespaceUtils::KernelSupportsUnprivilegedNamespace(CLONE_NEWPID));
  CHECK_EQ(in_net_ns,
           NamespaceUtils::KernelSupportsUnprivilegedNamespace(CLONE_NEWNET));
  if (in_pid_ns) {
    CHECK_EQ(1, getpid());
  }
  return 0;
}

TEST_F(NamespaceSandboxTest, BasicUsage) {
  TestProc("SimpleChildProcess");
}

MULTIPROCESS_TEST_MAIN(ChrootMe) {
  CHECK(!RootDirectoryIsEmpty());
  CHECK(sandbox::Credentials::MoveToNewUserNS());
  CHECK(sandbox::Credentials::DropFileSystemAccess(ProcUtil::OpenProc().get()));
  CHECK(RootDirectoryIsEmpty());
  return 0;
}

// Temporarily disabled on ASAN due to crbug.com/451603.
TEST_F(NamespaceSandboxTest, DISABLE_ON_ASAN(ChrootAndDropCapabilities)) {
  TestProc("ChrootMe");
}

MULTIPROCESS_TEST_MAIN(NestedNamespaceSandbox) {
  base::FileHandleMappingVector fds_to_remap = {
      std::make_pair(STDOUT_FILENO, STDOUT_FILENO),
      std::make_pair(STDERR_FILENO, STDERR_FILENO),
  };
  base::LaunchOptions launch_options;
  launch_options.fds_to_remap = &fds_to_remap;
  base::Process process = NamespaceSandbox::LaunchProcess(
      base::CommandLine(base::FilePath("/bin/true")), launch_options);
  CHECK(process.IsValid());

  const int kDummyExitCode = 42;
  int exit_code = kDummyExitCode;
  CHECK(process.WaitForExit(&exit_code));
  CHECK_EQ(0, exit_code);
  return 0;
}

TEST_F(NamespaceSandboxTest, NestedNamespaceSandbox) {
  TestProc("NestedNamespaceSandbox");
}

}  // namespace

}  // namespace sandbox
