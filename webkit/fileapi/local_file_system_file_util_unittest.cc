// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_path.h"
#include "base/message_loop_proxy.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/platform_file.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/local_file_system_file_util.h"
#include "webkit/fileapi/quota_file_util.h"
#include "webkit/quota/quota_manager.h"

namespace fileapi {
namespace {

class MockFileSystemPathManager : public FileSystemPathManager {
 public:
  MockFileSystemPathManager(const FilePath& filesystem_path)
      : FileSystemPathManager(base::MessageLoopProxy::CreateForCurrentThread(),
                              filesystem_path, NULL, false, true),
        test_filesystem_path_(filesystem_path) {}

  virtual FilePath ValidateFileSystemRootAndGetPathOnFileThread(
      const GURL& origin_url,
      fileapi::FileSystemType type,
      const FilePath& virtual_path,
      bool create) {
    return test_filesystem_path_;
  }

 private:
  FilePath test_filesystem_path_;
};

FilePath UTF8ToFilePath(const std::string& str) {
  FilePath::StringType result;
#if defined(OS_POSIX)
  result = base::SysWideToNativeMB(UTF8ToWide(str));
#elif defined(OS_WIN)
  result = UTF8ToUTF16(str);
#endif
  return FilePath(result);
}

}  // namespace (anonymous)

// TODO(dmikurube): Cover all public methods in LocalFileSystemFileUtil.
class LocalFileSystemFileUtilTest : public testing::Test {
 public:
  LocalFileSystemFileUtilTest()
      : callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  }

  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    filesystem_dir_ = data_dir_.path().AppendASCII("filesystem");
    file_util::CreateDirectory(filesystem_dir_);
    ASSERT_TRUE(file_util::CreateTemporaryFileInDir(filesystem_dir_,
                                                    &file_path_));
  }

 protected:
  FileSystemOperationContext* NewContext() {
    FileSystemOperationContext* context = new FileSystemOperationContext(
        new FileSystemContext(base::MessageLoopProxy::CreateForCurrentThread(),
                              base::MessageLoopProxy::CreateForCurrentThread(),
                              NULL, NULL, FilePath(), false /* is_incognito */,
                              true, true,
                              new MockFileSystemPathManager(filesystem_dir_)),
        FileUtil());
    context->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
    return context;
  }

  LocalFileSystemFileUtil* FileUtil() {
    return LocalFileSystemFileUtil::GetInstance();
  }

  FilePath Path(const char *file_name) {
    return UTF8ToFilePath(file_name);
  }

  bool FileExists(const char *file_name) {
    return file_util::PathExists(filesystem_dir_.AppendASCII(file_name)) &&
        !file_util::DirectoryExists(filesystem_dir_.AppendASCII(file_name));
  }

  bool DirectoryExists(const char *file_name) {
    return file_util::DirectoryExists(filesystem_dir_.AppendASCII(file_name));
  }

  int64 GetSize(const char *file_name) {
    base::PlatformFileInfo info;
    file_util::GetFileInfo(filesystem_dir_.AppendASCII(file_name), &info);
    return info.size;
  }

  base::PlatformFileError CreateFile(const char* file_name,
      base::PlatformFile* file_handle, bool* created) {
    int file_flags = base::PLATFORM_FILE_CREATE |
        base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_ASYNC;

    scoped_ptr<FileSystemOperationContext> context(NewContext());
    return FileUtil()->CreateOrOpen(
        context.get(),
        UTF8ToFilePath(file_name),
        file_flags, file_handle, created);
  }

  base::PlatformFileError EnsureFileExists(const char* file_name,
      bool* created) {
    scoped_ptr<FileSystemOperationContext> context(NewContext());
    return FileUtil()->EnsureFileExists(
        context.get(),
        UTF8ToFilePath(file_name), created);
  }

 private:
  ScopedTempDir data_dir_;
  FilePath filesystem_dir_;
  FilePath file_path_;

  base::ScopedCallbackFactory<LocalFileSystemFileUtilTest> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(LocalFileSystemFileUtilTest);
};

TEST_F(LocalFileSystemFileUtilTest, CreateAndClose) {
  const char *file_name = "test_file";
  base::PlatformFile file_handle;
  bool created;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            CreateFile(file_name, &file_handle, &created));
  ASSERT_TRUE(created);

  EXPECT_TRUE(FileExists(file_name));
  EXPECT_EQ(0, GetSize(file_name));

  scoped_ptr<FileSystemOperationContext> context(NewContext());
  EXPECT_EQ(base::PLATFORM_FILE_OK,
      FileUtil()->Close(context.get(), file_handle));
}

TEST_F(LocalFileSystemFileUtilTest, EnsureFileExists) {
  const char *file_name = "foobar";
  bool created;
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(file_name, &created));
  ASSERT_TRUE(created);

  EXPECT_TRUE(FileExists(file_name));
  EXPECT_EQ(0, GetSize(file_name));

  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(file_name, &created));
  EXPECT_FALSE(created);
}

TEST_F(LocalFileSystemFileUtilTest, Truncate) {
  const char *file_name = "truncated";
  bool created;
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(file_name, &created));
  ASSERT_TRUE(created);

  scoped_ptr<FileSystemOperationContext> context;

  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
      FileUtil()->Truncate(context.get(), Path(file_name), 1020));

  EXPECT_TRUE(FileExists(file_name));
  EXPECT_EQ(1020, GetSize(file_name));
}

TEST_F(LocalFileSystemFileUtilTest, CopyFile) {
  const char *from_file = "fromfile";
  const char *to_file1 = "tofile1";
  const char *to_file2 = "tofile2";
  bool created;
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);
  scoped_ptr<FileSystemOperationContext> context;

  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
      FileUtil()->Truncate(context.get(), Path(from_file), 1020));

  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));

  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
      FileUtil()->Copy(context.get(), Path(from_file), Path(to_file1)));

  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
      FileUtil()->Copy(context.get(), Path(from_file), Path(to_file2)));

  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));
  EXPECT_TRUE(FileExists(to_file1));
  EXPECT_EQ(1020, GetSize(to_file1));
  EXPECT_TRUE(FileExists(to_file2));
  EXPECT_EQ(1020, GetSize(to_file2));
}

TEST_F(LocalFileSystemFileUtilTest, CopyDirectory) {
  const char *from_dir = "fromdir";
  const char *from_file = "fromdir/fromfile";
  const char *to_dir = "todir";
  const char *to_file = "todir/fromfile";
  bool created;
  scoped_ptr<FileSystemOperationContext> context;

  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
      FileUtil()->CreateDirectory(context.get(), Path(from_dir), false, false));
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);

  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
      FileUtil()->Truncate(context.get(), Path(from_file), 1020));

  EXPECT_TRUE(DirectoryExists(from_dir));
  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));
  EXPECT_FALSE(DirectoryExists(to_dir));

  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
      FileUtil()->Copy(context.get(), Path(from_dir), Path(to_dir)));

  EXPECT_TRUE(DirectoryExists(from_dir));
  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));
  EXPECT_TRUE(DirectoryExists(to_dir));
  EXPECT_TRUE(FileExists(to_file));
  EXPECT_EQ(1020, GetSize(to_file));
}

TEST_F(LocalFileSystemFileUtilTest, MoveFile) {
  const char *from_file = "fromfile";
  const char *to_file = "tofile";
  bool created;
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);
  scoped_ptr<FileSystemOperationContext> context;

  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
      FileUtil()->Truncate(context.get(), Path(from_file), 1020));

  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));

  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
      FileUtil()->Move(context.get(), Path(from_file), Path(to_file)));

  EXPECT_FALSE(FileExists(from_file));
  EXPECT_TRUE(FileExists(to_file));
  EXPECT_EQ(1020, GetSize(to_file));
}

TEST_F(LocalFileSystemFileUtilTest, MoveDirectory) {
  const char *from_dir = "fromdir";
  const char *from_file = "fromdir/fromfile";
  const char *to_dir = "todir";
  const char *to_file = "todir/fromfile";
  bool created;
  scoped_ptr<FileSystemOperationContext> context;

  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
      FileUtil()->CreateDirectory(context.get(), Path(from_dir), false, false));
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);

  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
      FileUtil()->Truncate(context.get(), Path(from_file), 1020));

  EXPECT_TRUE(DirectoryExists(from_dir));
  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));
  EXPECT_FALSE(DirectoryExists(to_dir));

  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
      FileUtil()->Move(context.get(), Path(from_dir), Path(to_dir)));

  EXPECT_FALSE(DirectoryExists(from_dir));
  EXPECT_TRUE(DirectoryExists(to_dir));
  EXPECT_TRUE(FileExists(to_file));
  EXPECT_EQ(1020, GetSize(to_file));
}

}  // namespace fileapi
