// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_util_helper.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/isolated_file_util.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/local_file_system_test_helper.h"
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

bool IsDirectoryEmpty(FileSystemOperationContext* context,
                      FileSystemFileUtil* file_util,
                      const FileSystemURL& url) {
  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum =
      file_util->CreateFileEnumerator(context, url, false /* recursive */);
  return file_enum->Next().empty();
}

}  // namespace

// TODO(kinuko): we should have separate tests for DraggedFileUtil and
// IsolatedFileUtil.
class IsolatedFileUtilTest : public testing::Test {
 public:
  IsolatedFileUtilTest()
      : other_file_util_helper_(GURL("http://foo/"), kFileSystemTypeTest) {}

  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(partition_dir_.CreateUniqueTempDir());
    file_util_.reset(new DraggedFileUtil());

    // Register the files/directories of RegularTestCases (with random
    // root paths) as dropped files.
    SimulateDropFiles();

    file_system_context_ = new FileSystemContext(
        FileSystemTaskRunners::CreateMockTaskRunners(),
        make_scoped_refptr(new quota::MockSpecialStoragePolicy()),
        NULL /* quota_manager */,
        partition_dir_.path(),
        CreateAllowFileAccessOptions());

    // For cross-FileUtil copy/move tests.
    other_file_util_helper_.SetUp(file_system_context_);

    isolated_context()->AddReference(filesystem_id_);
  }

  void TearDown() {
    isolated_context()->RemoveReference(filesystem_id_);
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
  FileSystemFileUtil* other_file_util() const {
    return other_file_util_helper_.file_util();
  }
  std::string filesystem_id() const { return filesystem_id_; }

  FilePath GetTestCasePlatformPath(const FilePath::StringType& path) {
    return toplevel_root_map_[GetTopLevelPath(FilePath(path))].Append(path).
        NormalizePathSeparators();
  }

  FilePath GetTestCaseLocalPath(const FilePath& path) {
    FilePath relative;
    if (data_dir_.path().AppendRelativePath(path, &relative))
      return relative;
    return path;
  }

  FileSystemURL GetFileSystemURL(const FilePath& path) const {
    FilePath virtual_path = isolated_context()->CreateVirtualRootPath(
        filesystem_id()).Append(path);
    return FileSystemURL(GURL("http://example.com"),
                         kFileSystemTypeIsolated,
                         virtual_path);
  }

  FileSystemURL GetOtherFileSystemURL(const FilePath& path) {
    return other_file_util_helper_.CreateURL(GetTestCaseLocalPath(path));
  }

  void VerifyFilesHaveSameContent(FileSystemFileUtil* file_util1,
                                  FileSystemFileUtil* file_util2,
                                  const FileSystemURL& url1,
                                  const FileSystemURL& url2) {
    scoped_ptr<FileSystemOperationContext> context;

    // Get the file info for url1.
    base::PlatformFileInfo info1;
    FilePath platform_path1;
    context.reset(new FileSystemOperationContext(file_system_context()));
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              file_util1->GetFileInfo(context.get(), url1,
                                      &info1, &platform_path1));

    // Get the file info for url2.
    base::PlatformFileInfo info2;
    FilePath platform_path2;
    context.reset(new FileSystemOperationContext(file_system_context()));
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              file_util2->GetFileInfo(context.get(), url2,
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
                                        const FileSystemURL& root1,
                                        const FileSystemURL& root2) {
    scoped_ptr<FileSystemOperationContext> context;
    FilePath root_path1 = root1.path();
    FilePath root_path2 = root2.path();

    context.reset(new FileSystemOperationContext(file_system_context()));
    scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum1 =
        file_util1->CreateFileEnumerator(context.get(), root1,
                                         true /* recursive */);

    FilePath current;
    std::set<FilePath> file_set1;
    while (!(current = file_enum1->Next()).empty()) {
      if (file_enum1->IsDirectory())
        continue;
      FilePath relative;
      root_path1.AppendRelativePath(current, &relative);
      file_set1.insert(relative);
    }

    context.reset(new FileSystemOperationContext(file_system_context()));
    scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum2 =
        file_util2->CreateFileEnumerator(context.get(), root2,
                                         true /* recursive */);

    while (!(current = file_enum2->Next()).empty()) {
      FilePath relative;
      root_path2.AppendRelativePath(current, &relative);
      FileSystemURL url1 = root1.WithPath(root_path1.Append(relative));
      FileSystemURL url2 = root2.WithPath(root_path2.Append(relative));
      if (file_enum2->IsDirectory()) {
        FileSystemOperationContext context1(file_system_context());
        FileSystemOperationContext context2(file_system_context());
        EXPECT_EQ(IsDirectoryEmpty(&context1, file_util1, url1),
                  IsDirectoryEmpty(&context2, file_util2, url2));
        continue;
      }
      EXPECT_TRUE(file_set1.find(relative) != file_set1.end());
      VerifyFilesHaveSameContent(file_util1, file_util2, url1, url2);
    }
  }

  scoped_ptr<FileSystemOperationContext> GetOperationContext() {
    return make_scoped_ptr(
        new FileSystemOperationContext(file_system_context())).Pass();
  }


 private:
  void SimulateDropFiles() {
    size_t root_path_index = 0;

    IsolatedContext::FileInfoSet toplevels;
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
        toplevels.AddPath(root.Append(path), NULL);
      }

      test::SetUpOneTestCase(toplevel_root_map_[toplevel], test_case);
    }

    // Register the toplevel entries.
    filesystem_id_ = isolated_context()->RegisterDraggedFileSystem(toplevels);
  }

  base::ScopedTempDir data_dir_;
  base::ScopedTempDir partition_dir_;
  MessageLoop message_loop_;
  std::string filesystem_id_;
  scoped_refptr<FileSystemContext> file_system_context_;
  std::map<FilePath, FilePath> toplevel_root_map_;
  scoped_ptr<IsolatedFileUtil> file_util_;
  LocalFileSystemTestOriginHelper other_file_util_helper_;
  DISALLOW_COPY_AND_ASSIGN(IsolatedFileUtilTest);
};

TEST_F(IsolatedFileUtilTest, BasicTest) {
  for (size_t i = 0; i < test::kRegularTestCaseSize; ++i) {
    SCOPED_TRACE(testing::Message() << "Testing RegularTestCases " << i);
    const test::TestCaseRecord& test_case = test::kRegularTestCases[i];

    FileSystemURL url = GetFileSystemURL(FilePath(test_case.path));

    // See if we can query the file info via the isolated FileUtil.
    // (This should succeed since we have registered all the top-level
    // entries of the test cases in SetUp())
    base::PlatformFileInfo info;
    FilePath platform_path;
    FileSystemOperationContext context(file_system_context());
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              file_util()->GetFileInfo(&context, url, &info, &platform_path));

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
    FileSystemURL url = GetFileSystemURL(FilePath(test_case.path));

    // We should not be able to get the valid URL for unregistered files.
    ASSERT_FALSE(url.is_valid());

    // We should not be able to create a new operation for an invalid URL.
    base::PlatformFileError error_code;
    FileSystemOperation* operation =
        file_system_context()->CreateFileSystemOperation(url, &error_code);
    ASSERT_EQ(NULL, operation);
    ASSERT_EQ(base::PLATFORM_FILE_ERROR_INVALID_URL, error_code);
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
        GetTestCasePlatformPath(test_case.path), false /* not recursive */,
        FileEnumerator::FILES | FileEnumerator::DIRECTORIES);
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
    FileSystemURL url = GetFileSystemURL(FilePath(test_case.path));
    std::vector<base::FileUtilProxy::Entry> entries;
    FileSystemOperationContext context(file_system_context());
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              FileUtilHelper::ReadDirectory(
                  &context, file_util(), url, &entries));

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

TEST_F(IsolatedFileUtilTest, GetLocalFilePathTest) {
  for (size_t i = 0; i < test::kRegularTestCaseSize; ++i) {
    const test::TestCaseRecord& test_case = test::kRegularTestCases[i];
    FileSystemURL url = GetFileSystemURL(FilePath(test_case.path));

    FileSystemOperationContext context(file_system_context());

    FilePath local_file_path;
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              file_util()->GetLocalFilePath(&context, url, &local_file_path));
    EXPECT_EQ(GetTestCasePlatformPath(test_case.path).value(),
              local_file_path.value());
  }
}

TEST_F(IsolatedFileUtilTest, CopyOutFileTest) {
  scoped_ptr<FileSystemOperationContext> context(
      new FileSystemOperationContext(file_system_context()));
  FileSystemURL root_url = GetFileSystemURL(FilePath());
  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum(
      file_util()->CreateFileEnumerator(context.get(),
                                        root_url,
                                        true /* recursive */));
  FilePath current;
  while (!(current = file_enum->Next()).empty()) {
    if (file_enum->IsDirectory())
      continue;

    SCOPED_TRACE(testing::Message() << "Testing file copy "
                 << current.value());

    FileSystemURL src_url = root_url.WithPath(current);
    FileSystemURL dest_url = GetOtherFileSystemURL(current);

    // Create the parent directory of the destination.
    context.reset(new FileSystemOperationContext(file_system_context()));
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              other_file_util()->CreateDirectory(
                  context.get(),
                  GetOtherFileSystemURL(current.DirName()),
                  false /* exclusive */,
                  true /* recursive */));

    context.reset(new FileSystemOperationContext(file_system_context()));
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              FileUtilHelper::Copy(
                  context.get(),
                  file_util(), other_file_util(),
                  src_url, dest_url));

    VerifyFilesHaveSameContent(file_util(), other_file_util(),
                               src_url, dest_url);
  }
}

TEST_F(IsolatedFileUtilTest, CopyOutDirectoryTest) {
  scoped_ptr<FileSystemOperationContext> context(
      new FileSystemOperationContext(file_system_context()));
  FileSystemURL root_url = GetFileSystemURL(FilePath());
  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum(
      file_util()->CreateFileEnumerator(context.get(),
                                        root_url,
                                        false /* recursive */));
  FilePath current;
  while (!(current = file_enum->Next()).empty()) {
    if (!file_enum->IsDirectory())
      continue;

    SCOPED_TRACE(testing::Message() << "Testing directory copy "
                 << current.value());

    FileSystemURL src_url = root_url.WithPath(current);
    FileSystemURL dest_url = GetOtherFileSystemURL(current);

    // Create the parent directory of the destination.
    context.reset(new FileSystemOperationContext(file_system_context()));
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              other_file_util()->CreateDirectory(
                  context.get(),
                  GetOtherFileSystemURL(current.DirName()),
                  false /* exclusive */,
                  true /* recursive */));

    context.reset(new FileSystemOperationContext(file_system_context()));
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              FileUtilHelper::Copy(
                  context.get(),
                  file_util(), other_file_util(),
                  src_url, dest_url));

    VerifyDirectoriesHaveSameContent(file_util(), other_file_util(),
                                     src_url, dest_url);
  }
}

TEST_F(IsolatedFileUtilTest, TouchTest) {
  for (size_t i = 0; i < test::kRegularTestCaseSize; ++i) {
    const test::TestCaseRecord& test_case = test::kRegularTestCases[i];
    if (test_case.is_directory)
      continue;
    SCOPED_TRACE(testing::Message() << test_case.path);
    FileSystemURL url = GetFileSystemURL(FilePath(test_case.path));

    base::Time last_access_time = base::Time::FromTimeT(1000);
    base::Time last_modified_time = base::Time::FromTimeT(2000);

    EXPECT_EQ(base::PLATFORM_FILE_OK,
              file_util()->Touch(GetOperationContext().get(), url,
                                 last_access_time,
                                 last_modified_time));

    // Verification.
    base::PlatformFileInfo info;
    FilePath platform_path;
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              file_util()->GetFileInfo(GetOperationContext().get(), url,
                                       &info, &platform_path));
    EXPECT_EQ(last_access_time.ToTimeT(), info.last_accessed.ToTimeT());
    EXPECT_EQ(last_modified_time.ToTimeT(), info.last_modified.ToTimeT());
  }
}

TEST_F(IsolatedFileUtilTest, TruncateTest) {
  for (size_t i = 0; i < test::kRegularTestCaseSize; ++i) {
    const test::TestCaseRecord& test_case = test::kRegularTestCases[i];
    if (test_case.is_directory)
      continue;

    SCOPED_TRACE(testing::Message() << test_case.path);
    FileSystemURL url = GetFileSystemURL(FilePath(test_case.path));

    // Truncate to 0.
    base::PlatformFileInfo info;
    FilePath platform_path;
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              file_util()->Truncate(GetOperationContext().get(), url, 0));
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              file_util()->GetFileInfo(GetOperationContext().get(), url,
                                       &info, &platform_path));
    EXPECT_EQ(0, info.size);

    // Truncate (extend) to 999.
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              file_util()->Truncate(GetOperationContext().get(), url, 999));
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              file_util()->GetFileInfo(GetOperationContext().get(), url,
                                       &info, &platform_path));
    EXPECT_EQ(999, info.size);
  }
}

}  // namespace fileapi
