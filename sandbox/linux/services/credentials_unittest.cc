// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/credentials.h"

#include <errno.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

bool DirectoryExists(const char* path) {
  struct stat dir;
  errno = 0;
  int ret = stat(path, &dir);
  return -1 != ret || ENOENT != errno;
}

bool WorkingDirectoryIsRoot() {
  char current_dir[PATH_MAX];
  char* cwd = getcwd(current_dir, sizeof(current_dir));
  PCHECK(cwd);
  if (strcmp("/", cwd)) return false;

  // The current directory is the root. Add a few paranoid checks.
  struct stat current;
  CHECK_EQ(0, stat(".", &current));
  struct stat parrent;
  CHECK_EQ(0, stat("..", &parrent));
  CHECK_EQ(current.st_dev, parrent.st_dev);
  CHECK_EQ(current.st_ino, parrent.st_ino);
  CHECK_EQ(current.st_mode, parrent.st_mode);
  CHECK_EQ(current.st_uid, parrent.st_uid);
  CHECK_EQ(current.st_gid, parrent.st_gid);
  return true;
}

// Give dynamic tools a simple thing to test.
TEST(Credentials, CreateAndDestroy) {
  {
    Credentials cred1;
    (void) cred1;
  }
  scoped_ptr<Credentials> cred2(new Credentials);
}

SANDBOX_TEST(Credentials, DropAllCaps) {
  Credentials creds;
  CHECK(creds.DropAllCapabilities());
  CHECK(!creds.HasAnyCapability());
}

SANDBOX_TEST(Credentials, GetCurrentCapString) {
  Credentials creds;
  CHECK(creds.DropAllCapabilities());
  const char kNoCapabilityText[] = "=";
  CHECK(*creds.GetCurrentCapString() == kNoCapabilityText);
}

SANDBOX_TEST(Credentials, MoveToNewUserNS) {
  Credentials creds;
  creds.DropAllCapabilities();
  bool userns_supported = creds.MoveToNewUserNS();
  printf("Unprivileged CLONE_NEWUSER supported: %s\n",
         userns_supported ? "true." : "false.");
  if (!userns_supported) {
    printf("This kernel does not support unprivileged namespaces. "
           "USERNS tests will all pass.\n");
    return;
  }
  CHECK(creds.HasAnyCapability());
  creds.DropAllCapabilities();
  CHECK(!creds.HasAnyCapability());
}

SANDBOX_TEST(Credentials, UidIsPreserved) {
  Credentials creds;
  creds.DropAllCapabilities();
  uid_t old_ruid, old_euid, old_suid;
  gid_t old_rgid, old_egid, old_sgid;
  PCHECK(0 == getresuid(&old_ruid, &old_euid, &old_suid));
  PCHECK(0 == getresgid(&old_rgid, &old_egid, &old_sgid));
  // Probably missing kernel support.
  if (!creds.MoveToNewUserNS()) return;
  uid_t new_ruid, new_euid, new_suid;
  PCHECK(0 == getresuid(&new_ruid, &new_euid, &new_suid));
  CHECK(old_ruid == new_ruid);
  CHECK(old_euid == new_euid);
  CHECK(old_suid == new_suid);

  gid_t new_rgid, new_egid, new_sgid;
  PCHECK(0 == getresgid(&new_rgid, &new_egid, &new_sgid));
  CHECK(old_rgid == new_rgid);
  CHECK(old_egid == new_egid);
  CHECK(old_sgid == new_sgid);
}

bool NewUserNSCycle(Credentials* creds) {
  DCHECK(creds);
  if (!creds->MoveToNewUserNS() ||
      !creds->HasAnyCapability() ||
      !creds->DropAllCapabilities() ||
      creds->HasAnyCapability()) {
    return false;
  }
  return true;
}

SANDBOX_TEST(Credentials, NestedUserNS) {
  Credentials creds;
  CHECK(creds.DropAllCapabilities());
  // Probably missing kernel support.
  if (!creds.MoveToNewUserNS()) return;
  creds.DropAllCapabilities();
  // As of 3.12, the kernel has a limit of 32. See create_user_ns().
  const int kNestLevel = 10;
  for (int i = 0; i < kNestLevel; ++i) {
    CHECK(NewUserNSCycle(&creds)) << "Creating new user NS failed at iteration "
                                  << i << ".";
  }
}

// Test the WorkingDirectoryIsRoot() helper.
TEST(Credentials, CanDetectRoot) {
  ASSERT_EQ(0, chdir("/proc/"));
  ASSERT_FALSE(WorkingDirectoryIsRoot());
  ASSERT_EQ(0, chdir("/"));
  ASSERT_TRUE(WorkingDirectoryIsRoot());
}

SANDBOX_TEST(Credentials, DropFileSystemAccessIsSafe) {
  Credentials creds;
  CHECK(creds.DropAllCapabilities());
  // Probably missing kernel support.
  if (!creds.MoveToNewUserNS()) return;
  CHECK(creds.DropFileSystemAccess());
  CHECK(!DirectoryExists("/proc"));
  CHECK(WorkingDirectoryIsRoot());
  // We want the chroot to never have a subdirectory. A subdirectory
  // could allow a chroot escape.
  CHECK_NE(0, mkdir("/test", 0700));
}

// Check that after dropping filesystem access and dropping privileges
// it is not possible to regain capabilities.
SANDBOX_TEST(Credentials, CannotRegainPrivileges) {
  Credentials creds;
  CHECK(creds.DropAllCapabilities());
  // Probably missing kernel support.
  if (!creds.MoveToNewUserNS()) return;
  CHECK(creds.DropFileSystemAccess());
  CHECK(creds.DropAllCapabilities());

  // The kernel should now prevent us from regaining capabilities because we
  // are in a chroot.
  CHECK(!creds.MoveToNewUserNS());
}

}  // namespace.

}  // namespace sandbox.
