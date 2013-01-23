// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/local_file_system_test_helper.h"
#include "webkit/fileapi/local_file_util.h"
#include "webkit/fileapi/native_file_util.h"

namespace fileapi {

// TODO(dmikurube): Cover all public methods in LocalFileUtil.
class LocalFileUtilTest : public testing::Test {
 public:
  LocalFileUtilTest()
      : test_helper_(GURL("http://foo/"), kFileSystemTypeTest) {}

  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    test_helper_.SetUp(data_dir_.path());
  }

  void TearDown() {
    test_helper_.TearDown();
  }

 protected:
  FileSystemOperationContext* NewContext() {
    FileSystemOperationContext* context = test_helper_.NewOperationContext();
    return context;
  }

  LocalFileUtil* FileUtil() {
    return static_cast<LocalFileUtil*>(test_helper_.file_util());
  }

  FileSystemURL Path(const std::string& file_name) {
    return test_helper_.CreateURLFromUTF8(file_name);
  }

  FilePath LocalPath(const char *file_name) {
    return test_helper_.GetLocalPathFromASCII(file_name);
  }

  bool FileExists(const char *file_name) {
    return file_util::PathExists(LocalPath(file_name)) &&
        !file_util::DirectoryExists(LocalPath(file_name));
  }

  bool DirectoryExists(const char *file_name) {
    return file_util::DirectoryExists(LocalPath(file_name));
  }

  int64 GetSize(const char *file_name) {
    base::PlatformFileInfo info;
    file_util::GetFileInfo(LocalPath(file_name), &info);
    return info.size;
  }

  base::PlatformFileError CreateFile(const char* file_name,
      base::PlatformFile* file_handle, bool* created) {
    int file_flags = base::PLATFORM_FILE_CREATE |
        base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_ASYNC;

    scoped_ptr<FileSystemOperationContext> context(NewContext());
    return FileUtil()->CreateOrOpen(
        context.get(),
        Path(file_name),
        file_flags, file_handle, created);
  }

  base::PlatformFileError EnsureFileExists(const char* file_name,
      bool* created) {
    scoped_ptr<FileSystemOperationContext> context(NewContext());
    return FileUtil()->EnsureFileExists(
        context.get(),
        Path(file_name), created);
  }

  const LocalFileSystemTestOriginHelper& test_helper() const {
    return test_helper_;
  }

 private:
  scoped_ptr<LocalFileUtil> local_file_util_;
  base::ScopedTempDir data_dir_;
  MessageLoop message_loop_;
  LocalFileSystemTestOriginHelper test_helper_;

  DISALLOW_COPY_AND_ASSIGN(LocalFileUtilTest);
};

TEST_F(LocalFileUtilTest, CreateAndClose) {
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

TEST_F(LocalFileUtilTest, EnsureFileExists) {
  const char *file_name = "foobar";
  bool created;
  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(file_name, &created));
  ASSERT_TRUE(created);

  EXPECT_TRUE(FileExists(file_name));
  EXPECT_EQ(0, GetSize(file_name));

  ASSERT_EQ(base::PLATFORM_FILE_OK, EnsureFileExists(file_name, &created));
  EXPECT_FALSE(created);
}

TEST_F(LocalFileUtilTest, TouchFile) {
  const char *file_name = "test_file";
  base::PlatformFile file_handle;
  bool created;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            CreateFile(file_name, &file_handle, &created));
  ASSERT_TRUE(created);

  scoped_ptr<FileSystemOperationContext> context(NewContext());

  base::PlatformFileInfo info;
  ASSERT_TRUE(file_util::GetFileInfo(LocalPath(file_name), &info));
  const base::Time new_accessed =
      info.last_accessed + base::TimeDelta::FromHours(10);
  const base::Time new_modified =
      info.last_modified + base::TimeDelta::FromHours(5);

  EXPECT_EQ(base::PLATFORM_FILE_OK,
            FileUtil()->Touch(context.get(), Path(file_name),
                              new_accessed, new_modified));

  ASSERT_TRUE(file_util::GetFileInfo(LocalPath(file_name), &info));
  EXPECT_EQ(new_accessed, info.last_accessed);
  EXPECT_EQ(new_modified, info.last_modified);

  EXPECT_EQ(base::PLATFORM_FILE_OK,
            FileUtil()->Close(context.get(), file_handle));
}

TEST_F(LocalFileUtilTest, TouchDirectory) {
  const char *dir_name = "test_dir";
  scoped_ptr<FileSystemOperationContext> context(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            FileUtil()->CreateDirectory(context.get(),
                                        Path(dir_name),
                                        false /* exclusive */,
                                        false /* recursive */));

  base::PlatformFileInfo info;
  ASSERT_TRUE(file_util::GetFileInfo(LocalPath(dir_name), &info));
  const base::Time new_accessed =
      info.last_accessed + base::TimeDelta::FromHours(10);
  const base::Time new_modified =
      info.last_modified + base::TimeDelta::FromHours(5);

  EXPECT_EQ(base::PLATFORM_FILE_OK,
            FileUtil()->Touch(context.get(), Path(dir_name),
                              new_accessed, new_modified));

  ASSERT_TRUE(file_util::GetFileInfo(LocalPath(dir_name), &info));
  EXPECT_EQ(new_accessed, info.last_accessed);
  EXPECT_EQ(new_modified, info.last_modified);
}

TEST_F(LocalFileUtilTest, Truncate) {
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

TEST_F(LocalFileUtilTest, CopyFile) {
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
            test_helper().SameFileUtilCopy(context.get(),
                                           Path(from_file), Path(to_file1)));

  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            test_helper().SameFileUtilCopy(context.get(),
                                           Path(from_file), Path(to_file2)));

  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));
  EXPECT_TRUE(FileExists(to_file1));
  EXPECT_EQ(1020, GetSize(to_file1));
  EXPECT_TRUE(FileExists(to_file2));
  EXPECT_EQ(1020, GetSize(to_file2));
}

TEST_F(LocalFileUtilTest, CopyDirectory) {
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
            test_helper().SameFileUtilCopy(context.get(),
                                           Path(from_dir), Path(to_dir)));

  EXPECT_TRUE(DirectoryExists(from_dir));
  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));
  EXPECT_TRUE(DirectoryExists(to_dir));
  EXPECT_TRUE(FileExists(to_file));
  EXPECT_EQ(1020, GetSize(to_file));
}

TEST_F(LocalFileUtilTest, MoveFile) {
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
            test_helper().SameFileUtilMove(context.get(),
                                           Path(from_file), Path(to_file)));

  EXPECT_FALSE(FileExists(from_file));
  EXPECT_TRUE(FileExists(to_file));
  EXPECT_EQ(1020, GetSize(to_file));
}

TEST_F(LocalFileUtilTest, MoveDirectory) {
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
            test_helper().SameFileUtilMove(context.get(),
                                           Path(from_dir), Path(to_dir)));

  EXPECT_FALSE(DirectoryExists(from_dir));
  EXPECT_TRUE(DirectoryExists(to_dir));
  EXPECT_TRUE(FileExists(to_file));
  EXPECT_EQ(1020, GetSize(to_file));
}

}  // namespace fileapi
