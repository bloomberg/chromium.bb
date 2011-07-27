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
#include "webkit/fileapi/quota_file_util.h"

namespace fileapi {

class QuotaFileUtilTest : public testing::Test {
 public:
  QuotaFileUtilTest()
      : callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  }

  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    quota_file_util_.reset(QuotaFileUtil::CreateDefault());
    quota_test_helper_.SetUp(data_dir_.path(), quota_file_util_.get());
    obfuscated_test_helper_.SetUp(
        quota_test_helper_.file_system_context(), NULL);
    base_dir_ = obfuscated_test_helper_.GetOriginRootPath();
  }

  void TearDown() {
    quota_test_helper_.TearDown();
    obfuscated_test_helper_.TearDown();
  }

 protected:
  FileSystemOperationContext* NewContext(
      int64 allowed_bytes_growth,
      const FilePath& src_virtual_path,
      const FilePath& dest_virtual_path) {
    FileSystemOperationContext* context =
        quota_test_helper_.NewOperationContext();
    context->set_allowed_bytes_growth(allowed_bytes_growth);
    context->set_src_virtual_path(src_virtual_path);
    context->set_dest_virtual_path(dest_virtual_path);
    return context;
  }

  FilePath Path(const std::string& file_name) {
    return base_dir_.AppendASCII(file_name);
  }

  int64 ComputeFilePathCost(const char* file_name) {
    return quota_file_util_->ComputeFilePathCost(
        FilePath().AppendASCII(file_name));
  }

  base::PlatformFileError EnsureFileExists(
      const char* file_name, bool* created) {
    int64 file_path_cost = ComputeFilePathCost(file_name);
    scoped_ptr<FileSystemOperationContext> context(NewContext(
        file_path_cost, Path(file_name), FilePath()));
    return quota_file_util_->EnsureFileExists(
        context.get(), Path(file_name), created);
  }

  base::PlatformFileError Truncate(
      const char* file_name, int64 size, int64 quota) {
    scoped_ptr<FileSystemOperationContext> context(NewContext(
        quota, Path(file_name), FilePath()));
    return quota_file_util_->Truncate(
        context.get(), Path(file_name), size);
  }

  void CheckUsage(int64 estimated_content, int64 estimated_path) {
    ASSERT_EQ(estimated_content + estimated_path,
              quota_test_helper().GetCachedOriginUsage());
    ASSERT_EQ(quota_test_helper().ComputeCurrentOriginUsage() +
                  estimated_path,
              quota_test_helper().GetCachedOriginUsage());
  }

  QuotaFileUtil* quota_file_util() const {
    return quota_file_util_.get();
  }

  const FileSystemTestOriginHelper& quota_test_helper() const {
    return quota_test_helper_;
  }

 private:
  ScopedTempDir data_dir_;
  FilePath base_dir_;
  FileSystemTestOriginHelper obfuscated_test_helper_;
  FileSystemTestOriginHelper quota_test_helper_;
  scoped_ptr<QuotaFileUtil> quota_file_util_;
  base::ScopedCallbackFactory<QuotaFileUtilTest> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuotaFileUtilTest);
};

TEST_F(QuotaFileUtilTest, CreateAndClose) {
  const char *file_name = "test_file";
  int64 file_path_cost = ComputeFilePathCost(file_name);

  int file_flags = base::PLATFORM_FILE_CREATE |
      base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_ASYNC;
  base::PlatformFile file_handle;
  bool created;
  scoped_ptr<FileSystemOperationContext> context;

  created = false;
  context.reset(NewContext(file_path_cost - 1, Path(file_name), FilePath()));
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, quota_file_util()->CreateOrOpen(
      context.get(), Path(file_name), file_flags, &file_handle, &created));
  ASSERT_FALSE(created);

  created = false;
  context.reset(NewContext(file_path_cost, Path(file_name), FilePath()));
  ASSERT_EQ(base::PLATFORM_FILE_OK, quota_file_util()->CreateOrOpen(
      context.get(), Path(file_name), file_flags, &file_handle, &created));
  ASSERT_TRUE(created);

  context.reset(NewContext(0, FilePath(), FilePath()));
  EXPECT_EQ(base::PLATFORM_FILE_OK, quota_file_util()->Close(
      context.get(), file_handle));
}

TEST_F(QuotaFileUtilTest, EnsureFileExists) {
  const char *file_name = "foobar";

  bool created;
  scoped_ptr<FileSystemOperationContext> context;

  created = false;
  context.reset(NewContext(
      ComputeFilePathCost(file_name) - 1, Path(file_name), FilePath()));
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
      quota_file_util()->EnsureFileExists(
          context.get(), Path(file_name), &created));
  ASSERT_FALSE(created);

  created = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(file_name, &created));
  ASSERT_TRUE(created);

  created = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(file_name, &created));
  ASSERT_FALSE(created);
}

TEST_F(QuotaFileUtilTest, Truncate) {
  const char *file_name = "truncated";
  bool created;

  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(file_name, &created));
  ASSERT_TRUE(created);

  scoped_ptr<FileSystemOperationContext> context;

  ASSERT_EQ(base::PLATFORM_FILE_OK, Truncate(file_name, 1020, 1020));
  CheckUsage(1020, ComputeFilePathCost(file_name));

  ASSERT_EQ(base::PLATFORM_FILE_OK, Truncate(file_name, 0, 0));
  CheckUsage(0, ComputeFilePathCost(file_name));

  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            Truncate(file_name, 1021, 1020));
  CheckUsage(0, ComputeFilePathCost(file_name));
}

TEST_F(QuotaFileUtilTest, CopyFile) {
  int64 file_path_cost = 0;
  const char *from_file = "fromfile";
  const char *prior_file = "obstaclefile";
  const char *to_file1 = "tofile1";
  const char *to_file2 = "tomuchlongerfile2";
  bool created;
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);
  file_path_cost += ComputeFilePathCost(from_file);
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(prior_file, &created));
  ASSERT_TRUE(created);
  file_path_cost += ComputeFilePathCost(prior_file);
  scoped_ptr<FileSystemOperationContext> context;

  ASSERT_EQ(base::PLATFORM_FILE_OK, Truncate(from_file, 1020, 1020));
  CheckUsage(1020, file_path_cost);

  ASSERT_EQ(base::PLATFORM_FILE_OK, Truncate(prior_file, 1, 1));
  CheckUsage(1021, file_path_cost);

  context.reset(NewContext(1020 + ComputeFilePathCost(to_file1),
                           Path(from_file), Path(to_file1)));
  ASSERT_EQ(base::PLATFORM_FILE_OK, quota_file_util()->Copy(
      context.get(), Path(from_file), Path(to_file1)));
  file_path_cost += ComputeFilePathCost(to_file1);
  CheckUsage(2041, file_path_cost);

  context.reset(NewContext(1020 + ComputeFilePathCost(to_file2) - 1,
                           Path(from_file), Path(to_file2)));
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            quota_file_util()->Copy(context.get(),
                                    Path(from_file),
                                    Path(to_file2)));
  CheckUsage(2041, file_path_cost);

  context.reset(NewContext(1019, Path(from_file), Path(prior_file)));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            quota_file_util()->Copy(context.get(),
                                    Path(from_file),
                                    Path(prior_file)));
  CheckUsage(3060, file_path_cost);
}

TEST_F(QuotaFileUtilTest, CopyDirectory) {
  int64 file_path_cost = 0;
  const char *from_dir = "fromdir";
  const char *from_file1 = "fromdir/fromfile1";
  const char *from_file2 = "fromdir/fromlongerfile2";
  const char *to_dir1 = "todir1";
  const char *to_dir2 = "tolongerdir2";
  bool created;
  scoped_ptr<FileSystemOperationContext> context;

  context.reset(NewContext(ComputeFilePathCost(from_dir),
                           Path(from_dir), FilePath()));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            quota_file_util()->CreateDirectory(context.get(),
                                                          Path(from_dir),
                                                          false, false));
  file_path_cost += ComputeFilePathCost(from_dir);

  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file1, &created));
  ASSERT_TRUE(created);
  file_path_cost += ComputeFilePathCost(from_file1);
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file2, &created));
  ASSERT_TRUE(created);
  file_path_cost += ComputeFilePathCost(from_file2);

  ASSERT_EQ(base::PLATFORM_FILE_OK, Truncate(from_file1, 520, 520));
  CheckUsage(520, file_path_cost);

  ASSERT_EQ(base::PLATFORM_FILE_OK, Truncate(from_file2, 500, 500));
  CheckUsage(1020, file_path_cost);

  context.reset(NewContext(1020 +
                           ComputeFilePathCost(to_dir1) +
                           ComputeFilePathCost(from_file1) +
                           ComputeFilePathCost(from_file2),
                           Path(from_dir), Path(to_dir1)));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            quota_file_util()->Copy(context.get(),
                                    Path(from_dir),
                                    Path(to_dir1)));
  file_path_cost += ComputeFilePathCost(to_dir1) +
                    ComputeFilePathCost(from_file1) +
                    ComputeFilePathCost(from_file2);
  CheckUsage(2040, file_path_cost);

  context.reset(NewContext(1020 +
                           ComputeFilePathCost(to_dir2) +
                           ComputeFilePathCost(from_file1) +
                           ComputeFilePathCost(from_file2) - 1,
                           Path(from_dir), Path(to_dir2)));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            quota_file_util()->Copy(context.get(),
                                    Path(from_dir),
                                    Path(to_dir2)));
  int64 file_path_cost1 = file_path_cost +
      ComputeFilePathCost(to_dir2) + ComputeFilePathCost(from_file1);
  int64 file_path_cost2 = file_path_cost +
      ComputeFilePathCost(to_dir2) + ComputeFilePathCost(from_file2);
  ASSERT_TRUE((2560 + file_path_cost1) ==
                  quota_test_helper().GetCachedOriginUsage() ||
              (2540 + file_path_cost2) ==
                  quota_test_helper().GetCachedOriginUsage());
  ASSERT_TRUE((quota_test_helper().ComputeCurrentOriginUsage() + file_path_cost1
                   == quota_test_helper().GetCachedOriginUsage()) ||
              (quota_test_helper().ComputeCurrentOriginUsage() + file_path_cost2
                   == quota_test_helper().GetCachedOriginUsage()));
}

TEST_F(QuotaFileUtilTest, MoveFile) {
  int64 file_path_cost = 0;
  const char *from_file = "fromfile";
  const char *prior_file = "obstaclelongnamefile";
  const char *to_file = "tofile";
  bool created;
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);
  file_path_cost += ComputeFilePathCost(from_file);
  scoped_ptr<FileSystemOperationContext> context;

  ASSERT_EQ(base::PLATFORM_FILE_OK, Truncate(from_file, 1020, 1020));
  CheckUsage(1020, file_path_cost);

  context.reset(NewContext(0, Path(from_file), Path(to_file)));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            quota_file_util()->Move(context.get(),
                                    Path(from_file),
                                    Path(to_file)));
  file_path_cost -= ComputeFilePathCost(from_file);
  file_path_cost += ComputeFilePathCost(to_file);
  CheckUsage(1020, file_path_cost);

  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);
  file_path_cost += ComputeFilePathCost(from_file);
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(prior_file, &created));
  ASSERT_TRUE(created);
  file_path_cost += ComputeFilePathCost(prior_file);

  ASSERT_EQ(base::PLATFORM_FILE_OK, Truncate(from_file, 1020, 1020));
  CheckUsage(2040, file_path_cost);

  ASSERT_EQ(base::PLATFORM_FILE_OK, Truncate(prior_file, 1, 1));
  CheckUsage(2041, file_path_cost);

  context.reset(NewContext(ComputeFilePathCost(prior_file) -
                               ComputeFilePathCost(from_file) - 1 - 1,
                           Path(from_file), Path(prior_file)));
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            quota_file_util()->Move(context.get(),
                                    Path(from_file),
                                    Path(prior_file)));
  CheckUsage(2041, file_path_cost);

  context.reset(NewContext(ComputeFilePathCost(prior_file) -
                               ComputeFilePathCost(from_file) - 1,
                           Path(from_file), Path(prior_file)));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            quota_file_util()->Move(context.get(),
                                    Path(from_file),
                                    Path(prior_file)));
  file_path_cost -= ComputeFilePathCost(from_file);
  file_path_cost += ComputeFilePathCost(prior_file);
  CheckUsage(2040, file_path_cost);
}

TEST_F(QuotaFileUtilTest, MoveDirectory) {
  int64 file_path_cost = 0;
  const char *from_dir = "fromdir";
  const char *from_file = "fromdir/fromfile";
  const char *to_dir1 = "todir1";
  const char *to_dir2 = "tolongnamedir2";
  bool created;
  scoped_ptr<FileSystemOperationContext> context;

  context.reset(NewContext(QuotaFileUtil::kNoLimit,
                           Path(from_dir), FilePath()));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            quota_file_util()->CreateDirectory(context.get(),
                                               Path(from_dir),
                                               false, false));
  file_path_cost += ComputeFilePathCost(from_dir);
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);
  file_path_cost += ComputeFilePathCost(from_file);

  ASSERT_EQ(base::PLATFORM_FILE_OK, Truncate(from_file, 1020, 1020));
  CheckUsage(1020, file_path_cost);

  context.reset(NewContext(1020, Path(from_dir), Path(to_dir1)));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            quota_file_util()->Move(context.get(),
                                    Path(from_dir),
                                    Path(to_dir1)));
  file_path_cost -= ComputeFilePathCost(from_dir);
  file_path_cost += ComputeFilePathCost(to_dir1);
  CheckUsage(1020, file_path_cost);

  context.reset(NewContext(QuotaFileUtil::kNoLimit,
                           Path(from_dir), FilePath()));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            quota_file_util()->CreateDirectory(context.get(),
                                               Path(from_dir),
                                               false, false));
  file_path_cost += ComputeFilePathCost(from_dir);
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);
  file_path_cost += ComputeFilePathCost(from_file);

  ASSERT_EQ(base::PLATFORM_FILE_OK, Truncate(from_file, 1020, 1020));
  CheckUsage(2040, file_path_cost);

  context.reset(NewContext(1019, Path(from_dir), Path(to_dir2)));
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            quota_file_util()->Move(context.get(),
                                    Path(from_dir),
                                    Path(to_dir2)));
  file_path_cost -= ComputeFilePathCost(from_dir);
  file_path_cost += ComputeFilePathCost(to_dir2);
  CheckUsage(2040, file_path_cost);
}

TEST_F(QuotaFileUtilTest, Remove) {
  int64 file_path_cost = 0;
  const char *dir = "dir";
  const char *file = "file";
  const char *dfile1 = "dir/dfile1";
  const char *dfile2 = "dir/dfile2";
  bool created;
  scoped_ptr<FileSystemOperationContext> context;

  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(file, &created));
  ASSERT_TRUE(created);
  file_path_cost += ComputeFilePathCost(file);
  context.reset(NewContext(QuotaFileUtil::kNoLimit, Path(dir), FilePath()));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            quota_file_util()->CreateDirectory(context.get(),
                                               Path(dir),
                                               false, false));
  file_path_cost += ComputeFilePathCost(dir);
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(dfile1, &created));
  ASSERT_TRUE(created);
  file_path_cost += ComputeFilePathCost(dfile1);
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(dfile2, &created));
  ASSERT_TRUE(created);
  file_path_cost += ComputeFilePathCost(dfile2);

  ASSERT_EQ(base::PLATFORM_FILE_OK, Truncate(file, 340, 340));
  CheckUsage(340, file_path_cost);

  ASSERT_EQ(base::PLATFORM_FILE_OK, Truncate(dfile1, 1020, 1020));
  CheckUsage(1360, file_path_cost);

  ASSERT_EQ(base::PLATFORM_FILE_OK, Truncate(dfile2, 120, 120));
  CheckUsage(1480, file_path_cost);

  context.reset(NewContext(QuotaFileUtil::kNoLimit, Path(file), FilePath()));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            quota_file_util()->Delete(context.get(),
                                      Path(file),
                                      false));
  file_path_cost -= ComputeFilePathCost(file);
  CheckUsage(1140, file_path_cost);

  context.reset(NewContext(QuotaFileUtil::kNoLimit, Path(dir), FilePath()));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            quota_file_util()->Delete(context.get(),
                                      Path(dir),
                                      true));
  file_path_cost = 0;
  CheckUsage(0, 0);
}

}  // namespace fileapi
