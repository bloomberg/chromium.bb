// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/chromeos/fileapi/file_access_permissions.h"
#include "testing/gtest/include/gtest/gtest.h"


class FileAccessPermissionsTest : public testing::Test {
};

TEST_F(FileAccessPermissionsTest, FileAccessChecks) {
#if defined(OS_WIN)
  base::FilePath good_dir(FILE_PATH_LITERAL("c:\\root\\dir"));
  base::FilePath bad_dir(FILE_PATH_LITERAL("c:\\root"));
  base::FilePath good_file(FILE_PATH_LITERAL("c:\\root\\dir\\good_file.txt"));
  base::FilePath bad_file(FILE_PATH_LITERAL("c:\\root\\dir\\bad_file.txt"));
#elif defined(OS_POSIX)
  base::FilePath good_dir(FILE_PATH_LITERAL("/root/dir"));
  base::FilePath bad_dir(FILE_PATH_LITERAL("/root"));
  base::FilePath good_file(FILE_PATH_LITERAL("/root/dir/good_file.txt"));
  base::FilePath bad_file(FILE_PATH_LITERAL("/root/dir/bad_file.txt"));
#endif
  std::string extension1("ddammdhioacbehjngdmkjcjbnfginlla");
  std::string extension2("jkhdjkhkhsdkfhsdkhrterwmtermeter");

  chromeos::FileAccessPermissions permissions;
  // By default extension have no access to any local file.
  EXPECT_FALSE(permissions.HasAccessPermission(extension1, good_dir));
  EXPECT_FALSE(permissions.HasAccessPermission(extension1, good_file));
  EXPECT_FALSE(permissions.HasAccessPermission(extension1, bad_file));
  EXPECT_FALSE(permissions.HasAccessPermission(extension2, good_dir));
  EXPECT_FALSE(permissions.HasAccessPermission(extension2, good_file));
  EXPECT_FALSE(permissions.HasAccessPermission(extension2, bad_file));

  // After granting file access to the handler extension for a given file, it
  // can only access that file an nothing else.
  permissions.GrantAccessPermission(extension1, good_file);
  EXPECT_FALSE(permissions.HasAccessPermission(extension1, good_dir));
  EXPECT_TRUE(permissions.HasAccessPermission(extension1, good_file));
  EXPECT_FALSE(permissions.HasAccessPermission(extension1, bad_file));
  EXPECT_FALSE(permissions.HasAccessPermission(extension2, good_dir));
  EXPECT_FALSE(permissions.HasAccessPermission(extension2, good_file));
  EXPECT_FALSE(permissions.HasAccessPermission(extension2, bad_file));


  // After granting file access to the handler extension for a given directory,
  // it can access that directory and all files within it.
  permissions.GrantAccessPermission(extension2, good_dir);
  EXPECT_FALSE(permissions.HasAccessPermission(extension1, good_dir));
  EXPECT_TRUE(permissions.HasAccessPermission(extension1, good_file));
  EXPECT_FALSE(permissions.HasAccessPermission(extension1, bad_file));
  EXPECT_TRUE(permissions.HasAccessPermission(extension2, good_dir));
  EXPECT_TRUE(permissions.HasAccessPermission(extension2, good_file));
  EXPECT_TRUE(permissions.HasAccessPermission(extension2, bad_file));

  // After revoking rights for extensions, they should not be able to access
  // any file system element anymore.
  permissions.RevokePermissions(extension1);
  permissions.RevokePermissions(extension2);
  EXPECT_FALSE(permissions.HasAccessPermission(extension1, good_dir));
  EXPECT_FALSE(permissions.HasAccessPermission(extension1, good_file));
  EXPECT_FALSE(permissions.HasAccessPermission(extension1, bad_file));
  EXPECT_FALSE(permissions.HasAccessPermission(extension2, good_dir));
  EXPECT_FALSE(permissions.HasAccessPermission(extension2, good_file));
  EXPECT_FALSE(permissions.HasAccessPermission(extension2, bad_file));
}
