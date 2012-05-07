// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_test_helper.h"
#include "webkit/fileapi/file_util_helper.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/isolated_file_util.h"
#include "webkit/fileapi/local_file_util.h"
#include "webkit/fileapi/mock_file_system_options.h"
#include "webkit/fileapi/native_file_util.h"
#include "webkit/fileapi/test_file_set.h"
#include "webkit/quota/mock_special_storage_policy.h"

using file_util::FileEnumerator;

namespace fileapi {

namespace {

// Used in IsolatedFileUtilTest::SimulateDropFiles().
// Random root paths in which we create each file/directory of the
// RegularTestCases (so that we can simulate a drop with files/directories
// from multiple directories).
static const FilePath::CharType* kRootPaths[] = {
  FILE_PATH_LITERAL("a"),
  FILE_PATH_LITERAL("b/c"),
  FILE_PATH_LITERAL("etc"),
};

FilePath GetTopLevelPath(const FilePath& path) {
  std::vector<FilePath::StringType> components;
  path.GetComponents(&components);
  return FilePath(components[0]);
}

}  // namespace

class IsolatedFileUtilTest : public testing::Test {
 public:
  IsolatedFileUtilTest() {}

  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    file_util_.reset(new IsolatedFileUtil(new NativeFileUtil()));

    // Register the files/directories of RegularTestCases (with random
    // root paths) as dropped files.
    SimulateDropFiles();

    file_system_context_ = new FileSystemContext(
        base::MessageLoopProxy::current(),
        base::MessageLoopProxy::current(),
        make_scoped_refptr(new quota::MockSpecialStoragePolicy()),
        NULL /* quota_manager */,
        data_dir_.path(),
        CreateAllowFileAccessOptions());

    // For cross-FileUtil copy/move tests.
    other_file_util_.reset(new LocalFileUtil(new NativeFileUtil()));
    other_file_util_helper_.SetUp(file_system_context_, other_file_util_.get());
  }

  void TearDown() {
    isolated_context()->RevokeIsolatedFileSystem(filesystem_id_);
    other_file_util_helper_.TearDown();
  }

 protected:
  IsolatedContext* isolated_context() const {
    return IsolatedContext::GetInstance();
  }
  const FilePath& root_path() const {
    return data_dir_.path();
  }
  FileSystemContext* file_system_context() const {
    return file_system_context_.get();
  }
  FileSystemFileUtil* file_util() const { return file_util_.get(); }
  FileSystemFileUtil* other_file_util() const { return other_file_util_.get(); }
  std::string filesystem_id() const { return filesystem_id_; }

  FilePath GetTestCasePlatformPath(const FilePath::StringType& path) {
    return toplevel_root_map_[GetTopLevelPath(FilePath(path))].Append(path).
        NormalizePathSeparators();
  }

  FileSystemPath GetFileSystemPath(const FilePath& path) const {
    FilePath virtual_path = isolated_context()->CreateVirtualPath(
        filesystem_id(), path);
    return FileSystemPath(GURL("http://example.com"),
                          kFileSystemTypeIsolated,
                          virtual_path);
  }

  FileSystemPath GetOtherFileSystemPath(const FilePath& path) {
    return other_file_util_helper_.CreatePath(path);
  }

  void VerifyFilesHaveSameContent(FileSystemFileUtil* file_util1,
                                  FileSystemFileUtil* file_util2,
                                  const FileSystemPath& path1,
                                  const FileSystemPath& path2) {
    scoped_ptr<FileSystemOperationContext> context;

    // Get the file info for path1.
    base::PlatformFileInfo info1;
    FilePath platform_path1;
    context.reset(new FileSystemOperationContext(file_system_context()));
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              file_util1->GetFileInfo(context.get(), path1,
                                      &info1, &platform_path1));

    // Get the file info for path2.
    base::PlatformFileInfo info2;
    FilePath platform_path2;
    context.reset(new FileSystemOperationContext(file_system_context()));
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              file_util2->GetFileInfo(context.get(), path2,
                                      &info2, &platform_path2));

    // See if file info matches with the other one.
    EXPECT_EQ(info1.is_directory, info2.is_directory);
    EXPECT_EQ(info1.size, info2.size);
    EXPECT_EQ(info1.is_symbolic_link, info2.is_symbolic_link);
    EXPECT_NE(platform_path1, platform_path2);

    std::string content1, content2;
    EXPECT_TRUE(file_util::ReadFileToString(platform_path1, &content1));
    EXPECT_TRUE(file_util::ReadFileToString(platform_path2, &content2));
    EXPECT_EQ(content1, content2);
  }

  void VerifyDirectoriesHaveSameContent(FileSystemFileUtil* file_util1,
                                        FileSystemFileUtil* file_util2,
                                        const FileSystemPath& root1,
                                        const FileSystemPath& root2) {
    scoped_ptr<FileSystemOperationContext> context;

    scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum1;
    context.reset(new FileSystemOperationContext(file_system_context()));
    file_enum1.reset(file_util1->CreateFileEnumerator(
        context.get(), root1, true /* recursive */));

    FilePath current;
    std::set<FilePath> file_set1;
    while (!(current = file_enum1->Next()).empty()) {
      if (file_enum1->IsDirectory())
        continue;
      file_set1.insert(current);
    }

    scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum2;
    context.reset(new FileSystemOperationContext(file_system_context()));
    file_enum2.reset(file_util2->CreateFileEnumerator(
        context.get(), root2, true /* recursive */));

    while (!(current = file_enum2->Next()).empty()) {
      FileSystemPath path1 = root1.WithInternalPath(current);
      FileSystemPath path2 = root2.WithInternalPath(current);
      if (file_enum2->IsDirectory()) {
        FileSystemOperationContext context1(file_system_context());
        FileSystemOperationContext context2(file_system_context());
        EXPECT_EQ(file_util1->IsDirectoryEmpty(&context1, path1),
                  file_util2->IsDirectoryEmpty(&context2, path2));
        continue;
      }
      EXPECT_TRUE(file_set1.find(current) != file_set1.end());
      VerifyFilesHaveSameContent(file_util1, file_util2, path1, path2);
    }
  }

 private:
  void SimulateDropFiles() {
    size_t root_path_index = 0;

    std::set<FilePath> toplevels;
    for (size_t i = 0; i < test::kRegularTestCaseSize; ++i) {
      const test::TestCaseRecord& test_case = test::kRegularTestCases[i];
      FilePath path(test_case.path);
      FilePath toplevel = GetTopLevelPath(path);

      // We create the test case files under one of the kRootPaths
      // to simulate a drop with multiple directories.
      if (toplevel_root_map_.find(toplevel) == toplevel_root_map_.end()) {
        FilePath root = root_path().Append(
            kRootPaths[(root_path_index++) % arraysize(kRootPaths)]);
        toplevel_root_map_[toplevel] = root;
        toplevels.insert(root.Append(path));
      }

      test::SetUpOneTestCase(toplevel_root_map_[toplevel], test_case);
    }

    // Register the toplevel entries.
    filesystem_id_ = isolated_context()->RegisterIsolatedFileSystem(toplevels);
  }

  ScopedTempDir data_dir_;
  std::string filesystem_id_;
  scoped_refptr<FileSystemContext> file_system_context_;
  std::map<FilePath, FilePath> toplevel_root_map_;
  scoped_ptr<IsolatedFileUtil> file_util_;
  scoped_ptr<LocalFileUtil> other_file_util_;
  FileSystemTestOriginHelper other_file_util_helper_;
  DISALLOW_COPY_AND_ASSIGN(IsolatedFileUtilTest);
};

TEST_F(IsolatedFileUtilTest, BasicTest) {
  for (size_t i = 0; i < test::kRegularTestCaseSize; ++i) {
    SCOPED_TRACE(testing::Message() << "Testing RegularTestCases " << i);
    const test::TestCaseRecord& test_case = test::kRegularTestCases[i];

    FileSystemPath path = GetFileSystemPath(FilePath(test_case.path));

    // See if we can query the file info via the isolated FileUtil.
    // (This should succeed since we have registered all the top-level
    // entries of the test cases in SetUp())
    base::PlatformFileInfo info;
    FilePath platform_path;
    FileSystemOperationContext context(file_system_context());
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              file_util()->GetFileInfo(&context, path, &info, &platform_path));

    // See if the obtained file info is correct.
    if (!test_case.is_directory)
      ASSERT_EQ(test_case.data_file_size, info.size);
    ASSERT_EQ(test_case.is_directory, info.is_directory);
    ASSERT_EQ(GetTestCasePlatformPath(test_case.path),
              platform_path.NormalizePathSeparators());
  }
}

TEST_F(IsolatedFileUtilTest, UnregisteredPathsTest) {
  static const fileapi::test::TestCaseRecord kUnregisteredCases[] = {
    {true, FILE_PATH_LITERAL("nonexistent"), 0},
    {true, FILE_PATH_LITERAL("nonexistent/dir foo"), 0},
    {false, FILE_PATH_LITERAL("nonexistent/false"), 0},
    {false, FILE_PATH_LITERAL("foo"), 30},
    {false, FILE_PATH_LITERAL("bar"), 20},
  };

  for (size_t i = 0; i < arraysize(kUnregisteredCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Creating kUnregisteredCases " << i);
    const test::TestCaseRecord& test_case = kUnregisteredCases[i];

    // Prepare the test file/directory.
    SetUpOneTestCase(root_path(), test_case);

    // Make sure regular GetFileInfo succeeds.
    base::PlatformFileInfo info;
    ASSERT_TRUE(file_util::GetFileInfo(
        root_path().Append(test_case.path), &info));
    if (!test_case.is_directory)
      ASSERT_EQ(test_case.data_file_size, info.size);
    ASSERT_EQ(test_case.is_directory, info.is_directory);
  }

  for (size_t i = 0; i < arraysize(kUnregisteredCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Creating kUnregisteredCases " << i);
    const test::TestCaseRecord& test_case = kUnregisteredCases[i];
    FileSystemPath path = GetFileSystemPath(FilePath(test_case.path));

    // This should fail as the paths in kUnregisteredCases are not included
    // in the dropped files (i.e. the regular test cases).
    base::PlatformFileInfo info;
    FilePath platform_path;
    FileSystemOperationContext context(file_system_context());
    ASSERT_EQ(base::PLATFORM_FILE_ERROR_SECURITY,
              file_util()->GetFileInfo(&context, path, &info, &platform_path));
  }
}

TEST_F(IsolatedFileUtilTest, ReadDirectoryTest) {
  for (size_t i = 0; i < test::kRegularTestCaseSize; ++i) {
    const test::TestCaseRecord& test_case = test::kRegularTestCases[i];
    if (!test_case.is_directory)
      continue;

    SCOPED_TRACE(testing::Message() << "Testing RegularTestCases " << i
                 << ": " << test_case.path);

    // Read entries in the directory to construct the expected results map.
    typedef std::map<FilePath::StringType, base::FileUtilProxy::Entry> EntryMap;
    EntryMap expected_entry_map;

    FileEnumerator file_enum(
        GetTestCasePlatformPath(test_case.path),
        false /* recursive */,
        static_cast<file_util::FileEnumerator::FileType>(
            FileEnumerator::FILES | FileEnumerator::DIRECTORIES));
    FilePath current;
    while (!(current = file_enum.Next()).empty()) {
      FileEnumerator::FindInfo file_info;
      file_enum.GetFindInfo(&file_info);
      base::FileUtilProxy::Entry entry;
      entry.is_directory = FileEnumerator::IsDirectory(file_info);
      entry.name = current.BaseName().value();
      entry.size = FileEnumerator::GetFilesize(file_info);
      entry.last_modified_time = FileEnumerator::GetLastModifiedTime(file_info);
      expected_entry_map[entry.name] = entry;
    }

    // Perform ReadDirectory in the isolated filesystem.
    FileSystemPath path = GetFileSystemPath(FilePath(test_case.path));
    std::vector<base::FileUtilProxy::Entry> entries;
    FileSystemOperationContext context(file_system_context());
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              FileUtilHelper::ReadDirectory(
                  &context, file_util(), path, &entries));

    EXPECT_EQ(expected_entry_map.size(), entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
      const base::FileUtilProxy::Entry& entry = entries[i];
      EntryMap::iterator found = expected_entry_map.find(entry.name);
      EXPECT_TRUE(found != expected_entry_map.end());
      EXPECT_EQ(found->second.name, entry.name);
      EXPECT_EQ(found->second.is_directory, entry.is_directory);
      EXPECT_EQ(found->second.size, entry.size);
      EXPECT_EQ(found->second.last_modified_time.ToDoubleT(),
                entry.last_modified_time.ToDoubleT());
    }
  }
}

TEST_F(IsolatedFileUtilTest, CopyOutFileTest) {
  scoped_ptr<FileSystemOperationContext> context(
      new FileSystemOperationContext(file_system_context()));
  FileSystemPath root_path = GetFileSystemPath(FilePath());
  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum(
      file_util()->CreateFileEnumerator(context.get(),
                                        root_path,
                                        true /* recursive */));
  FilePath current;
  while (!(current = file_enum->Next()).empty()) {
    if (file_enum->IsDirectory())
      continue;

    SCOPED_TRACE(testing::Message() << "Testing file copy "
                 << current.value());

    FileSystemPath src_path = root_path.WithInternalPath(current);
    FileSystemPath dest_path = GetOtherFileSystemPath(current);

    // Create the parent directory of the destination.
    context.reset(new FileSystemOperationContext(file_system_context()));
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              other_file_util()->CreateDirectory(
                  context.get(),
                  GetOtherFileSystemPath(current.DirName()),
                  false /* exclusive */,
                  true /* recursive */));

    context.reset(new FileSystemOperationContext(file_system_context()));
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              FileUtilHelper::Copy(
                  context.get(),
                  file_util(), other_file_util(),
                  src_path, dest_path));

    // The other way (copy-in) should not work.
    context.reset(new FileSystemOperationContext(file_system_context()));
    ASSERT_EQ(base::PLATFORM_FILE_ERROR_SECURITY,
              FileUtilHelper::Copy(
                  context.get(),
                  other_file_util(), file_util(),
                  dest_path, src_path));

    VerifyFilesHaveSameContent(file_util(), other_file_util(),
                               src_path, dest_path);
  }
}

TEST_F(IsolatedFileUtilTest, CopyOutDirectoryTest) {
  scoped_ptr<FileSystemOperationContext> context(
      new FileSystemOperationContext(file_system_context()));
  FileSystemPath root_path = GetFileSystemPath(FilePath());
  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum(
      file_util()->CreateFileEnumerator(context.get(),
                                        root_path,
                                        false /* recursive */));
  FilePath current;
  while (!(current = file_enum->Next()).empty()) {
    if (!file_enum->IsDirectory())
      continue;

    SCOPED_TRACE(testing::Message() << "Testing directory copy "
                 << current.value());

    FileSystemPath src_path = root_path.WithInternalPath(current);
    FileSystemPath dest_path = GetOtherFileSystemPath(current);

    // Create the parent directory of the destination.
    context.reset(new FileSystemOperationContext(file_system_context()));
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              other_file_util()->CreateDirectory(
                  context.get(),
                  GetOtherFileSystemPath(current.DirName()),
                  false /* exclusive */,
                  true /* recursive */));

    context.reset(new FileSystemOperationContext(file_system_context()));
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              FileUtilHelper::Copy(
                  context.get(),
                  file_util(), other_file_util(),
                  src_path, dest_path));

    // The other way (copy-in) should not work for two reasons:
    // write is prohibited in the isolated filesystem, and copying directory
    // to non-empty directory shouldn't work.
    context.reset(new FileSystemOperationContext(file_system_context()));
    base::PlatformFileError result = FileUtilHelper::Copy(
        context.get(), other_file_util(), file_util(), dest_path, src_path);
    ASSERT_TRUE(result == base::PLATFORM_FILE_ERROR_FAILED ||
                result == base::PLATFORM_FILE_ERROR_NOT_EMPTY);

    VerifyDirectoriesHaveSameContent(file_util(), other_file_util(),
                                     src_path, dest_path);
  }
}

}  // namespace fileapi
