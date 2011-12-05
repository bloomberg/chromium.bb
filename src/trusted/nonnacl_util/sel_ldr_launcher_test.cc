/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <vector>

#include "gtest/gtest.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

using ::std::vector;

#define NACL_STRING_ARRAY_TO_VECTOR(a) \
  vector<nacl::string>(a, a + NACL_ARRAY_SIZE(a))

namespace {

// Tests for SelLdrLauncher

// Helper function that tests construction of the command line
// via InitCommandLine() and BuildCommandLine() calls.  The executable path
// (0th arg) is not tested and should not be included in |expected_argv|.
void ExpectSelLdrLauncherArgv(std::vector<nacl::string>& expected_argv) {
  nacl::SelLdrLauncher launcher;

  // Prepare the fixed sample args.
  int imc_fd = NACL_NO_FILE_DESC;
  const char* sel_ldr_argv[] = { "-X", "5" };
  const char* application_argv[] = { "foo" };
  const char* extra_expected_argv[] = { "-X", "5", "--", "foo" };
  for (size_t i = 0; i < NACL_ARRAY_SIZE(extra_expected_argv); i++) {
    expected_argv.push_back(extra_expected_argv[i]);
  }

  // Initialize the arg fields and check the ones that have public getters.
  launcher.InitCommandLine(imc_fd,
                           NACL_STRING_ARRAY_TO_VECTOR(sel_ldr_argv),
                           NACL_STRING_ARRAY_TO_VECTOR(application_argv));

  // Build the command line and verify it, skipping the executable.
  vector<nacl::string> command;
  launcher.BuildCommandLine(&command);

  // command vector includes the sel_ldr executable.
  size_t ignored_args = 1;
#if NACL_LINUX
  // Skip the bootstrap executable, the sel_ldr executable, and --r_debug.
  ignored_args += 2;
#endif

  ASSERT_EQ(expected_argv.size() + ignored_args, command.size());
  for (size_t i = 0; i < expected_argv.size(); i++) {
    EXPECT_EQ(expected_argv[i], command[i + ignored_args]);
  }
}


TEST(SelLdrLauncherTest, BuildCommandLine) {
  vector<nacl::string> expected_R;
  expected_R.push_back("-R");
  ExpectSelLdrLauncherArgv(expected_R);
}

}  // namespace

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
