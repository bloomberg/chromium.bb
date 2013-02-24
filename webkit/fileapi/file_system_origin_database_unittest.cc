// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <functional>
#include <limits>
#include <string>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/platform_file.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/db/filename.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "webkit/fileapi/file_system_database_test_helper.h"
#include "webkit/fileapi/file_system_origin_database.h"
#include "webkit/fileapi/file_system_util.h"

namespace fileapi {

namespace {
const base::FilePath::CharType kFileSystemDirName[] =
    FILE_PATH_LITERAL("File System");
const base::FilePath::CharType kOriginDatabaseName[] = FILE_PATH_LITERAL("Origins");
}  // namespace

TEST(FileSystemOriginDatabaseTest, BasicTest) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const base::FilePath kFSDir = dir.path().Append(kFileSystemDirName);
  EXPECT_FALSE(file_util::PathExists(kFSDir));
  EXPECT_TRUE(file_util::CreateDirectory(kFSDir));

  FileSystemOriginDatabase database(kFSDir);
  std::string origin("origin");

  EXPECT_FALSE(database.HasOriginPath(origin));
  // Double-check to make sure that had no side effects.
  EXPECT_FALSE(database.HasOriginPath(origin));

  base::FilePath path0;
  base::FilePath path1;

  // Empty strings aren't valid origins.
  EXPECT_FALSE(database.GetPathForOrigin(std::string(), &path0));

  EXPECT_TRUE(database.GetPathForOrigin(origin, &path0));
  EXPECT_TRUE(database.HasOriginPath(origin));
  EXPECT_TRUE(database.GetPathForOrigin(origin, &path1));
  EXPECT_FALSE(path0.empty());
  EXPECT_FALSE(path1.empty());
  EXPECT_EQ(path0, path1);

  EXPECT_TRUE(file_util::PathExists(kFSDir.Append(kOriginDatabaseName)));
}

TEST(FileSystemOriginDatabaseTest, TwoPathTest) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const base::FilePath kFSDir = dir.path().Append(kFileSystemDirName);
  EXPECT_FALSE(file_util::PathExists(kFSDir));
  EXPECT_TRUE(file_util::CreateDirectory(kFSDir));

  FileSystemOriginDatabase database(kFSDir);
  std::string origin0("origin0");
  std::string origin1("origin1");

  EXPECT_FALSE(database.HasOriginPath(origin0));
  EXPECT_FALSE(database.HasOriginPath(origin1));

  base::FilePath path0;
  base::FilePath path1;
  EXPECT_TRUE(database.GetPathForOrigin(origin0, &path0));
  EXPECT_TRUE(database.HasOriginPath(origin0));
  EXPECT_FALSE(database.HasOriginPath(origin1));
  EXPECT_TRUE(database.GetPathForOrigin(origin1, &path1));
  EXPECT_TRUE(database.HasOriginPath(origin1));
  EXPECT_FALSE(path0.empty());
  EXPECT_FALSE(path1.empty());
  EXPECT_NE(path0, path1);

  EXPECT_TRUE(file_util::PathExists(kFSDir.Append(kOriginDatabaseName)));
}

TEST(FileSystemOriginDatabaseTest, DropDatabaseTest) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const base::FilePath kFSDir = dir.path().Append(kFileSystemDirName);
  EXPECT_FALSE(file_util::PathExists(kFSDir));
  EXPECT_TRUE(file_util::CreateDirectory(kFSDir));

  FileSystemOriginDatabase database(kFSDir);
  std::string origin("origin");

  EXPECT_FALSE(database.HasOriginPath(origin));

  base::FilePath path0;
  EXPECT_TRUE(database.GetPathForOrigin(origin, &path0));
  EXPECT_TRUE(database.HasOriginPath(origin));
  EXPECT_FALSE(path0.empty());

  EXPECT_TRUE(file_util::PathExists(kFSDir.Append(kOriginDatabaseName)));

  database.DropDatabase();

  base::FilePath path1;
  EXPECT_TRUE(database.HasOriginPath(origin));
  EXPECT_TRUE(database.GetPathForOrigin(origin, &path1));
  EXPECT_FALSE(path1.empty());
  EXPECT_EQ(path0, path1);
}

TEST(FileSystemOriginDatabaseTest, DeleteOriginTest) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const base::FilePath kFSDir = dir.path().Append(kFileSystemDirName);
  EXPECT_FALSE(file_util::PathExists(kFSDir));
  EXPECT_TRUE(file_util::CreateDirectory(kFSDir));

  FileSystemOriginDatabase database(kFSDir);
  std::string origin("origin");

  EXPECT_FALSE(database.HasOriginPath(origin));
  EXPECT_TRUE(database.RemovePathForOrigin(origin));

  base::FilePath path0;
  EXPECT_TRUE(database.GetPathForOrigin(origin, &path0));
  EXPECT_TRUE(database.HasOriginPath(origin));
  EXPECT_FALSE(path0.empty());

  EXPECT_TRUE(database.RemovePathForOrigin(origin));
  EXPECT_FALSE(database.HasOriginPath(origin));

  base::FilePath path1;
  EXPECT_TRUE(database.GetPathForOrigin(origin, &path1));
  EXPECT_FALSE(path1.empty());
  EXPECT_NE(path0, path1);
}

TEST(FileSystemOriginDatabaseTest, ListOriginsTest) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const base::FilePath kFSDir = dir.path().Append(kFileSystemDirName);
  EXPECT_FALSE(file_util::PathExists(kFSDir));
  EXPECT_TRUE(file_util::CreateDirectory(kFSDir));

  std::vector<FileSystemOriginDatabase::OriginRecord> origins;

  FileSystemOriginDatabase database(kFSDir);
  EXPECT_TRUE(database.ListAllOrigins(&origins));
  EXPECT_TRUE(origins.empty());
  origins.clear();

  std::string origin0("origin0");
  std::string origin1("origin1");

  EXPECT_FALSE(database.HasOriginPath(origin0));
  EXPECT_FALSE(database.HasOriginPath(origin1));

  base::FilePath path0;
  base::FilePath path1;
  EXPECT_TRUE(database.GetPathForOrigin(origin0, &path0));
  EXPECT_TRUE(database.ListAllOrigins(&origins));
  EXPECT_EQ(origins.size(), 1UL);
  EXPECT_EQ(origins[0].origin, origin0);
  EXPECT_EQ(origins[0].path, path0);
  origins.clear();
  EXPECT_TRUE(database.GetPathForOrigin(origin1, &path1));
  EXPECT_TRUE(database.ListAllOrigins(&origins));
  EXPECT_EQ(origins.size(), 2UL);
  if (origins[0].origin == origin0) {
    EXPECT_EQ(origins[0].path, path0);
    EXPECT_EQ(origins[1].origin, origin1);
    EXPECT_EQ(origins[1].path, path1);
  } else {
    EXPECT_EQ(origins[0].origin, origin1);
    EXPECT_EQ(origins[0].path, path1);
    EXPECT_EQ(origins[1].origin, origin0);
    EXPECT_EQ(origins[1].path, path0);
  }
}

TEST(FileSystemOriginDatabaseTest, DatabaseRecoveryTest) {
  // Checks if FileSystemOriginDatabase properly handles database corruption.
  // In this test, we'll register some origins to the origin database, then
  // corrupt database and its log file.
  // After repairing, the origin database should be consistent even when some
  // entries lost.

  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const base::FilePath kFSDir = dir.path().Append(kFileSystemDirName);
  const base::FilePath kDBDir = kFSDir.Append(kOriginDatabaseName);
  EXPECT_FALSE(file_util::PathExists(kFSDir));
  EXPECT_TRUE(file_util::CreateDirectory(kFSDir));

  const std::string kOrigins[] = {
    "foo.example.com",
    "bar.example.com",
    "baz.example.com",
    "hoge.example.com",
    "fuga.example.com",
  };

  scoped_ptr<FileSystemOriginDatabase> database(
      new FileSystemOriginDatabase(kFSDir));
  for (size_t i = 0; i < arraysize(kOrigins); ++i) {
    base::FilePath path;
    EXPECT_FALSE(database->HasOriginPath(kOrigins[i]));
    EXPECT_TRUE(database->GetPathForOrigin(kOrigins[i], &path));
    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(database->GetPathForOrigin(kOrigins[i], &path));

    if (i != 1)
      EXPECT_TRUE(file_util::CreateDirectory(kFSDir.Append(path)));
  }
  database.reset();

  const base::FilePath kGarbageDir = kFSDir.AppendASCII("foo");
  const base::FilePath kGarbageFile = kGarbageDir.AppendASCII("bar");
  EXPECT_TRUE(file_util::CreateDirectory(kGarbageDir));
  bool created = false;
  base::PlatformFileError error;
  base::PlatformFile file = base::CreatePlatformFile(
      kGarbageFile,
      base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE,
      &created, &error);
  EXPECT_EQ(base::PLATFORM_FILE_OK, error);
  EXPECT_TRUE(created);
  EXPECT_TRUE(base::ClosePlatformFile(file));

  // Corrupt database itself and last log entry to drop last 1 database
  // operation.  The database should detect the corruption and should recover
  // its consistency after recovery.
  CorruptDatabase(kDBDir, leveldb::kDescriptorFile,
                  0, std::numeric_limits<size_t>::max());
  CorruptDatabase(kDBDir, leveldb::kLogFile, -1, 1);

  base::FilePath path;
  database.reset(new FileSystemOriginDatabase(kFSDir));
  std::vector<FileSystemOriginDatabase::OriginRecord> origins_in_db;
  EXPECT_TRUE(database->ListAllOrigins(&origins_in_db));

  // Expect all but last added origin will be repaired back, and kOrigins[1]
  // should be dropped due to absence of backing directory.
  EXPECT_EQ(arraysize(kOrigins) - 2, origins_in_db.size());

  const std::string kOrigin("piyo.example.org");
  EXPECT_FALSE(database->HasOriginPath(kOrigin));
  EXPECT_TRUE(database->GetPathForOrigin(kOrigin, &path));
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(database->HasOriginPath(kOrigin));

  EXPECT_FALSE(file_util::PathExists(kGarbageFile));
  EXPECT_FALSE(file_util::PathExists(kGarbageDir));
}

}  // namespace fileapi
