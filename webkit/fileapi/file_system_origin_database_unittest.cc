// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include <algorithm>
#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/platform_file.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "third_party/leveldatabase/src/db/filename.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "webkit/fileapi/file_system_origin_database.h"
#include "webkit/fileapi/file_system_util.h"

namespace fileapi {

namespace {
const FilePath::CharType kFileSystemDirName[] =
    FILE_PATH_LITERAL("File System");
const FilePath::CharType kOriginDatabaseName[] = FILE_PATH_LITERAL("Origins");

void CorruptDatabase(const FilePath& db_path,
                     leveldb::FileType type,
                     ptrdiff_t offset,
                     size_t size) {
  file_util::FileEnumerator file_enum(
      db_path, false /* recursive */,
      static_cast<file_util::FileEnumerator::FileType>(
          file_util::FileEnumerator::DIRECTORIES |
          file_util::FileEnumerator::FILES));
  FilePath file_path;
  FilePath picked_file_path;
  uint64 picked_file_number = kuint64max;

  while (!(file_path = file_enum.Next()).empty()) {
    uint64 number = kuint64max;
    leveldb::FileType file_type;
    EXPECT_TRUE(leveldb::ParseFileName(FilePathToString(file_path.BaseName()),
                                       &number, &file_type));
    if (file_type == type &&
        (picked_file_number == kuint64max || picked_file_number < number)) {
      picked_file_path = file_path;
      picked_file_number = number;
    }
  }

  EXPECT_FALSE(picked_file_path.empty());
  EXPECT_NE(kuint64max, picked_file_number);

  bool created = true;
  base::PlatformFileError error = base::PLATFORM_FILE_ERROR_FAILED;
  base::PlatformFile file =
      CreatePlatformFile(picked_file_path,
                         base::PLATFORM_FILE_OPEN |
                         base::PLATFORM_FILE_READ |
                         base::PLATFORM_FILE_WRITE,
                         &created, &error);
  EXPECT_EQ(base::PLATFORM_FILE_OK, error);
  EXPECT_FALSE(created);

  base::PlatformFileInfo file_info;
  EXPECT_TRUE(base::GetPlatformFileInfo(file, &file_info));
  if (offset < 0)
    offset += file_info.size;
  EXPECT_GE(offset, 0);
  EXPECT_LE(offset, file_info.size);

  size = std::min(size, static_cast<size_t>(file_info.size - offset));

  std::vector<char> buf(size);
  int read_size = base::ReadPlatformFile(file, offset,
                                         vector_as_array(&buf), buf.size());
  EXPECT_LT(0, read_size);
  EXPECT_GE(buf.size(), static_cast<size_t>(read_size));
  buf.resize(read_size);

  std::transform(buf.begin(), buf.end(), buf.begin(),
                 std::logical_not<char>());

  int written_size = base::WritePlatformFile(file, offset,
                                             vector_as_array(&buf), buf.size());
  EXPECT_GT(written_size, 0);
  EXPECT_EQ(buf.size(), static_cast<size_t>(written_size));

  base::ClosePlatformFile(file);
}

}

TEST(FileSystemOriginDatabaseTest, BasicTest) {
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const FilePath kFSDir = dir.path().Append(kFileSystemDirName);
  EXPECT_FALSE(file_util::PathExists(kFSDir));
  EXPECT_TRUE(file_util::CreateDirectory(kFSDir));

  FileSystemOriginDatabase database(kFSDir);
  std::string origin("origin");

  EXPECT_FALSE(database.HasOriginPath(origin));
  // Double-check to make sure that had no side effects.
  EXPECT_FALSE(database.HasOriginPath(origin));

  FilePath path0;
  FilePath path1;

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
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const FilePath kFSDir = dir.path().Append(kFileSystemDirName);
  EXPECT_FALSE(file_util::PathExists(kFSDir));
  EXPECT_TRUE(file_util::CreateDirectory(kFSDir));

  FileSystemOriginDatabase database(kFSDir);
  std::string origin0("origin0");
  std::string origin1("origin1");

  EXPECT_FALSE(database.HasOriginPath(origin0));
  EXPECT_FALSE(database.HasOriginPath(origin1));

  FilePath path0;
  FilePath path1;
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
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const FilePath kFSDir = dir.path().Append(kFileSystemDirName);
  EXPECT_FALSE(file_util::PathExists(kFSDir));
  EXPECT_TRUE(file_util::CreateDirectory(kFSDir));

  FileSystemOriginDatabase database(kFSDir);
  std::string origin("origin");

  EXPECT_FALSE(database.HasOriginPath(origin));

  FilePath path0;
  EXPECT_TRUE(database.GetPathForOrigin(origin, &path0));
  EXPECT_TRUE(database.HasOriginPath(origin));
  EXPECT_FALSE(path0.empty());

  EXPECT_TRUE(file_util::PathExists(kFSDir.Append(kOriginDatabaseName)));

  database.DropDatabase();

  FilePath path1;
  EXPECT_TRUE(database.HasOriginPath(origin));
  EXPECT_TRUE(database.GetPathForOrigin(origin, &path1));
  EXPECT_FALSE(path1.empty());
  EXPECT_EQ(path0, path1);
}

TEST(FileSystemOriginDatabaseTest, DeleteOriginTest) {
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const FilePath kFSDir = dir.path().Append(kFileSystemDirName);
  EXPECT_FALSE(file_util::PathExists(kFSDir));
  EXPECT_TRUE(file_util::CreateDirectory(kFSDir));

  FileSystemOriginDatabase database(kFSDir);
  std::string origin("origin");

  EXPECT_FALSE(database.HasOriginPath(origin));
  EXPECT_TRUE(database.RemovePathForOrigin(origin));

  FilePath path0;
  EXPECT_TRUE(database.GetPathForOrigin(origin, &path0));
  EXPECT_TRUE(database.HasOriginPath(origin));
  EXPECT_FALSE(path0.empty());

  EXPECT_TRUE(database.RemovePathForOrigin(origin));
  EXPECT_FALSE(database.HasOriginPath(origin));

  FilePath path1;
  EXPECT_TRUE(database.GetPathForOrigin(origin, &path1));
  EXPECT_FALSE(path1.empty());
  EXPECT_NE(path0, path1);
}

TEST(FileSystemOriginDatabaseTest, ListOriginsTest) {
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const FilePath kFSDir = dir.path().Append(kFileSystemDirName);
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

  FilePath path0;
  FilePath path1;
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

  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const FilePath kFSDir = dir.path().Append(kFileSystemDirName);
  const FilePath kDBDir = kFSDir.Append(kOriginDatabaseName);
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
    FilePath path;
    EXPECT_FALSE(database->HasOriginPath(kOrigins[i]));
    EXPECT_TRUE(database->GetPathForOrigin(kOrigins[i], &path));
    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(database->GetPathForOrigin(kOrigins[i], &path));

    if (i != 1)
      EXPECT_TRUE(file_util::CreateDirectory(kFSDir.Append(path)));
  }
  database.reset();

  const FilePath kGarbageDir = kFSDir.AppendASCII("foo");
  const FilePath kGarbageFile = kGarbageDir.AppendASCII("bar");
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

  FilePath path;
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
