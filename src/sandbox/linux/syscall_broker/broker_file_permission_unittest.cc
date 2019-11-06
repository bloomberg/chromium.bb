// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/syscall_broker/broker_file_permission.h"

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/macros.h"
#include "sandbox/linux/tests/test_utils.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace syscall_broker {

class BrokerFilePermissionTester {
 public:
  static bool ValidatePath(const char* path) {
    return BrokerFilePermission::ValidatePath(path);
  }
  static const char* GetErrorMessage() {
    return BrokerFilePermission::GetErrorMessageForTests();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrokerFilePermissionTester);
};

namespace {

// Creation tests are DEATH tests as a bad permission causes termination.
SANDBOX_TEST(BrokerFilePermission, CreateGood) {
  const char kPath[] = "/tmp/good";
  BrokerFilePermission perm = BrokerFilePermission::ReadOnly(kPath);
}

SANDBOX_TEST(BrokerFilePermission, CreateGoodRecursive) {
  const char kPath[] = "/tmp/good/";
  BrokerFilePermission perm = BrokerFilePermission::ReadOnlyRecursive(kPath);
}

// In official builds, CHECK(x) causes a SIGTRAP on the architectures where the
// sanbox is enabled (that are x86, x86_64, arm64 and 32-bit arm processes
// running on a arm64 kernel).
#if defined(OFFICIAL_BUILD)
#define DEATH_BY_CHECK(msg) DEATH_BY_SIGNAL(SIGTRAP)
#else
#define DEATH_BY_CHECK(msg) DEATH_MESSAGE(msg)
#endif

SANDBOX_DEATH_TEST(
    BrokerFilePermission,
    CreateBad,
    DEATH_BY_CHECK(BrokerFilePermissionTester::GetErrorMessage())) {
  const char kPath[] = "/tmp/bad/";
  BrokerFilePermission perm = BrokerFilePermission::ReadOnly(kPath);
}

SANDBOX_DEATH_TEST(
    BrokerFilePermission,
    CreateBadRecursive,
    DEATH_BY_CHECK(BrokerFilePermissionTester::GetErrorMessage())) {
  const char kPath[] = "/tmp/bad";
  BrokerFilePermission perm = BrokerFilePermission::ReadOnlyRecursive(kPath);
}

SANDBOX_DEATH_TEST(
    BrokerFilePermission,
    CreateBadNotAbs,
    DEATH_BY_CHECK(BrokerFilePermissionTester::GetErrorMessage())) {
  const char kPath[] = "tmp/bad";
  BrokerFilePermission perm = BrokerFilePermission::ReadOnly(kPath);
}

SANDBOX_DEATH_TEST(
    BrokerFilePermission,
    CreateBadEmpty,
    DEATH_BY_CHECK(BrokerFilePermissionTester::GetErrorMessage())) {
  const char kPath[] = "";
  BrokerFilePermission perm = BrokerFilePermission::ReadOnly(kPath);
}

// CheckPerm tests |path| against |perm| given |access_flags|.
// If |create| is true then file creation is tested for success.
void CheckPerm(const BrokerFilePermission& perm,
               const char* path,
               int access_flags,
               bool create) {
  const char* file_to_open = NULL;

  ASSERT_FALSE(perm.CheckAccess(path, X_OK, NULL));
  ASSERT_TRUE(perm.CheckAccess(path, F_OK, NULL));
  // check bad perms
  switch (access_flags) {
    case O_RDONLY:
      ASSERT_TRUE(perm.CheckOpen(path, O_RDONLY, &file_to_open, NULL));
      ASSERT_FALSE(perm.CheckOpen(path, O_WRONLY, &file_to_open, NULL));
      ASSERT_FALSE(perm.CheckOpen(path, O_RDWR, &file_to_open, NULL));
      ASSERT_TRUE(perm.CheckAccess(path, R_OK, NULL));
      ASSERT_FALSE(perm.CheckAccess(path, W_OK, NULL));
      break;
    case O_WRONLY:
      ASSERT_FALSE(perm.CheckOpen(path, O_RDONLY, &file_to_open, NULL));
      ASSERT_TRUE(perm.CheckOpen(path, O_WRONLY, &file_to_open, NULL));
      ASSERT_FALSE(perm.CheckOpen(path, O_RDWR, &file_to_open, NULL));
      ASSERT_FALSE(perm.CheckAccess(path, R_OK, NULL));
      ASSERT_TRUE(perm.CheckAccess(path, W_OK, NULL));
      break;
    case O_RDWR:
      ASSERT_TRUE(perm.CheckOpen(path, O_RDONLY, &file_to_open, NULL));
      ASSERT_TRUE(perm.CheckOpen(path, O_WRONLY, &file_to_open, NULL));
      ASSERT_TRUE(perm.CheckOpen(path, O_RDWR, &file_to_open, NULL));
      ASSERT_TRUE(perm.CheckAccess(path, R_OK, NULL));
      ASSERT_TRUE(perm.CheckAccess(path, W_OK, NULL));
      break;
    default:
      // Bad test case
      NOTREACHED();
  }

// O_SYNC can be defined as (__O_SYNC|O_DSYNC)
#ifdef O_DSYNC
  const int kSyncFlag = O_SYNC & ~O_DSYNC;
#else
  const int kSyncFlag = O_SYNC;
#endif

  const int kNumberOfBitsInOAccMode = 2;
  static_assert(O_ACCMODE == ((1 << kNumberOfBitsInOAccMode) - 1),
                "incorrect number of bits");
  // check every possible flag and act accordingly.
  // Skipping AccMode bits as they are present in every case.
  for (int i = kNumberOfBitsInOAccMode; i < 32; i++) {
    int flag = 1 << i;
    switch (flag) {
      case O_APPEND:
      case O_ASYNC:
      case O_DIRECT:
      case O_DIRECTORY:
#ifdef O_DSYNC
      case O_DSYNC:
#endif
      case O_EXCL:
      case O_LARGEFILE:
      case O_NOATIME:
      case O_NOCTTY:
      case O_NOFOLLOW:
      case O_NONBLOCK:
#if (O_NONBLOCK != O_NDELAY)
      case O_NDELAY:
#endif
      case kSyncFlag:
        ASSERT_TRUE(
            perm.CheckOpen(path, access_flags | flag, &file_to_open, NULL));
        break;
      case O_TRUNC: {
        // The effect of (O_RDONLY | O_TRUNC) is undefined, and in some cases it
        // actually truncates, so deny.
        bool result =
            perm.CheckOpen(path, access_flags | flag, &file_to_open, NULL);
        if (access_flags == O_RDONLY) {
          ASSERT_FALSE(result);
        } else {
          ASSERT_TRUE(result);
        }
        break;
      }
      case O_CREAT:
        continue;  // Handled below.
      case O_CLOEXEC:
      default:
        ASSERT_FALSE(
            perm.CheckOpen(path, access_flags | flag, &file_to_open, NULL));
    }
  }
  if (create) {
    bool unlink;
    ASSERT_TRUE(perm.CheckOpen(path, O_CREAT | O_EXCL | access_flags,
                               &file_to_open, &unlink));
    ASSERT_FALSE(unlink);
  } else {
    ASSERT_FALSE(perm.CheckOpen(path, O_CREAT | O_EXCL | access_flags,
                                &file_to_open, NULL));
  }
}

TEST(BrokerFilePermission, ReadOnly) {
  const char kPath[] = "/tmp/good";
  BrokerFilePermission perm = BrokerFilePermission::ReadOnly(kPath);
  CheckPerm(perm, kPath, O_RDONLY, false);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerFilePermission, ReadOnlyRecursive) {
  const char kPath[] = "/tmp/good/";
  const char kPathFile[] = "/tmp/good/file";
  BrokerFilePermission perm = BrokerFilePermission::ReadOnlyRecursive(kPath);
  CheckPerm(perm, kPathFile, O_RDONLY, false);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

// Explicit test for O_RDONLY|O_TRUNC, which should be denied due to
// undefined behavior.
TEST(BrokerFilePermission, ReadOnlyTruncate) {
  const char kPath[] = "/tmp/good";
  BrokerFilePermission perm = BrokerFilePermission::ReadOnly(kPath);
  ASSERT_FALSE(perm.CheckOpen(kPath, O_RDONLY | O_TRUNC, nullptr, nullptr));
}

TEST(BrokerFilePermission, WriteOnly) {
  const char kPath[] = "/tmp/good";
  BrokerFilePermission perm = BrokerFilePermission::WriteOnly(kPath);
  CheckPerm(perm, kPath, O_WRONLY, false);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerFilePermission, ReadWrite) {
  const char kPath[] = "/tmp/good";
  BrokerFilePermission perm = BrokerFilePermission::ReadWrite(kPath);
  CheckPerm(perm, kPath, O_RDWR, false);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerFilePermission, ReadWriteCreate) {
  const char kPath[] = "/tmp/good";
  BrokerFilePermission perm = BrokerFilePermission::ReadWriteCreate(kPath);
  CheckPerm(perm, kPath, O_RDWR, true);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

void CheckUnlink(BrokerFilePermission& perm,
                 const char* path,
                 int access_flags) {
  bool unlink;
  ASSERT_FALSE(perm.CheckOpen(path, access_flags, NULL, &unlink));
  ASSERT_TRUE(
      perm.CheckOpen(path, access_flags | O_CREAT | O_EXCL, NULL, &unlink));
  ASSERT_TRUE(unlink);
}

TEST(BrokerFilePermission, ReadWriteCreateTemporaryRecursive) {
  const char kPath[] = "/tmp/good/";
  const char kPathFile[] = "/tmp/good/file";
  BrokerFilePermission perm =
      BrokerFilePermission::ReadWriteCreateTemporaryRecursive(kPath);
  CheckUnlink(perm, kPathFile, O_RDWR);
  // Don't do anything here, so that ASSERT works in the subfunction as
  // expected.
}

TEST(BrokerFilePermission, StatOnlyWithIntermediateDirs) {
  const char kPath[] = "/tmp/good/path";
  const char kLeading1[] = "/";
  const char kLeading2[] = "/tmp";
  const char kLeading3[] = "/tmp/good/path";
  const char kTrailing[] = "/tmp/good/path/bad";

  BrokerFilePermission perm =
      BrokerFilePermission::StatOnlyWithIntermediateDirs(kPath);
  // No open or access permission.
  ASSERT_FALSE(perm.CheckOpen(kPath, O_RDONLY, NULL, NULL));
  ASSERT_FALSE(perm.CheckOpen(kPath, O_WRONLY, NULL, NULL));
  ASSERT_FALSE(perm.CheckOpen(kPath, O_RDWR, NULL, NULL));
  ASSERT_FALSE(perm.CheckAccess(kPath, R_OK, NULL));
  ASSERT_FALSE(perm.CheckAccess(kPath, W_OK, NULL));

  // Stat for all leading paths, but not trailing paths.
  ASSERT_TRUE(perm.CheckStat(kPath, NULL));
  ASSERT_TRUE(perm.CheckStat(kLeading1, NULL));
  ASSERT_TRUE(perm.CheckStat(kLeading2, NULL));
  ASSERT_TRUE(perm.CheckStat(kLeading3, NULL));
  ASSERT_FALSE(perm.CheckStat(kTrailing, NULL));
}

TEST(BrokerFilePermission, ValidatePath) {
  EXPECT_TRUE(BrokerFilePermissionTester::ValidatePath("/path"));
  EXPECT_TRUE(BrokerFilePermissionTester::ValidatePath("/"));
  EXPECT_TRUE(BrokerFilePermissionTester::ValidatePath("/..path"));

  EXPECT_FALSE(BrokerFilePermissionTester::ValidatePath(""));
  EXPECT_FALSE(BrokerFilePermissionTester::ValidatePath("bad"));
  EXPECT_FALSE(BrokerFilePermissionTester::ValidatePath("/bad/"));
  EXPECT_FALSE(BrokerFilePermissionTester::ValidatePath("bad/"));
  EXPECT_FALSE(BrokerFilePermissionTester::ValidatePath("/bad/.."));
  EXPECT_FALSE(BrokerFilePermissionTester::ValidatePath("/bad/../bad"));
  EXPECT_FALSE(BrokerFilePermissionTester::ValidatePath("/../bad"));
}

}  // namespace

}  // namespace syscall_broker

}  // namespace sandbox
