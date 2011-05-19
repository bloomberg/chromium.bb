// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/quota_file_util.h"

#include "base/file_path.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_test_helper.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/local_file_system_file_util.h"

namespace fileapi {

class QuotaFileUtilTest : public testing::Test {
 public:
  QuotaFileUtilTest()
      : callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  }

  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    FilePath base_dir = data_dir_.path().AppendASCII("filesystem");

    // For path translation we rely on LocalFileSystemFileUtil::GetLocalPath().
    test_helper_.SetUp(base_dir, LocalFileSystemFileUtil::GetInstance());
  }

  void TearDown() {
    test_helper_.TearDown();
  }

 protected:
  FileSystemOperationContext* NewContext() {
    return test_helper_.NewOperationContext();
  }

  QuotaFileUtil* FileUtil() {
    return QuotaFileUtil::GetInstance();
  }

  FilePath Path(const std::string& file_name) {
    return test_helper_.GetLocalPathFromASCII(file_name);
  }

  base::PlatformFileError CreateFile(const char* file_name,
      base::PlatformFile* file_handle, bool* created) {
    int file_flags = base::PLATFORM_FILE_CREATE |
        base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_ASYNC;

    scoped_ptr<FileSystemOperationContext> context(NewContext());
    return FileUtil()->CreateOrOpen(context.get(), Path(file_name),
        file_flags, file_handle, created);
  }

  base::PlatformFileError EnsureFileExists(const char* file_name,
      bool* created) {
    scoped_ptr<FileSystemOperationContext> context(NewContext());
    return FileUtil()->EnsureFileExists(context.get(),
                                        Path(file_name), created);
  }

  int64 GetCachedUsage() {
    return FileSystemUsageCache::GetUsage(test_helper_.GetUsageCachePath());
  }

  FileSystemContext* file_system_context() const {
    return test_helper_.file_system_context();
  }

 private:
  ScopedTempDir data_dir_;
  FileSystemTestOriginHelper test_helper_;
  base::ScopedCallbackFactory<QuotaFileUtilTest> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuotaFileUtilTest);
};

TEST_F(QuotaFileUtilTest, CreateAndClose) {
  const char *file_name = "test_file";
  base::PlatformFile file_handle;
  bool created;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            CreateFile(file_name, &file_handle, &created));
  ASSERT_TRUE(created);

  scoped_ptr<FileSystemOperationContext> context(NewContext());
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            FileUtil()->Close(context.get(), file_handle));
}

TEST_F(QuotaFileUtilTest, EnsureFileExists) {
  const char *file_name = "foobar";
  bool created;
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(file_name, &created));
  ASSERT_TRUE(created);

  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(file_name, &created));
  EXPECT_FALSE(created);
}

TEST_F(QuotaFileUtilTest, Truncate) {
  const char *file_name = "truncated";
  bool created;

  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(file_name, &created));
  ASSERT_TRUE(created);

  scoped_ptr<FileSystemOperationContext> truncate_context;

  truncate_context.reset(NewContext());
  truncate_context->set_allowed_bytes_growth(1020);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(truncate_context.get(),
                                                   Path(file_name),
                                                   1020));
  ASSERT_EQ(1020, GetCachedUsage());

  truncate_context.reset(NewContext());
  truncate_context->set_allowed_bytes_growth(0);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(truncate_context.get(),
                                                   Path(file_name),
                                                   0));
  ASSERT_EQ(0, GetCachedUsage());

  truncate_context.reset(NewContext());
  truncate_context->set_allowed_bytes_growth(1020);
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            QuotaFileUtil::GetInstance()->Truncate(truncate_context.get(),
                                                   Path(file_name),
                                                   1021));
  ASSERT_EQ(0, GetCachedUsage());
}

TEST_F(QuotaFileUtilTest, CopyFile) {
  const char *from_file = "fromfile";
  const char *obstacle_file = "obstaclefile";
  const char *to_file1 = "tofile1";
  const char *to_file2 = "tofile2";
  bool created;
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(obstacle_file, &created));
  ASSERT_TRUE(created);
  scoped_ptr<FileSystemOperationContext> context;

  context.reset(NewContext());
  context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(context.get(),
                                                   Path(from_file),
                                                   1020));
  ASSERT_EQ(1020, GetCachedUsage());

  context.reset(NewContext());
  context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(context.get(),
                                                   Path(obstacle_file),
                                                   1));
  ASSERT_EQ(1021, GetCachedUsage());

  context.reset(NewContext());
  context->set_allowed_bytes_growth(1020);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Copy(context.get(),
                                               Path(from_file),
                                               Path(to_file1)));
  ASSERT_EQ(2041, GetCachedUsage());

  context.reset(NewContext());
  context->set_allowed_bytes_growth(1019);
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            QuotaFileUtil::GetInstance()->Copy(context.get(),
                                               Path(from_file),
                                               Path(to_file2)));
  ASSERT_EQ(2041, GetCachedUsage());

  context.reset(NewContext());
  context->set_allowed_bytes_growth(1019);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Copy(context.get(),
                                               Path(from_file),
                                               Path(obstacle_file)));
  ASSERT_EQ(3060, GetCachedUsage());
}

TEST_F(QuotaFileUtilTest, CopyDirectory) {
  const char *from_dir = "fromdir";
  const char *from_file = "fromdir/fromfile";
  const char *to_dir1 = "todir1";
  const char *to_dir2 = "todir2";
  bool created;
  scoped_ptr<FileSystemOperationContext> context;

  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->CreateDirectory(context.get(),
                                                          Path(from_dir),
                                                          false, false));
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);

  context.reset(NewContext());
  context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(context.get(),
                                                   Path(from_file),
                                                   1020));
  ASSERT_EQ(1020, GetCachedUsage());

  context.reset(NewContext());
  context->set_allowed_bytes_growth(1020);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Copy(context.get(),
                                               Path(from_dir),
                                               Path(to_dir1)));
  ASSERT_EQ(2040, GetCachedUsage());

  context.reset(NewContext());
  context->set_allowed_bytes_growth(1019);
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            QuotaFileUtil::GetInstance()->Copy(context.get(),
                                               Path(from_dir),
                                               Path(to_dir2)));
  ASSERT_EQ(2040, GetCachedUsage());
}

TEST_F(QuotaFileUtilTest, MoveFile) {
  const char *from_file = "fromfile";
  const char *obstacle_file = "obstaclefile";
  const char *to_file = "tofile";
  bool created;
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);
  scoped_ptr<FileSystemOperationContext> context;

  context.reset(NewContext());
  context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(context.get(),
                                                   Path(from_file),
                                                   1020));
  ASSERT_EQ(1020, GetCachedUsage());

  context.reset(NewContext());
  context->set_allowed_bytes_growth(0);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Move(context.get(),
                                               Path(from_file),
                                               Path(to_file)));
  ASSERT_EQ(1020, GetCachedUsage());

  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(obstacle_file, &created));
  ASSERT_TRUE(created);

  context.reset(NewContext());
  context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(context.get(),
                                                   Path(from_file),
                                                   1020));
  ASSERT_EQ(2040, GetCachedUsage());

  context.reset(NewContext());
  context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(context.get(),
                                                   Path(obstacle_file),
                                                   1));
  ASSERT_EQ(2041, GetCachedUsage());

  context.reset(NewContext());
  context->set_allowed_bytes_growth(0);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Move(context.get(),
                                               Path(from_file),
                                               Path(obstacle_file)));
  ASSERT_EQ(2040, GetCachedUsage());
}

TEST_F(QuotaFileUtilTest, MoveDirectory) {
  const char *from_dir = "fromdir";
  const char *from_file = "fromdir/fromfile";
  const char *to_dir1 = "todir1";
  const char *to_dir2 = "todir2";
  bool created;
  scoped_ptr<FileSystemOperationContext> context;

  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->CreateDirectory(context.get(),
                                                          Path(from_dir),
                                                          false, false));
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);

  context.reset(NewContext());
  context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(context.get(),
                                                   Path(from_file),
                                                   1020));
  ASSERT_EQ(1020, GetCachedUsage());

  context.reset(NewContext());
  context->set_allowed_bytes_growth(1020);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Move(context.get(),
                                               Path(from_dir),
                                               Path(to_dir1)));
  ASSERT_EQ(1020, GetCachedUsage());

  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->CreateDirectory(context.get(),
                                                          Path(from_dir),
                                                          false, false));
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);

  context.reset(NewContext());
  context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(context.get(),
                                                   Path(from_file),
                                                   1020));
  ASSERT_EQ(2040, GetCachedUsage());

  context.reset(NewContext());
  context->set_allowed_bytes_growth(1019);
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Move(context.get(),
                                               Path(from_dir),
                                               Path(to_dir2)));
  ASSERT_EQ(2040, GetCachedUsage());
}

TEST_F(QuotaFileUtilTest, Remove) {
  const char *dir = "dir";
  const char *file = "file";
  const char *dfile1 = "dir/dfile1";
  const char *dfile2 = "dir/dfile2";
  bool created;
  scoped_ptr<FileSystemOperationContext> context;

  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(file, &created));
  ASSERT_TRUE(created);
  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->CreateDirectory(context.get(),
                                                          Path(dir),
                                                          false, false));
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(dfile1, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(dfile2, &created));
  ASSERT_TRUE(created);

  context.reset(NewContext());
  context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(context.get(),
                                                   Path(file),
                                                   340));
  ASSERT_EQ(340, GetCachedUsage());

  context.reset(NewContext());
  context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(context.get(),
                                                   Path(dfile1),
                                                   1020));
  ASSERT_EQ(1360, GetCachedUsage());

  context.reset(NewContext());
  context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(context.get(),
                                                   Path(dfile2),
                                                   120));
  ASSERT_EQ(1480, GetCachedUsage());

  context.reset(NewContext());
  context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Delete(context.get(),
                                                 Path(file),
                                                 false));
  ASSERT_EQ(1140, GetCachedUsage());

  context.reset(NewContext());
  context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Delete(context.get(),
                                                 Path(dir),
                                                 true));
  ASSERT_EQ(0, GetCachedUsage());
}

}  // namespace fileapi
