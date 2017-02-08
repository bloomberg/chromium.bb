// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <IOSurface/IOSurface.h>

#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <unistd.h>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/mac/mac_util.h"
#include "base/process/kill.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "sandbox/mac/sandbox_compiler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace sandbox {

// These tests are designed to begin testing the V2 style sandbox rules on the
// bots, rendering the earliest possible test results on how the rules perform
// consistently across all test bots and supported OS versions.
class SandboxMacCompilerV2Test : public base::MultiProcessTest {};

MULTIPROCESS_TEST_MAIN(V2ProfileProcess) {
  // Note: newlines are not necessary in the profile, but do make it easier to
  // print the profile out for debugging purposes.
  std::string profile =
      "(version 1)\n"
      "(deny default)\n"
      "(define allowed-dir \"ALLOWED_READ_DIR\")\n"
      "(define temp-file \"ALLOWED_TEMP_FILE\")\n"
      "(define is-pre-10_10 \"IS_PRE_10_10\")\n"
      "; Make it easier to drop (literal) once we stop supporting 10.9\n"
      "(define (path x) (literal x))\n"
      "(allow file-read-metadata (subpath \"/Applications\"))\n"
      "(allow file-read* (subpath (param allowed-dir)))\n"
      "(allow file-read-data (path \"/usr/share/zoneinfo/zone.tab\"))\n"
      "(allow file-write* (path (param temp-file)))\n"
      "(allow ipc-posix-shm-read-data (ipc-posix-name "
      "\"apple.shm.notification_center\"))\n"
      "(allow mach-lookup (global-name \"com.apple.logd\"))\n"
      "(if (string=? (param is-pre-10_10) \"TRUE\") (allow sysctl-read))\n"
      "(if (string=? (param is-pre-10_10) \"FALSE\") (allow sysctl-read "
      "(sysctl-name \"hw.activecpu\")))\n";

  std::string temp_file_path = "/private/tmp/sf234234wfsfsdfdsf";
  SandboxCompiler compiler(profile);
  CHECK(compiler.InsertStringParam("ALLOWED_READ_DIR", "/usr/lib"));
  CHECK(compiler.InsertStringParam("ALLOWED_TEMP_FILE", temp_file_path));
  CHECK(compiler.InsertBooleanParam("IS_PRE_10_10",
                                    !base::mac::IsAtLeastOS10_10()));

  std::string error;
  bool result = compiler.CompileAndApplyProfile(&error);
  CHECK(result) << error;

  // Now attempt the appropriate resource access.
  base::FilePath path("/usr/lib/libsandbox.dylib");
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  CHECK(file.IsValid());

  char buf[4096];
  EXPECT_EQ(static_cast<int>(sizeof(buf)),
            file.Read(/*offset=*/0, buf, sizeof(buf)));
  file.Close();  // Protect again other checks accidentally using this file.

  struct stat sb;
  EXPECT_EQ(0, stat("/Applications/TextEdit.app", &sb));

  base::FilePath zone_path("/usr/share/zoneinfo/zone.tab");
  base::File zone_file(zone_path,
                       base::File::FLAG_OPEN | base::File::FLAG_READ);
  CHECK(zone_file.IsValid());

  char zone_buf[2];
  EXPECT_EQ(static_cast<int>(sizeof(zone_buf)),
            zone_file.Read(/*offset=*/0, zone_buf, sizeof(zone_buf)));
  zone_file.Close();

  // Make sure we cannot read any files in zoneinfo.
  base::FilePath zone_dir_path("/usr/share/zoneinfo");
  base::File zoneinfo(zone_dir_path,
                      base::File::FLAG_OPEN | base::File::FLAG_READ);
  EXPECT_FALSE(zoneinfo.IsValid());

  base::FilePath temp_path(temp_file_path);
  base::File temp_file(temp_path,
                       base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);
  CHECK(temp_file.IsValid());

  const char msg[] = "I can write this file.";
  EXPECT_EQ(static_cast<int>(sizeof(msg)),
            temp_file.WriteAtCurrentPos(msg, sizeof(msg)));
  temp_file.Close();

  int shm_fd = shm_open("apple.shm.notification_center", O_RDONLY, 0644);
  EXPECT_GE(shm_fd, 0);

  NSPort* mach = [[NSMachBootstrapServer sharedInstance]
      servicePortWithName:@"com.apple.logd"];
  EXPECT_NE(nil, mach);

  NSPort* forbidden_mach = [[NSMachBootstrapServer sharedInstance]
      servicePortWithName:@"com.apple.fonts."];
  EXPECT_EQ(nil, forbidden_mach);

  size_t oldp_len;
  EXPECT_EQ(0, sysctlbyname("hw.activecpu", NULL, &oldp_len, NULL, 0));

  char oldp[oldp_len];
  EXPECT_EQ(0, sysctlbyname("hw.activecpu", oldp, &oldp_len, NULL, 0));

  size_t ncpu_len;
  EXPECT_NE(0, sysctlbyname("hw.ncpu", NULL, &ncpu_len, NULL, 0));

  return 0;
}

TEST_F(SandboxMacCompilerV2Test, V2ProfileTest) {
  base::Process process = SpawnChild("V2ProfileProcess");
  ASSERT_TRUE(process.IsValid());
  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(exit_code, 0);
}

}  // namespace sandbox
