// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/sandbox_isolated_origin_database.h"
#include "webkit/browser/fileapi/sandbox_origin_database.h"

namespace fileapi {

TEST(SandboxIsolatedOriginDatabaseTest, BasicTest) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());

  std::string kOrigin("origin");
  SandboxIsolatedOriginDatabase database(kOrigin, dir.path());

  EXPECT_TRUE(database.HasOriginPath(kOrigin));

  base::FilePath path1, path2;

  EXPECT_FALSE(database.GetPathForOrigin(std::string(), &path1));
  EXPECT_FALSE(database.GetPathForOrigin("foo", &path1));

  EXPECT_TRUE(database.HasOriginPath(kOrigin));
  EXPECT_TRUE(database.GetPathForOrigin(kOrigin, &path1));
  EXPECT_TRUE(database.GetPathForOrigin(kOrigin, &path2));
  EXPECT_FALSE(path1.empty());
  EXPECT_FALSE(path2.empty());
  EXPECT_EQ(path1, path2);
}

TEST(SandboxIsolatedOriginDatabaseTest, MigrationTest) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());

  std::string kOrigin("origin");
  std::string kFakeDirectoryData("0123456789");
  base::FilePath path;
  base::FilePath old_db_path;

  // Initialize the directory with one origin using the regular
  // SandboxOriginDatabase.
  {
    SandboxOriginDatabase database_old(dir.path());
    old_db_path = database_old.GetDatabasePath();
    EXPECT_FALSE(base::PathExists(old_db_path));
    EXPECT_TRUE(database_old.GetPathForOrigin(kOrigin, &path));
    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(base::DirectoryExists(old_db_path));

    // Populate the origin directory with some fake data.
    base::FilePath directory_db_path = dir.path().Append(path);
    ASSERT_TRUE(file_util::CreateDirectory(directory_db_path));
    EXPECT_EQ(static_cast<int>(kFakeDirectoryData.size()),
              file_util::WriteFile(directory_db_path.AppendASCII("dummy"),
                                   kFakeDirectoryData.data(),
                                   kFakeDirectoryData.size()));
  }

  // Re-open the directory using sandboxIsolatedOriginDatabase.
  SandboxIsolatedOriginDatabase database(kOrigin, dir.path());

  // The database is migrated from the old one, so we should still
  // see the same origin.
  EXPECT_TRUE(database.HasOriginPath(kOrigin));
  EXPECT_TRUE(database.GetPathForOrigin(kOrigin, &path));
  EXPECT_FALSE(path.empty());

  // The directory content must be kept (or migrated if necessary),
  // so we should see the same fake data.
  std::string origin_db_data;
  base::FilePath directory_db_path = dir.path().Append(path);
  EXPECT_TRUE(base::DirectoryExists(directory_db_path));
  EXPECT_TRUE(base::PathExists(directory_db_path.AppendASCII("dummy")));
  EXPECT_TRUE(file_util::ReadFileToString(
      directory_db_path.AppendASCII("dummy"), &origin_db_data));
  EXPECT_EQ(kFakeDirectoryData, origin_db_data);

  // After the migration the database must be gone.
  EXPECT_FALSE(base::PathExists(old_db_path));
}

}  // namespace fileapi
