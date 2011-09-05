// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "base/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_test_helper.h"
#include "webkit/fileapi/native_file_util.h"
#include "webkit/fileapi/obfuscated_file_util.h"

using namespace fileapi;

namespace {

struct CopyMoveTestCaseRecord {
  bool is_directory;
  const FilePath::CharType path[64];
  int64 data_file_size;
};

const CopyMoveTestCaseRecord kCopyMoveTestCases[] = {
  {true, FILE_PATH_LITERAL("dir a"), 0},
  {true, FILE_PATH_LITERAL("dir a/dir a"), 0},
  {true, FILE_PATH_LITERAL("dir a/dir d"), 0},
  {true, FILE_PATH_LITERAL("dir a/dir d/dir e"), 0},
  {true, FILE_PATH_LITERAL("dir a/dir d/dir e/dir f"), 0},
  {true, FILE_PATH_LITERAL("dir a/dir d/dir e/dir g"), 0},
  {true, FILE_PATH_LITERAL("dir a/dir d/dir e/dir h"), 0},
  {true, FILE_PATH_LITERAL("dir b"), 0},
  {true, FILE_PATH_LITERAL("dir b/dir a"), 0},
  {true, FILE_PATH_LITERAL("dir c"), 0},
  {false, FILE_PATH_LITERAL("file 0"), 38},
  {false, FILE_PATH_LITERAL("file 2"), 60},
  {false, FILE_PATH_LITERAL("file 3"), 0},
  {false, FILE_PATH_LITERAL("dir a/file 0"), 39},
  {false, FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 0"), 40},
  {false, FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 1"), 41},
  {false, FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 2"), 42},
  {false, FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 3"), 50},
};

}  // namespace (anonymous)

// This is not yet a full unit test for FileSystemFileUtil.  TODO(ericu): Adapt
// the other subclasses' unit tests, as mentioned in the comments in
// ObfuscatedFileUtil's unit test.
// Currently this is just a test of cross-filesystem copy and move, which
// actually exercises subclasses of FileSystemFileUtil as well as the class
// itself.  We currently only test copies between obfuscated filesystems.
// TODO(ericu): Add a test for copying between obfuscated and local filesystems,
// and between different local filesystems.
class FileSystemFileUtilTest : public testing::Test {
 public:
  FileSystemFileUtilTest() {
  }

  void SetUp() {
  }

  FileSystemOperationContext* NewContext(FileSystemTestOriginHelper* helper) {
    FileSystemOperationContext* context = helper->NewOperationContext();
    // We need to allocate quota for paths for
    // TestCrossFileSystemCopyMoveHelper, since it calls into OFSFU, which
    // charges quota for paths.
    context->set_allowed_bytes_growth(1024 * 1024);
    return context;
  }

  void TestCrossFileSystemCopyMoveHelper(
      const GURL& src_origin, fileapi::FileSystemType src_type,
      const GURL& dest_origin, fileapi::FileSystemType dest_type,
      bool copy) {
    ScopedTempDir base_dir;
    ASSERT_TRUE(base_dir.CreateUniqueTempDir());
    scoped_refptr<ObfuscatedFileUtil> file_util(
        new ObfuscatedFileUtil(base_dir.path(), new NativeFileUtil()));
    FileSystemTestOriginHelper src_helper(src_origin, src_type);
    src_helper.SetUp(base_dir.path(),
                     false,  // incognito
                     false,  // unlimited quota
                     NULL,  // quota::QuotaManagerProxy
                     file_util.get());
    FileSystemTestOriginHelper dest_helper(dest_origin, dest_type);
    dest_helper.SetUp(src_helper.file_system_context(), NULL);

    // Set up all the source data.
    scoped_ptr<FileSystemOperationContext> context;
    FilePath test_root(FILE_PATH_LITERAL("root directory"));
    for (size_t i = 0; i < arraysize(kCopyMoveTestCases); ++i) {
      const CopyMoveTestCaseRecord& test_case = kCopyMoveTestCases[i];
      FilePath path = test_root.Append(test_case.path);
      if (test_case.is_directory) {
        context.reset(NewContext(&src_helper));
        ASSERT_EQ(base::PLATFORM_FILE_OK,
            file_util->CreateDirectory(context.get(), path, true, true));
      } else {
        context.reset(NewContext(&src_helper));
        bool created = false;
        ASSERT_EQ(base::PLATFORM_FILE_OK,
            file_util->EnsureFileExists(context.get(), path, &created));
        ASSERT_TRUE(created);
        context.reset(NewContext(&src_helper));
        ASSERT_EQ(base::PLATFORM_FILE_OK, file_util->Truncate(
            context.get(), path, test_case.data_file_size));
      }
    }

    // Do the actual copy or move.
    FileSystemContext* file_system_context = dest_helper.file_system_context();
    scoped_ptr<FileSystemOperationContext> copy_context(
        new FileSystemOperationContext(file_system_context, NULL));
    copy_context->set_src_file_util(file_util);
    copy_context->set_dest_file_util(file_util);
    copy_context->set_src_origin_url(src_helper.origin());
    copy_context->set_dest_origin_url(dest_helper.origin());
    copy_context->set_src_type(src_helper.type());
    copy_context->set_dest_type(dest_helper.type());
    copy_context->set_allowed_bytes_growth(1024 * 1024); // OFSFU path quota.

    if (copy)
      ASSERT_EQ(base::PLATFORM_FILE_OK,
          file_util->Copy(copy_context.get(), test_root, test_root));
    else
      ASSERT_EQ(base::PLATFORM_FILE_OK,
          file_util->Move(copy_context.get(), test_root, test_root));

    // Validate that the destination paths are correct.
    for (size_t i = 0; i < arraysize(kCopyMoveTestCases); ++i) {
      const CopyMoveTestCaseRecord& test_case = kCopyMoveTestCases[i];
      FilePath path = test_root.Append(test_case.path);

      base::PlatformFileInfo dest_file_info;
      FilePath data_path;
      context.reset(NewContext(&dest_helper));
      EXPECT_EQ(base::PLATFORM_FILE_OK,
          file_util->GetFileInfo(
              context.get(), path, &dest_file_info, &data_path));
      if (test_case.is_directory) {
        EXPECT_TRUE(dest_file_info.is_directory);
      } else {
        base::PlatformFileInfo platform_file_info;
        ASSERT_TRUE(file_util::GetFileInfo(data_path, &platform_file_info));
        EXPECT_EQ(test_case.data_file_size, platform_file_info.size);
        EXPECT_FALSE(platform_file_info.is_directory);
        EXPECT_EQ(platform_file_info.size, dest_file_info.size);
        EXPECT_FALSE(dest_file_info.is_directory);
      }
    }

    // Validate that the source paths are still there [for a copy] or gone [for
    // a move].
    for (size_t i = 0; i < arraysize(kCopyMoveTestCases); ++i) {
      const CopyMoveTestCaseRecord& test_case = kCopyMoveTestCases[i];
      FilePath path = test_root.Append(test_case.path);
      base::PlatformFileInfo src_file_info;
      FilePath data_path;
      context.reset(NewContext(&src_helper));
      base::PlatformFileError expected_result;
      if (copy)
        expected_result = base::PLATFORM_FILE_OK;
      else
        expected_result = base::PLATFORM_FILE_ERROR_NOT_FOUND;
      EXPECT_EQ(expected_result,
          file_util->GetFileInfo(
              context.get(), path, &src_file_info, &data_path));
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FileSystemFileUtilTest);
};

TEST_F(FileSystemFileUtilTest, TestCrossFileSystemCopyDifferentOrigins) {
  GURL src_origin("http://www.example.com");
  fileapi::FileSystemType type = kFileSystemTypePersistent;
  GURL dest_origin("http://www.not.the.same.domain.com");

  TestCrossFileSystemCopyMoveHelper(src_origin, type, dest_origin, type, true);
}

TEST_F(FileSystemFileUtilTest, TestCrossFileSystemCopySameOrigin) {
  GURL origin("http://www.example.com");
  fileapi::FileSystemType src_type = kFileSystemTypePersistent;
  fileapi::FileSystemType dest_type = kFileSystemTypeTemporary;

  TestCrossFileSystemCopyMoveHelper(origin, src_type, origin, dest_type, true);
}

TEST_F(FileSystemFileUtilTest, TestCrossFileSystemMoveDifferentOrigins) {
  GURL src_origin("http://www.example.com");
  fileapi::FileSystemType type = kFileSystemTypePersistent;
  GURL dest_origin("http://www.not.the.same.domain.com");

  TestCrossFileSystemCopyMoveHelper(src_origin, type, dest_origin, type, false);
}

TEST_F(FileSystemFileUtilTest, TestCrossFileSystemMoveSameOrigin) {
  GURL origin("http://www.example.com");
  fileapi::FileSystemType src_type = kFileSystemTypePersistent;
  fileapi::FileSystemType dest_type = kFileSystemTypeTemporary;

  TestCrossFileSystemCopyMoveHelper(origin, src_type, origin, dest_type, false);
}
