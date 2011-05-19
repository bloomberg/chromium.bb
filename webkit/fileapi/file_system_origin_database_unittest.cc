// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include <string>

#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "webkit/fileapi/file_system_origin_database.h"

namespace fileapi {

TEST(FileSystemOriginDatabaseTest, BasicTest) {
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const FilePath kDBFile = dir.path().AppendASCII("fsod.db");
  EXPECT_FALSE(file_util::PathExists(kDBFile));

  FileSystemOriginDatabase database(kDBFile);
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

  EXPECT_TRUE(file_util::PathExists(kDBFile));
}

TEST(FileSystemOriginDatabaseTest, TwoPathTest) {
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const FilePath kDBFile = dir.path().AppendASCII("fsod.db");
  EXPECT_FALSE(file_util::PathExists(kDBFile));

  FileSystemOriginDatabase database(kDBFile);
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

  EXPECT_TRUE(file_util::PathExists(kDBFile));
}

TEST(FileSystemOriginDatabaseTest, DropDatabaseTest) {
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const FilePath kDBFile = dir.path().AppendASCII("fsod.db");
  EXPECT_FALSE(file_util::PathExists(kDBFile));

  FileSystemOriginDatabase database(kDBFile);
  std::string origin("origin");

  EXPECT_FALSE(database.HasOriginPath(origin));

  FilePath path0;
  EXPECT_TRUE(database.GetPathForOrigin(origin, &path0));
  EXPECT_TRUE(database.HasOriginPath(origin));
  EXPECT_FALSE(path0.empty());

  EXPECT_TRUE(file_util::PathExists(kDBFile));

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
  const FilePath kDBFile = dir.path().AppendASCII("fsod.db");
  EXPECT_FALSE(file_util::PathExists(kDBFile));

  FileSystemOriginDatabase database(kDBFile);
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
  const FilePath kDBFile = dir.path().AppendASCII("fsod.db");
  EXPECT_FALSE(file_util::PathExists(kDBFile));

  std::vector<FileSystemOriginDatabase::OriginRecord> origins;

  FileSystemOriginDatabase database(kDBFile);
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
  EXPECT_EQ(origins[0].first, origin0);
  EXPECT_EQ(origins[0].second, path0);
  origins.clear();
  EXPECT_TRUE(database.GetPathForOrigin(origin1, &path1));
  EXPECT_TRUE(database.ListAllOrigins(&origins));
  EXPECT_EQ(origins.size(), 2UL);
  if (origins[0].first == origin0) {
    EXPECT_EQ(origins[0].second, path0);
    EXPECT_EQ(origins[1].first, origin1);
    EXPECT_EQ(origins[1].second, path1);
  } else {
    EXPECT_EQ(origins[0].first, origin1);
    EXPECT_EQ(origins[0].second, path1);
    EXPECT_EQ(origins[1].first, origin0);
    EXPECT_EQ(origins[1].second, path0);
  }
}

}  // namespace fileapi
