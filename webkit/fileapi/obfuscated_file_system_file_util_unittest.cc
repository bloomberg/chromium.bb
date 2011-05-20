// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "base/scoped_temp_dir.h"
#include "base/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/obfuscated_file_system_file_util.h"

using namespace fileapi;

namespace {

FilePath UTF8ToFilePath(const std::string& str) {
  FilePath::StringType result;
#if defined(OS_POSIX)
  result = str;
#elif defined(OS_WIN)
  result = base::SysUTF8ToWide(str);
#endif
  return FilePath(result);
}

bool FileExists(const FilePath& path) {
  return file_util::PathExists(path) && !file_util::DirectoryExists(path);
}

// After a move, the dest exists and the source doesn't.
// After a copy, both source and dest exist.
struct CopyMoveTestCaseRecord {
  bool is_copy_not_move;
  const char source_path[64];
  const char dest_path[64];
  bool cause_overwrite;
};

const CopyMoveTestCaseRecord kCopyMoveTestCases[] = {
  // This is the combinatoric set of:
  //  rename vs. same-name
  //  different directory vs. same directory
  //  overwrite vs. no-overwrite
  //  copy vs. move
  //  We can never be called with source and destination paths identical, so
  //  those cases are omitted.
  {true, "dir0/file0", "dir0/file1", false},
  {false, "dir0/file0", "dir0/file1", false},
  {true, "dir0/file0", "dir0/file1", true},
  {false, "dir0/file0", "dir0/file1", true},

  {true, "dir0/file0", "dir1/file0", false},
  {false, "dir0/file0", "dir1/file0", false},
  {true, "dir0/file0", "dir1/file0", true},
  {false, "dir0/file0", "dir1/file0", true},
  {true, "dir0/file0", "dir1/file1", false},
  {false, "dir0/file0", "dir1/file1", false},
  {true, "dir0/file0", "dir1/file1", true},
  {false, "dir0/file0", "dir1/file1", true},
};

struct MigrationTestCaseRecord {
  bool is_directory;
  const FilePath::CharType path[64];
  int64 data_file_size;
};

const MigrationTestCaseRecord kMigrationTestCases[] = {
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

// TODO(ericu): The vast majority of this and the other FSFU subclass tests
// could theoretically be shared.  It would basically be a FSFU interface
// compliance test, and only the subclass-specific bits that look into the
// implementation would need to be written per-subclass.
class ObfuscatedFileSystemFileUtilTest : public testing::Test {
 public:
  ObfuscatedFileSystemFileUtilTest() {
  }

  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());

    obfuscated_file_system_file_util_ =
        new ObfuscatedFileSystemFileUtil(data_dir_.path());
  }

  FileSystemOperationContext* NewContext() {
    FileSystemOperationContext *context =
        new FileSystemOperationContext(NULL, NULL);
    context->set_src_origin_url(GURL("http://example.com"));
    context->set_dest_origin_url(GURL("http://example.com"));
    context->set_src_type(kFileSystemTypeTemporary);
    context->set_dest_type(kFileSystemTypeTemporary);
    context->set_allowed_bytes_growth(1024 * 1024);

    return context;
  }

  ObfuscatedFileSystemFileUtil* ofsfu() {
    return obfuscated_file_system_file_util_.get();
  }

  const FilePath& test_directory() const {
    return data_dir_.path();
  }

  int64 GetSize(const FilePath& path) {
    int64 size;
    EXPECT_TRUE(file_util::GetFileSize(path, &size));
    return size;
  }

  void CheckFileAndCloseHandle(
      const FilePath& virtual_path, PlatformFile file_handle) {
    scoped_ptr<FileSystemOperationContext> context(NewContext());
    FilePath local_path;
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->GetLocalFilePath(
        context.get(), virtual_path, &local_path));

    base::PlatformFileInfo file_info0;
    FilePath data_path;
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->GetFileInfo(
        context.get(), virtual_path, &file_info0, &data_path));
    EXPECT_EQ(data_path, local_path);
    EXPECT_TRUE(FileExists(data_path));
    EXPECT_EQ(0, GetSize(data_path));

    const char data[] = "test data";
    const int length = arraysize(data) - 1;

    if (base::kInvalidPlatformFileValue == file_handle) {
      bool created = true;
      PlatformFileError error;
      file_handle = base::CreatePlatformFile(
          data_path,
          base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE,
          &created,
          &error);
      ASSERT_EQ(base::PLATFORM_FILE_OK, error);
      EXPECT_FALSE(created);
    }
    ASSERT_EQ(length, base::WritePlatformFile(file_handle, 0, data, length));
    EXPECT_TRUE(base::ClosePlatformFile(file_handle));

    base::PlatformFileInfo file_info1;
    EXPECT_EQ(length, GetSize(data_path));
    context.reset(NewContext());
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->GetFileInfo(
        context.get(), virtual_path, &file_info1, &data_path));
    EXPECT_EQ(data_path, local_path);

    EXPECT_FALSE(file_info0.is_directory);
    EXPECT_FALSE(file_info1.is_directory);
    EXPECT_FALSE(file_info0.is_symbolic_link);
    EXPECT_FALSE(file_info1.is_symbolic_link);
    EXPECT_EQ(0, file_info0.size);
    EXPECT_EQ(length, file_info1.size);
    EXPECT_LE(file_info0.last_accessed, file_info1.last_accessed);
    EXPECT_LE(file_info0.last_modified, file_info1.last_modified);
    EXPECT_EQ(file_info0.creation_time, file_info1.creation_time);

    context.reset(NewContext());
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->Truncate(
        context.get(), virtual_path, length * 2));
    EXPECT_EQ(length * 2, GetSize(data_path));

    context.reset(NewContext());
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->Truncate(
        context.get(), virtual_path, 1));
    EXPECT_EQ(1, GetSize(data_path));
  }

  void ValidateTestDirectory(
      const FilePath& root_path,
      const std::set<FilePath::StringType>& files,
      const std::set<FilePath::StringType>& directories) {
    scoped_ptr<FileSystemOperationContext> context;
    std::set<FilePath::StringType>::const_iterator iter;
    for (iter = files.begin(); iter != files.end(); ++iter) {
      bool created = true;
      context.reset(NewContext());
      ASSERT_EQ(base::PLATFORM_FILE_OK,
          ofsfu()->EnsureFileExists(
              context.get(), root_path.Append(*iter),
              &created));
      ASSERT_FALSE(created);
    }
    for (iter = directories.begin(); iter != directories.end(); ++iter) {
      context.reset(NewContext());
      EXPECT_TRUE(ofsfu()->DirectoryExists(context.get(),
          root_path.Append(*iter)));
    }
  }

  void FillTestDirectory(
      const FilePath& root_path,
      std::set<FilePath::StringType>* files,
      std::set<FilePath::StringType>* directories) {
    scoped_ptr<FileSystemOperationContext> context;
    context.reset(NewContext());
    std::vector<base::FileUtilProxy::Entry> entries;
    EXPECT_EQ(base::PLATFORM_FILE_OK,
        ofsfu()->ReadDirectory(context.get(), root_path, &entries));
    EXPECT_EQ(0UL, entries.size());

    files->clear();
    files->insert(FILE_PATH_LITERAL("first"));
    files->insert(FILE_PATH_LITERAL("second"));
    files->insert(FILE_PATH_LITERAL("third"));
    directories->clear();
    directories->insert(FILE_PATH_LITERAL("fourth"));
    directories->insert(FILE_PATH_LITERAL("fifth"));
    directories->insert(FILE_PATH_LITERAL("sixth"));
    std::set<FilePath::StringType>::iterator iter;
    for (iter = files->begin(); iter != files->end(); ++iter) {
      bool created = false;
      context.reset(NewContext());
      ASSERT_EQ(base::PLATFORM_FILE_OK,
          ofsfu()->EnsureFileExists(
              context.get(), root_path.Append(*iter), &created));
      ASSERT_TRUE(created);
    }
    for (iter = directories->begin(); iter != directories->end(); ++iter) {
      bool exclusive = true;
      bool recursive = false;
      context.reset(NewContext());
      EXPECT_EQ(base::PLATFORM_FILE_OK,
          ofsfu()->CreateDirectory(
              context.get(), root_path.Append(*iter), exclusive, recursive));
    }
    ValidateTestDirectory(root_path, *files, *directories);
  }

  void TestReadDirectoryHelper(const FilePath& root_path) {
    std::set<FilePath::StringType> files;
    std::set<FilePath::StringType> directories;
    FillTestDirectory(root_path, &files, &directories);

    scoped_ptr<FileSystemOperationContext> context;
    std::vector<base::FileUtilProxy::Entry> entries;
    context.reset(NewContext());
    EXPECT_EQ(base::PLATFORM_FILE_OK,
        ofsfu()->ReadDirectory(context.get(), root_path, &entries));
    std::vector<base::FileUtilProxy::Entry>::iterator entry_iter;
    EXPECT_EQ(files.size() + directories.size(), entries.size());
    for (entry_iter = entries.begin(); entry_iter != entries.end();
        ++entry_iter) {
      const base::FileUtilProxy::Entry& entry = *entry_iter;
      std::set<FilePath::StringType>::iterator iter = files.find(entry.name);
      if (iter != files.end()) {
        EXPECT_FALSE(entry.is_directory);
        files.erase(iter);
        continue;
      }
      iter = directories.find(entry.name);
      EXPECT_FALSE(directories.end() == iter);
      EXPECT_TRUE(entry.is_directory);
      directories.erase(iter);
    }
  }

  void TestTouchHelper(const FilePath& path) {
    base::Time last_access_time = base::Time::Now();  // Ignored, so not tested.
    base::Time last_modified_time = base::Time::Now();
    scoped_ptr<FileSystemOperationContext> context(NewContext());
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofsfu()->Touch(
                  context.get(), path, last_access_time, last_modified_time));
    FilePath local_path;
    base::PlatformFileInfo file_info;
    context.reset(NewContext());
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->GetFileInfo(
        context.get(), path, &file_info, &local_path));
    // We compare as time_t here to lower our resolution, to avoid false
    // negatives caused by conversion to the local filesystem's native
    // representation and back.
    EXPECT_EQ(file_info.last_modified.ToTimeT(), last_modified_time.ToTimeT());

    context.reset(NewContext());
    last_modified_time += base::TimeDelta::FromHours(1);
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofsfu()->Touch(
                  context.get(), path, last_access_time, last_modified_time));
    context.reset(NewContext());
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->GetFileInfo(
        context.get(), path, &file_info, &local_path));
    EXPECT_EQ(file_info.last_modified.ToTimeT(), last_modified_time.ToTimeT());
  }

 private:
  ScopedTempDir data_dir_;
  scoped_refptr<ObfuscatedFileSystemFileUtil> obfuscated_file_system_file_util_;

  DISALLOW_COPY_AND_ASSIGN(ObfuscatedFileSystemFileUtilTest);
};

TEST_F(ObfuscatedFileSystemFileUtilTest, TestCreateAndDeleteFile) {
  base::PlatformFile file_handle = base::kInvalidPlatformFileValue;
  bool created;
  FilePath path = UTF8ToFilePath("fake/file");
  scoped_ptr<FileSystemOperationContext> context(NewContext());
  int file_flags = base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE;

  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            ofsfu()->CreateOrOpen(
                context.get(), path, file_flags, &file_handle,
                &created));

  context.reset(NewContext());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            ofsfu()->DeleteFile(context.get(), path));

  path = UTF8ToFilePath("test file");

  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofsfu()->CreateOrOpen(
                context.get(), path, file_flags, &file_handle, &created));
  ASSERT_TRUE(created);
  EXPECT_NE(base::kInvalidPlatformFileValue, file_handle);

  CheckFileAndCloseHandle(path, file_handle);

  context.reset(NewContext());
  FilePath local_path;
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->GetLocalFilePath(
      context.get(), path, &local_path));
  EXPECT_TRUE(file_util::PathExists(local_path));

  context.reset(NewContext());
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofsfu()->DeleteFile(context.get(), path));
  EXPECT_FALSE(file_util::PathExists(local_path));

  context.reset(NewContext());
  bool exclusive = true;
  bool recursive = true;
  FilePath directory_path = UTF8ToFilePath("series/of/directories");
  path = directory_path.AppendASCII("file name");
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->CreateDirectory(
      context.get(), directory_path, exclusive, recursive));

  context.reset(NewContext());
  file_handle = base::kInvalidPlatformFileValue;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofsfu()->CreateOrOpen(
                context.get(), path, file_flags, &file_handle, &created));
  ASSERT_TRUE(created);
  EXPECT_NE(base::kInvalidPlatformFileValue, file_handle);

  CheckFileAndCloseHandle(path, file_handle);

  context.reset(NewContext());
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->GetLocalFilePath(
      context.get(), path, &local_path));
  EXPECT_TRUE(file_util::PathExists(local_path));

  context.reset(NewContext());
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            ofsfu()->DeleteFile(context.get(), path));
  EXPECT_FALSE(file_util::PathExists(local_path));
}

TEST_F(ObfuscatedFileSystemFileUtilTest, TestTruncate) {
  bool created = false;
  FilePath path = UTF8ToFilePath("file");
  scoped_ptr<FileSystemOperationContext> context(NewContext());

  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            ofsfu()->Truncate(context.get(), path, 4));

  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
      ofsfu()->EnsureFileExists(context.get(), path, &created));
  ASSERT_TRUE(created);

  context.reset(NewContext());
  FilePath local_path;
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->GetLocalFilePath(
      context.get(), path, &local_path));
  EXPECT_EQ(0, GetSize(local_path));

  context.reset(NewContext());
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->Truncate(
      context.get(), path, 10));
  EXPECT_EQ(10, GetSize(local_path));

  context.reset(NewContext());
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->Truncate(
      context.get(), path, 1));
  EXPECT_EQ(1, GetSize(local_path));

  context.reset(NewContext());
  EXPECT_FALSE(ofsfu()->DirectoryExists(context.get(), path));
  context.reset(NewContext());
  EXPECT_TRUE(ofsfu()->PathExists(context.get(), path));
}

TEST_F(ObfuscatedFileSystemFileUtilTest, TestEnsureFileExists) {
  FilePath path = UTF8ToFilePath("fake/file");
  bool created = false;
  scoped_ptr<FileSystemOperationContext> context(NewContext());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            ofsfu()->EnsureFileExists(
                context.get(), path, &created));

  context.reset(NewContext());
  path = UTF8ToFilePath("test file");
  created = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofsfu()->EnsureFileExists(context.get(), path, &created));
  ASSERT_TRUE(created);

  CheckFileAndCloseHandle(path, base::kInvalidPlatformFileValue);

  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofsfu()->EnsureFileExists(context.get(), path, &created));
  ASSERT_FALSE(created);

  // Also test in a subdirectory.
  path = UTF8ToFilePath("path/to/file.txt");
  context.reset(NewContext());
  bool exclusive = true;
  bool recursive = true;
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->CreateDirectory(
      context.get(), path.DirName(), exclusive, recursive));

  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ofsfu()->EnsureFileExists(context.get(), path, &created));
  ASSERT_TRUE(created);
  context.reset(NewContext());
  EXPECT_FALSE(ofsfu()->DirectoryExists(context.get(), path));
  context.reset(NewContext());
  EXPECT_TRUE(ofsfu()->PathExists(context.get(), path));
}

TEST_F(ObfuscatedFileSystemFileUtilTest, TestDirectoryOps) {
  scoped_ptr<FileSystemOperationContext> context(NewContext());

  bool exclusive = false;
  bool recursive = false;
  FilePath path = UTF8ToFilePath("foo/bar");
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, ofsfu()->CreateDirectory(
      context.get(), path, exclusive, recursive));

  context.reset(NewContext());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
      ofsfu()->DeleteSingleDirectory(context.get(), path));

  FilePath root = UTF8ToFilePath("");
  context.reset(NewContext());
  EXPECT_FALSE(ofsfu()->DirectoryExists(context.get(), path));
  context.reset(NewContext());
  EXPECT_FALSE(ofsfu()->PathExists(context.get(), path));
  context.reset(NewContext());
  EXPECT_TRUE(ofsfu()->IsDirectoryEmpty(context.get(), root));

  context.reset(NewContext());
  exclusive = false;
  recursive = true;
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->CreateDirectory(
      context.get(), path, exclusive, recursive));

  context.reset(NewContext());
  EXPECT_TRUE(ofsfu()->DirectoryExists(context.get(), path));
  context.reset(NewContext());
  EXPECT_TRUE(ofsfu()->PathExists(context.get(), path));
  context.reset(NewContext());
  EXPECT_FALSE(ofsfu()->IsDirectoryEmpty(context.get(), root));
  context.reset(NewContext());
  EXPECT_TRUE(ofsfu()->DirectoryExists(context.get(), path.DirName()));
  context.reset(NewContext());
  EXPECT_FALSE(ofsfu()->IsDirectoryEmpty(context.get(), path.DirName()));

  // Can't remove a non-empty directory.
  context.reset(NewContext());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_EMPTY,
      ofsfu()->DeleteSingleDirectory(context.get(), path.DirName()));

  base::PlatformFileInfo file_info;
  FilePath local_path;
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->GetFileInfo(
      context.get(), path, &file_info, &local_path));
  EXPECT_TRUE(local_path.empty());
  EXPECT_TRUE(file_info.is_directory);
  EXPECT_FALSE(file_info.is_symbolic_link);

  // Same create again should succeed, since exclusive is false.
  context.reset(NewContext());
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->CreateDirectory(
      context.get(), path, exclusive, recursive));

  exclusive = true;
  recursive = true;
  context.reset(NewContext());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, ofsfu()->CreateDirectory(
      context.get(), path, exclusive, recursive));

  context.reset(NewContext());
  EXPECT_EQ(base::PLATFORM_FILE_OK,
      ofsfu()->DeleteSingleDirectory(context.get(), path));

  path = UTF8ToFilePath("foo/bop");

  context.reset(NewContext());
  EXPECT_FALSE(ofsfu()->DirectoryExists(context.get(), path));
  context.reset(NewContext());
  EXPECT_FALSE(ofsfu()->PathExists(context.get(), path));
  context.reset(NewContext());
  EXPECT_TRUE(ofsfu()->IsDirectoryEmpty(context.get(), path));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, ofsfu()->GetFileInfo(
      context.get(), path, &file_info, &local_path));

  exclusive = true;
  recursive = false;
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->CreateDirectory(
      context.get(), path, exclusive, recursive));

  context.reset(NewContext());
  EXPECT_TRUE(ofsfu()->DirectoryExists(context.get(), path));
  context.reset(NewContext());
  EXPECT_TRUE(ofsfu()->PathExists(context.get(), path));

  exclusive = true;
  recursive = false;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, ofsfu()->CreateDirectory(
      context.get(), path, exclusive, recursive));

  exclusive = true;
  recursive = false;
  path = UTF8ToFilePath("foo");
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, ofsfu()->CreateDirectory(
      context.get(), path, exclusive, recursive));

  path = UTF8ToFilePath("blah");

  context.reset(NewContext());
  EXPECT_FALSE(ofsfu()->DirectoryExists(context.get(), path));
  context.reset(NewContext());
  EXPECT_FALSE(ofsfu()->PathExists(context.get(), path));

  exclusive = true;
  recursive = false;
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->CreateDirectory(
      context.get(), path, exclusive, recursive));

  context.reset(NewContext());
  EXPECT_TRUE(ofsfu()->DirectoryExists(context.get(), path));
  context.reset(NewContext());
  EXPECT_TRUE(ofsfu()->PathExists(context.get(), path));

  exclusive = true;
  recursive = false;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_EXISTS, ofsfu()->CreateDirectory(
      context.get(), path, exclusive, recursive));
}

TEST_F(ObfuscatedFileSystemFileUtilTest, TestReadDirectory) {
  scoped_ptr<FileSystemOperationContext> context(NewContext());
  bool exclusive = true;
  bool recursive = true;
  FilePath path = UTF8ToFilePath("directory/to/use");
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->CreateDirectory(
      context.get(), path, exclusive, recursive));
  TestReadDirectoryHelper(path);
}

TEST_F(ObfuscatedFileSystemFileUtilTest, TestReadRootWithSlash) {
  TestReadDirectoryHelper(UTF8ToFilePath(""));
}

TEST_F(ObfuscatedFileSystemFileUtilTest, TestReadRootWithEmptyString) {
  TestReadDirectoryHelper(UTF8ToFilePath("/"));
}

TEST_F(ObfuscatedFileSystemFileUtilTest, TestReadDirectoryOnFile) {
  FilePath path = UTF8ToFilePath("file");
  scoped_ptr<FileSystemOperationContext> context(NewContext());

  bool created = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
      ofsfu()->EnsureFileExists(context.get(), path, &created));
  ASSERT_TRUE(created);

  context.reset(NewContext());
  std::vector<base::FileUtilProxy::Entry> entries;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
      ofsfu()->ReadDirectory(context.get(), path, &entries));

  EXPECT_TRUE(ofsfu()->IsDirectoryEmpty(context.get(), path));
}

TEST_F(ObfuscatedFileSystemFileUtilTest, TestTouch) {
  FilePath path = UTF8ToFilePath("fake/file");
  base::Time last_access_time = base::Time::Now();  // Ignored, so not tested.
  base::Time last_modified_time = base::Time::Now();
  scoped_ptr<FileSystemOperationContext> context(NewContext());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            ofsfu()->Touch(
                context.get(), path, last_access_time, last_modified_time));

  // Touch will create a file if it's not there but its parent is.
  path = UTF8ToFilePath("file name");
  TestTouchHelper(path);

  bool exclusive = true;
  bool recursive = true;
  path = UTF8ToFilePath("directory/to/use");
  context.reset(NewContext());
  EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->CreateDirectory(
      context.get(), path, exclusive, recursive));
  TestTouchHelper(path);
}

TEST_F(ObfuscatedFileSystemFileUtilTest, TestCopyOrMoveFileNotFound) {
  FilePath source_path = UTF8ToFilePath("path0.txt");
  FilePath dest_path = UTF8ToFilePath("path1.txt");
  scoped_ptr<FileSystemOperationContext> context(NewContext());

  bool is_copy_not_move = false;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
      ofsfu()->CopyOrMoveFile(context.get(), source_path, dest_path,
          is_copy_not_move));
  context.reset(NewContext());
  is_copy_not_move = true;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
      ofsfu()->CopyOrMoveFile(context.get(), source_path, dest_path,
          is_copy_not_move));
  source_path = UTF8ToFilePath("dir/dir/file");
  bool exclusive = true;
  bool recursive = true;
  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK, ofsfu()->CreateDirectory(
      context.get(), source_path.DirName(), exclusive, recursive));
  is_copy_not_move = false;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
      ofsfu()->CopyOrMoveFile(context.get(), source_path, dest_path,
          is_copy_not_move));
  context.reset(NewContext());
  is_copy_not_move = true;
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
      ofsfu()->CopyOrMoveFile(context.get(), source_path, dest_path,
          is_copy_not_move));
}

TEST_F(ObfuscatedFileSystemFileUtilTest, TestCopyOrMoveFileSuccess) {
  const int64 kSourceLength = 5;
  const int64 kDestLength = 50;

  for (size_t i = 0; i < arraysize(kCopyMoveTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "kCopyMoveTestCase " << i);
    const CopyMoveTestCaseRecord& test_case = kCopyMoveTestCases[i];
    SCOPED_TRACE(testing::Message() << "\t is_copy_not_move " <<
      test_case.is_copy_not_move);
    SCOPED_TRACE(testing::Message() << "\t source_path " <<
      test_case.source_path);
    SCOPED_TRACE(testing::Message() << "\t dest_path " <<
      test_case.dest_path);
    SCOPED_TRACE(testing::Message() << "\t cause_overwrite " <<
      test_case.cause_overwrite);
    scoped_ptr<FileSystemOperationContext> context(NewContext());

    bool exclusive = false;
    bool recursive = true;
    FilePath source_path = UTF8ToFilePath(test_case.source_path);
    FilePath dest_path = UTF8ToFilePath(test_case.dest_path);

    context.reset(NewContext());
    ASSERT_EQ(base::PLATFORM_FILE_OK, ofsfu()->CreateDirectory(
        context.get(), source_path.DirName(), exclusive, recursive));
    context.reset(NewContext());
    ASSERT_EQ(base::PLATFORM_FILE_OK, ofsfu()->CreateDirectory(
        context.get(), dest_path.DirName(), exclusive, recursive));

    base::Time last_access_time;
    bool created = false;
    context.reset(NewContext());
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              ofsfu()->EnsureFileExists(context.get(), source_path, &created));
    ASSERT_TRUE(created);
    context.reset(NewContext());
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              ofsfu()->Truncate(context.get(), source_path, kSourceLength));

    if (test_case.cause_overwrite) {
      context.reset(NewContext());
      created = false;
      ASSERT_EQ(base::PLATFORM_FILE_OK,
                ofsfu()->EnsureFileExists(context.get(), dest_path, &created));
      ASSERT_TRUE(created);
      context.reset(NewContext());
      ASSERT_EQ(base::PLATFORM_FILE_OK,
                ofsfu()->Truncate(context.get(), dest_path, kDestLength));
    }

    context.reset(NewContext());
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->CopyOrMoveFile(context.get(),
        source_path, dest_path, test_case.is_copy_not_move));
    if (test_case.is_copy_not_move) {
      base::PlatformFileInfo file_info;
      FilePath local_path;
      context.reset(NewContext());
      EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->GetFileInfo(
          context.get(), source_path, &file_info, &local_path));
      EXPECT_EQ(kSourceLength, file_info.size);
      EXPECT_EQ(base::PLATFORM_FILE_OK,
                ofsfu()->DeleteFile(context.get(), source_path));
    } else {
      base::PlatformFileInfo file_info;
      FilePath local_path;
      context.reset(NewContext());
      EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, ofsfu()->GetFileInfo(
          context.get(), source_path, &file_info, &local_path));
    }
    base::PlatformFileInfo file_info;
    FilePath local_path;
    EXPECT_EQ(base::PLATFORM_FILE_OK, ofsfu()->GetFileInfo(
        context.get(), dest_path, &file_info, &local_path));
    EXPECT_EQ(kSourceLength, file_info.size);

    EXPECT_EQ(base::PLATFORM_FILE_OK,
              ofsfu()->DeleteFile(context.get(), dest_path));
  }
}

TEST_F(ObfuscatedFileSystemFileUtilTest, TestEnumerator) {
  scoped_ptr<FileSystemOperationContext> context(NewContext());
  FilePath src_path = UTF8ToFilePath("source dir");
  bool exclusive = true;
  bool recursive = false;
  ASSERT_EQ(base::PLATFORM_FILE_OK, ofsfu()->CreateDirectory(
      context.get(), src_path, exclusive, recursive));

  std::set<FilePath::StringType> files;
  std::set<FilePath::StringType> directories;
  FillTestDirectory(src_path, &files, &directories);

  FilePath dest_path = UTF8ToFilePath("destination dir");

  context.reset(NewContext());
  EXPECT_FALSE(ofsfu()->DirectoryExists(context.get(), dest_path));
  context.reset(NewContext());
  ASSERT_EQ(base::PLATFORM_FILE_OK,
      ofsfu()->Copy(context.get(), src_path, dest_path));

  ValidateTestDirectory(dest_path, files, directories);
  context.reset(NewContext());
  EXPECT_TRUE(ofsfu()->DirectoryExists(context.get(), src_path));
  context.reset(NewContext());
  EXPECT_TRUE(ofsfu()->DirectoryExists(context.get(), dest_path));
  context.reset(NewContext());
  recursive = true;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
      ofsfu()->Delete(context.get(), dest_path, recursive));
  context.reset(NewContext());
  EXPECT_FALSE(ofsfu()->DirectoryExists(context.get(), dest_path));
}

TEST_F(ObfuscatedFileSystemFileUtilTest, TestMigration) {
  ScopedTempDir source_dir;
  ASSERT_TRUE(source_dir.CreateUniqueTempDir());
  FilePath root_path = source_dir.path().AppendASCII("chrome-pLmnMWXE7NzTFRsn");
  ASSERT_TRUE(file_util::CreateDirectory(root_path));

  for (size_t i = 0; i < arraysize(kMigrationTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Creating kMigrationTestPath " << i);
    const MigrationTestCaseRecord& test_case = kMigrationTestCases[i];
    FilePath local_src_path = root_path.Append(test_case.path);
    if (test_case.is_directory) {
      ASSERT_TRUE(
          file_util::CreateDirectory(local_src_path));
    } else {
      base::PlatformFileError error_code;
      bool created = false;
      int file_flags = base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE;
      base::PlatformFile file_handle =
          base::CreatePlatformFile(
              local_src_path, file_flags, &created, &error_code);
      EXPECT_TRUE(created);
      ASSERT_EQ(base::PLATFORM_FILE_OK, error_code);
      ASSERT_TRUE(
          base::TruncatePlatformFile(file_handle, test_case.data_file_size));
      EXPECT_TRUE(base::ClosePlatformFile(file_handle));
    }
  }

  const GURL origin_url("http://example.com");
  fileapi::FileSystemType type = kFileSystemTypeTemporary;
  EXPECT_TRUE(ofsfu()->MigrateFromOldSandbox(origin_url, type, root_path));

  FilePath new_root =
    test_directory().AppendASCII("000").Append(
        ofsfu()->GetDirectoryNameForType(type)).AppendASCII("Legacy");
  for (size_t i = 0; i < arraysize(kMigrationTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Validating kMigrationTestPath " << i);
    const MigrationTestCaseRecord& test_case = kMigrationTestCases[i];
    FilePath local_data_path = new_root.Append(test_case.path);
#if defined(OS_WIN)
    local_data_path = local_data_path.NormalizeWindowsPathSeparators();
#endif
    scoped_ptr<FileSystemOperationContext> context(NewContext());
    base::PlatformFileInfo ofsfu_file_info;
    FilePath data_path;
    SCOPED_TRACE(testing::Message() << "Path is " << test_case.path);
    EXPECT_EQ(base::PLATFORM_FILE_OK,
        ofsfu()->GetFileInfo(context.get(), FilePath(test_case.path),
            &ofsfu_file_info, &data_path));
    if (test_case.is_directory) {
      EXPECT_TRUE(ofsfu_file_info.is_directory);
    } else {
      base::PlatformFileInfo platform_file_info;
      SCOPED_TRACE(testing::Message() << "local_data_path is " <<
          local_data_path.value());
      SCOPED_TRACE(testing::Message() << "data_path is " << data_path.value());
      ASSERT_TRUE(file_util::GetFileInfo(local_data_path, &platform_file_info));
      EXPECT_EQ(test_case.data_file_size, platform_file_info.size);
      EXPECT_FALSE(platform_file_info.is_directory);
      scoped_ptr<FileSystemOperationContext> context(NewContext());
      EXPECT_EQ(local_data_path, data_path);
      EXPECT_EQ(platform_file_info.size, ofsfu_file_info.size);
      EXPECT_FALSE(ofsfu_file_info.is_directory);
    }
  }
}
