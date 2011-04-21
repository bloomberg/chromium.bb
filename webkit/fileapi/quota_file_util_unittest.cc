// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/quota_file_util.h"

#include "base/file_path.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/platform_file.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_operation_context.h"

using namespace fileapi;

class QuotaFileUtilTest : public testing::Test {
 public:
  QuotaFileUtilTest()
      : callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  }

  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
  }

 protected:
  FileSystemOperationContext* Context() {
    return new FileSystemOperationContext(NULL, QuotaFileUtil::GetInstance());
  }

  QuotaFileUtil* FileUtil() {
    return QuotaFileUtil::GetInstance();
  }

  FilePath Path(const char *file_name) {
    return data_dir_.path().AppendASCII(file_name);
  }

  base::PlatformFileError CreateFile(const char* file_name,
      base::PlatformFile* file_handle, bool* created) {
    int file_flags = base::PLATFORM_FILE_CREATE |
        base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_ASYNC;

    return FileUtil()->CreateOrOpen(Context(),
        data_dir_.path().AppendASCII(file_name),
        file_flags, file_handle, created);
  }

  base::PlatformFileError EnsureFileExists(const char* file_name,
      bool* created) {
    return FileUtil()->EnsureFileExists(Context(),
        data_dir_.path().AppendASCII(file_name), created);
  }

 private:
  ScopedTempDir data_dir_;
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

  EXPECT_EQ(base::PLATFORM_FILE_OK,
            FileUtil()->Close(Context(), file_handle));
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

  FileSystemOperationContext *truncate_context;

  truncate_context = Context();
  truncate_context->set_allowed_bytes_growth(1020);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(truncate_context,
                                                   Path(file_name),
                                                   1020));

  truncate_context = Context();
  truncate_context->set_allowed_bytes_growth(0);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(truncate_context,
                                                   Path(file_name),
                                                   0));

  truncate_context = Context();
  truncate_context->set_allowed_bytes_growth(1020);
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            QuotaFileUtil::GetInstance()->Truncate(truncate_context,
                                                   Path(file_name),
                                                   1021));
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
  FileSystemOperationContext *context;

  context = Context();
  context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(context,
                                                   Path(from_file),
                                                   1020));

  context = Context();
  context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(context,
                                                   Path(obstacle_file),
                                                   1));

  context = Context();
  context->set_allowed_bytes_growth(1020);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Copy(context,
                                               Path(from_file),
                                               Path(to_file1)));

  context = Context();
  context->set_allowed_bytes_growth(1019);
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            QuotaFileUtil::GetInstance()->Copy(context,
                                               Path(from_file),
                                               Path(to_file2)));

  context = Context();
  context->set_allowed_bytes_growth(1019);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Copy(context,
                                               Path(from_file),
                                               Path(obstacle_file)));
}

TEST_F(QuotaFileUtilTest, CopyDirectory) {
  const char *from_dir = "fromdir";
  const char *from_file = "fromdir/fromfile";
  const char *to_dir1 = "todir1";
  const char *to_dir2 = "todir2";
  bool created;
  FileSystemOperationContext *context;

  context = Context();
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->CreateDirectory(context,
                                                          Path(from_dir),
                                                          false, false));
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);

  context = Context();
  context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(context,
                                                   Path(from_file),
                                                   1020));

  context = Context();
  context->set_allowed_bytes_growth(1020);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Copy(context,
                                               Path(from_dir),
                                               Path(to_dir1)));

  context = Context();
  context->set_allowed_bytes_growth(1019);
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            QuotaFileUtil::GetInstance()->Copy(context,
                                               Path(from_dir),
                                               Path(to_dir2)));
}

TEST_F(QuotaFileUtilTest, MoveFile) {
  const char *from_file = "fromfile";
  const char *obstacle_file = "obstaclefile";
  const char *to_file = "tofile";
  bool created;
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);
  FileSystemOperationContext *context;

  context = Context();
  context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(context,
                                                   Path(from_file),
                                                   1020));

  context = Context();
  context->set_allowed_bytes_growth(0);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Move(context,
                                               Path(from_file),
                                               Path(to_file)));

  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(obstacle_file, &created));
  ASSERT_TRUE(created);

  context = Context();
  context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(context,
                                                   Path(from_file),
                                                   1020));

  context = Context();
  context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(context,
                                                   Path(obstacle_file),
                                                   1));

  context = Context();
  context->set_allowed_bytes_growth(0);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Move(context,
                                               Path(from_file),
                                               Path(obstacle_file)));
}

TEST_F(QuotaFileUtilTest, MoveDirectory) {
  const char *from_dir = "fromdir";
  const char *from_file = "fromdir/fromfile";
  const char *to_dir1 = "todir1";
  const char *to_dir2 = "todir2";
  bool created;
  FileSystemOperationContext *context;

  context = Context();
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->CreateDirectory(context,
                                                          Path(from_dir),
                                                          false, false));
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);

  context = Context();
  context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(context,
                                                   Path(from_file),
                                                   1020));

  context = Context();
  context->set_allowed_bytes_growth(1020);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Move(context,
                                               Path(from_dir),
                                               Path(to_dir1)));

  context = Context();
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->CreateDirectory(context,
                                                          Path(from_dir),
                                                          false, false));
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);

  context = Context();
  context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Truncate(context,
                                                   Path(from_file),
                                                   1020));

  context = Context();
  context->set_allowed_bytes_growth(1019);
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            QuotaFileUtil::GetInstance()->Move(context,
                                               Path(from_dir),
                                               Path(to_dir2)));
}
