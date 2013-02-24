// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/platform_file.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/native_file_util.h"

namespace fileapi {

class NativeFileUtilTest : public testing::Test {
 public:
  NativeFileUtilTest() {}

  virtual void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
  }

 protected:
  base::FilePath Path() {
    return data_dir_.path();
  }

  base::FilePath Path(const char* file_name) {
    return data_dir_.path().AppendASCII(file_name);
  }

  bool FileExists(const base::FilePath& path) {
    return file_util::PathExists(path) &&
           !file_util::DirectoryExists(path);
  }

  int64 GetSize(const base::FilePath& path) {
    base::PlatformFileInfo info;
    file_util::GetFileInfo(path, &info);
    return info.size;
  }

 private:
  base::ScopedTempDir data_dir_;

  DISALLOW_COPY_AND_ASSIGN(NativeFileUtilTest);
};

TEST_F(NativeFileUtilTest, CreateCloseAndDeleteFile) {
  base::FilePath file_name = Path("test_file");
  base::PlatformFile file_handle;
  bool created = false;
  int flags = base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_ASYNC;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::CreateOrOpen(file_name,
                                         base::PLATFORM_FILE_CREATE | flags,
                                         &file_handle, &created));
  ASSERT_TRUE(created);

  EXPECT_TRUE(file_util::PathExists(file_name));
  EXPECT_TRUE(NativeFileUtil::PathExists(file_name));
  EXPECT_EQ(0, GetSize(file_name));
  EXPECT_NE(base::kInvalidPlatformFileValue, file_handle);

  ASSERT_EQ(base::PLATFORM_FILE_OK, NativeFileUtil::Close(file_handle));

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::CreateOrOpen(file_name,
                                         base::PLATFORM_FILE_OPEN | flags,
                                         &file_handle, &created));
  ASSERT_FALSE(created);
  ASSERT_EQ(base::PLATFORM_FILE_OK, NativeFileUtil::Close(file_handle));

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::DeleteFile(file_name));
  EXPECT_FALSE(file_util::PathExists(file_name));
  EXPECT_FALSE(NativeFileUtil::PathExists(file_name));
}

TEST_F(NativeFileUtilTest, EnsureFileExists) {
  base::FilePath file_name = Path("foobar");
  bool created = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::EnsureFileExists(file_name, &created));
  ASSERT_TRUE(created);

  EXPECT_TRUE(FileExists(file_name));
  EXPECT_EQ(0, GetSize(file_name));

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::EnsureFileExists(file_name, &created));
  EXPECT_FALSE(created);
}

TEST_F(NativeFileUtilTest, CreateAndDeleteDirectory) {
  base::FilePath dir_name = Path("test_dir");
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::CreateDirectory(dir_name,
                                            false /* exclusive */,
                                            false /* recursive */));

  EXPECT_TRUE(NativeFileUtil::DirectoryExists(dir_name));
  EXPECT_TRUE(file_util::DirectoryExists(dir_name));

  ASSERT_EQ(base::PLATFORM_FILE_ERROR_EXISTS,
            NativeFileUtil::CreateDirectory(dir_name,
                                            true /* exclusive */,
                                            false /* recursive */));

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::DeleteDirectory (dir_name));
  EXPECT_FALSE(file_util::DirectoryExists(dir_name));
  EXPECT_FALSE(NativeFileUtil::DirectoryExists(dir_name));
}

TEST_F(NativeFileUtilTest, TouchFileAndGetFileInfo) {
  base::FilePath file_name = Path("test_file");
  base::PlatformFileInfo native_info;
  ASSERT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            NativeFileUtil::GetFileInfo(file_name, &native_info));

  bool created = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::EnsureFileExists(file_name, &created));
  ASSERT_TRUE(created);

  base::PlatformFileInfo info;
  ASSERT_TRUE(file_util::GetFileInfo(file_name, &info));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::GetFileInfo(file_name, &native_info));
  ASSERT_EQ(info.size, native_info.size);
  ASSERT_EQ(info.is_directory, native_info.is_directory);
  ASSERT_EQ(info.is_symbolic_link, native_info.is_symbolic_link);
  ASSERT_EQ(info.last_modified, native_info.last_modified);
  ASSERT_EQ(info.last_accessed, native_info.last_accessed);
  ASSERT_EQ(info.creation_time, native_info.creation_time);

  const base::Time new_accessed =
      info.last_accessed + base::TimeDelta::FromHours(10);
  const base::Time new_modified =
      info.last_modified + base::TimeDelta::FromHours(5);

  EXPECT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::Touch(file_name,
                                  new_accessed, new_modified));

  ASSERT_TRUE(file_util::GetFileInfo(file_name, &info));
  EXPECT_EQ(new_accessed, info.last_accessed);
  EXPECT_EQ(new_modified, info.last_modified);
}

TEST_F(NativeFileUtilTest, CreateFileEnumerator) {
  base::FilePath path_1 = Path("dir1");
  base::FilePath path_2 = Path("file1");
  base::FilePath path_11 = Path("dir1").AppendASCII("file11");
  base::FilePath path_12 = Path("dir1").AppendASCII("dir12");
  base::FilePath path_121 =
      Path("dir1").AppendASCII("dir12").AppendASCII("file121");
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::CreateDirectory(path_1, false, false));
  bool created = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::EnsureFileExists(path_2, &created));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::EnsureFileExists(path_11, &created));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::CreateDirectory(path_12, false, false));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::EnsureFileExists(path_121, &created));

  {
    scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> enumerator =
        NativeFileUtil::CreateFileEnumerator(Path(), false);
    std::set<base::FilePath> set;
    set.insert(path_1);
    set.insert(path_2);
    for (base::FilePath path = enumerator->Next(); !path.empty();
         path = enumerator->Next())
      EXPECT_EQ(1U, set.erase(path));
    EXPECT_TRUE(set.empty());
  }

  {
    scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> enumerator =
        NativeFileUtil::CreateFileEnumerator(Path(), true);
        std::set<base::FilePath> set;
    set.insert(path_1);
    set.insert(path_2);
    set.insert(path_11);
    set.insert(path_12);
    set.insert(path_121);
    for (base::FilePath path = enumerator->Next(); !path.empty();
         path = enumerator->Next())
      EXPECT_EQ(1U, set.erase(path));
    EXPECT_TRUE(set.empty());
  }
}

TEST_F(NativeFileUtilTest, Truncate) {
  base::FilePath file_name = Path("truncated");
  bool created = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::EnsureFileExists(file_name, &created));
  ASSERT_TRUE(created);

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::Truncate(file_name, 1020));

  EXPECT_TRUE(FileExists(file_name));
  EXPECT_EQ(1020, GetSize(file_name));
}

TEST_F(NativeFileUtilTest, CopyFile) {
  base::FilePath from_file = Path("fromfile");
  base::FilePath to_file1 = Path("tofile1");
  base::FilePath to_file2 = Path("tofile2");
  bool created = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::Truncate(from_file, 1020));

  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::CopyOrMoveFile(from_file, to_file1, true));

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::CopyOrMoveFile(from_file, to_file2, true));

  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));
  EXPECT_TRUE(FileExists(to_file1));
  EXPECT_EQ(1020, GetSize(to_file1));
  EXPECT_TRUE(FileExists(to_file2));
  EXPECT_EQ(1020, GetSize(to_file2));

  base::FilePath dir = Path("dir");
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::CreateDirectory(dir, false, false));
  ASSERT_TRUE(file_util::DirectoryExists(dir));
  base::FilePath to_dir_file = dir.AppendASCII("file");
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::CopyOrMoveFile(from_file, to_dir_file, true));
  EXPECT_TRUE(FileExists(to_dir_file));
  EXPECT_EQ(1020, GetSize(to_dir_file));

  // Following tests are error checking.
  // Source doesn't exist.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            NativeFileUtil::CopyOrMoveFile(Path("nonexists"), Path("file"),
                                           true));

  // Source is not a file.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_FILE,
            NativeFileUtil::CopyOrMoveFile(dir, Path("file"), true));
  // Destination is not a file.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION,
            NativeFileUtil::CopyOrMoveFile(from_file, dir, true));
  // Destination's parent doesn't exist.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            NativeFileUtil::CopyOrMoveFile(from_file,
                                           Path("nodir").AppendASCII("file"),
                                           true));
  // Destination's parent is a file.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            NativeFileUtil::CopyOrMoveFile(from_file,
                                           Path("tofile1").AppendASCII("file"),
                                           true));
}

TEST_F(NativeFileUtilTest, MoveFile) {
  base::FilePath from_file = Path("fromfile");
  base::FilePath to_file = Path("tofile");
  bool created = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::EnsureFileExists(from_file, &created));
  ASSERT_TRUE(created);

  ASSERT_EQ(base::PLATFORM_FILE_OK, NativeFileUtil::Truncate(from_file, 1020));

  EXPECT_TRUE(FileExists(from_file));
  EXPECT_EQ(1020, GetSize(from_file));

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::CopyOrMoveFile(from_file, to_file, false));

  EXPECT_FALSE(FileExists(from_file));
  EXPECT_TRUE(FileExists(to_file));
  EXPECT_EQ(1020, GetSize(to_file));

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::EnsureFileExists(from_file, &created));
  ASSERT_TRUE(FileExists(from_file));
  ASSERT_EQ(base::PLATFORM_FILE_OK, NativeFileUtil::Truncate(from_file, 1020));

  base::FilePath dir = Path("dir");
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::CreateDirectory(dir, false, false));
  ASSERT_TRUE(file_util::DirectoryExists(dir));
  base::FilePath to_dir_file = dir.AppendASCII("file");
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::CopyOrMoveFile(from_file, to_dir_file, false));
  EXPECT_FALSE(FileExists(from_file));
  EXPECT_TRUE(FileExists(to_dir_file));
  EXPECT_EQ(1020, GetSize(to_dir_file));

  // Following is error checking.
  // Source doesn't exist.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            NativeFileUtil::CopyOrMoveFile(Path("nonexists"), Path("file"),
                                           false));

  // Source is not a file.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_FILE,
            NativeFileUtil::CopyOrMoveFile(dir, Path("file"), false));
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::EnsureFileExists(from_file, &created));
  ASSERT_TRUE(FileExists(from_file));
  // Destination is not a file.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION,
            NativeFileUtil::CopyOrMoveFile(from_file, dir, false));

  ASSERT_EQ(base::PLATFORM_FILE_OK,
            NativeFileUtil::EnsureFileExists(from_file, &created));
  ASSERT_TRUE(FileExists(from_file));
  // Destination's parent doesn't exist.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            NativeFileUtil::CopyOrMoveFile(from_file,
                                           Path("nodir").AppendASCII("file"),
                                           false));
  // Destination's parent is a file.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            NativeFileUtil::CopyOrMoveFile(from_file,
                                           Path("tofile1").AppendASCII("file"),
                                           false));
}

}  // namespace fileapi
