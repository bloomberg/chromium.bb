// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_usage_cache.h"

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace fileapi;

class FileSystemUsageCacheTest : public testing::Test {
 public:
  FileSystemUsageCacheTest() {}

  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
  }

protected:
  FilePath GetUsageFilePath() {
    return data_dir_.path().AppendASCII(FileSystemUsageCache::kUsageFileName);
  }

 private:
  ScopedTempDir data_dir_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemUsageCacheTest);
};

TEST_F(FileSystemUsageCacheTest, CreateTest) {
  FilePath usage_file_path = GetUsageFilePath();
  EXPECT_EQ(FileSystemUsageCache::kUsageFileSize,
            FileSystemUsageCache::UpdateUsage(usage_file_path, 0));
}

TEST_F(FileSystemUsageCacheTest, SetSizeTest) {
  static const int64 size = 240122;
  FilePath usage_file_path = GetUsageFilePath();
  ASSERT_EQ(FileSystemUsageCache::kUsageFileSize,
            FileSystemUsageCache::UpdateUsage(usage_file_path, size));
  EXPECT_EQ(size, FileSystemUsageCache::GetUsage(usage_file_path));
}

TEST_F(FileSystemUsageCacheTest, SetLargeSizeTest) {
  static const int64 size = kint64max;
  FilePath usage_file_path = GetUsageFilePath();
  ASSERT_EQ(FileSystemUsageCache::kUsageFileSize,
            FileSystemUsageCache::UpdateUsage(usage_file_path, size));
  EXPECT_EQ(size, FileSystemUsageCache::GetUsage(usage_file_path));
}

TEST_F(FileSystemUsageCacheTest, IncAndGetSizeTest) {
  FilePath usage_file_path = GetUsageFilePath();
  ASSERT_EQ(FileSystemUsageCache::kUsageFileSize,
            FileSystemUsageCache::UpdateUsage(usage_file_path, 98214));
  ASSERT_TRUE(FileSystemUsageCache::IncrementDirty(usage_file_path));
  EXPECT_EQ(-1, FileSystemUsageCache::GetUsage(usage_file_path));
}

TEST_F(FileSystemUsageCacheTest, DecAndGetSizeTest) {
  static const int64 size = 71839;
  FilePath usage_file_path = GetUsageFilePath();
  ASSERT_EQ(FileSystemUsageCache::kUsageFileSize,
            FileSystemUsageCache::UpdateUsage(usage_file_path, size));
  // DecrementDirty for dirty = 0 is invalid. It returns false.
  ASSERT_FALSE(FileSystemUsageCache::DecrementDirty(usage_file_path));
  EXPECT_EQ(size, FileSystemUsageCache::GetUsage(usage_file_path));
}

TEST_F(FileSystemUsageCacheTest, IncDecAndGetSizeTest) {
  static const int64 size = 198491;
  FilePath usage_file_path = GetUsageFilePath();
  ASSERT_EQ(FileSystemUsageCache::kUsageFileSize,
            FileSystemUsageCache::UpdateUsage(usage_file_path, size));
  ASSERT_TRUE(FileSystemUsageCache::IncrementDirty(usage_file_path));
  ASSERT_TRUE(FileSystemUsageCache::DecrementDirty(usage_file_path));
  EXPECT_EQ(size, FileSystemUsageCache::GetUsage(usage_file_path));
}

TEST_F(FileSystemUsageCacheTest, DecIncAndGetSizeTest) {
  FilePath usage_file_path = GetUsageFilePath();
  ASSERT_EQ(FileSystemUsageCache::kUsageFileSize,
            FileSystemUsageCache::UpdateUsage(usage_file_path, 854238));
  // DecrementDirty for dirty = 0 is invalid. It returns false.
  ASSERT_FALSE(FileSystemUsageCache::DecrementDirty(usage_file_path));
  ASSERT_TRUE(FileSystemUsageCache::IncrementDirty(usage_file_path));
  // It tests DecrementDirty (which returns false) has no effect, i.e
  // does not make dirty = -1 after DecrementDirty.
  EXPECT_EQ(-1, FileSystemUsageCache::GetUsage(usage_file_path));
}

TEST_F(FileSystemUsageCacheTest, ManyIncsSameDecsAndGetSizeTest) {
  static const int64 size = 82412;
  FilePath usage_file_path = GetUsageFilePath();
  ASSERT_EQ(FileSystemUsageCache::kUsageFileSize,
            FileSystemUsageCache::UpdateUsage(usage_file_path, size));
  for (int i = 0; i < 20; i++)
    ASSERT_TRUE(FileSystemUsageCache::IncrementDirty(usage_file_path));
  for (int i = 0; i < 20; i++)
    ASSERT_TRUE(FileSystemUsageCache::DecrementDirty(usage_file_path));
  EXPECT_EQ(size, FileSystemUsageCache::GetUsage(usage_file_path));
}

TEST_F(FileSystemUsageCacheTest, ManyIncsLessDecsAndGetSizeTest) {
  FilePath usage_file_path = GetUsageFilePath();
  ASSERT_EQ(FileSystemUsageCache::kUsageFileSize,
            FileSystemUsageCache::UpdateUsage(usage_file_path, 19319));
  for (int i = 0; i < 20; i++)
    ASSERT_TRUE(FileSystemUsageCache::IncrementDirty(usage_file_path));
  for (int i = 0; i < 19; i++)
    ASSERT_TRUE(FileSystemUsageCache::DecrementDirty(usage_file_path));
  EXPECT_EQ(-1, FileSystemUsageCache::GetUsage(usage_file_path));
}

TEST_F(FileSystemUsageCacheTest, GetSizeWithoutCacheFileTest) {
  FilePath usage_file_path = GetUsageFilePath();
  EXPECT_EQ(-1, FileSystemUsageCache::GetUsage(usage_file_path));
}

TEST_F(FileSystemUsageCacheTest, IncrementDirtyWithoutCacheFileTest) {
  FilePath usage_file_path = GetUsageFilePath();
  EXPECT_FALSE(FileSystemUsageCache::IncrementDirty(usage_file_path));
}

TEST_F(FileSystemUsageCacheTest, DecrementDirtyWithoutCacheFileTest) {
  FilePath usage_file_path = GetUsageFilePath();
  EXPECT_FALSE(FileSystemUsageCache::IncrementDirty(usage_file_path));
}
