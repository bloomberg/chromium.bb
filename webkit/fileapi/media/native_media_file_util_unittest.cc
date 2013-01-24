// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/media/native_media_file_util.h"
#include "webkit/fileapi/mock_file_system_options.h"
#include "webkit/fileapi/native_file_util.h"
#include "webkit/quota/mock_special_storage_policy.h"

#define FPL(x) FILE_PATH_LITERAL(x)

namespace fileapi {

namespace {

typedef FileSystemOperation::FileEntryList FileEntryList;

struct FilteringTestCase {
  const FilePath::CharType* path;
  bool is_directory;
  bool visible;
};

const FilteringTestCase kFilteringTestCases[] = {
  // Directory should always be visible.
  { FPL("hoge"), true, true },
  { FPL("fuga.jpg"), true, true },
  { FPL("piyo.txt"), true, true },
  { FPL("moga.cod"), true, true },

  // File should be visible if it's a supported media file.
  { FPL("foo"), false, false },  // File without extension.
  { FPL("bar.jpg"), false, true },  // Supported media file.
  { FPL("baz.txt"), false, false },  // Non-media file.
  { FPL("foobar.cod"), false, false },  // Unsupported media file.
};

void ExpectEqHelper(const std::string& test_name,
                    base::PlatformFileError expected,
                    base::PlatformFileError actual) {
  EXPECT_EQ(expected, actual) << test_name;
}

void ExpectMetadataEqHelper(const std::string& test_name,
                            base::PlatformFileError expected,
                            bool expected_is_directory,
                            base::PlatformFileError actual,
                            const base::PlatformFileInfo& file_info,
                            const FilePath& /*platform_path*/) {
  EXPECT_EQ(expected, actual) << test_name;
  if (actual == base::PLATFORM_FILE_OK)
    EXPECT_EQ(expected_is_directory, file_info.is_directory) << test_name;
}

void DidReadDirectory(std::set<FilePath::StringType>* content,
                      bool* completed,
                      base::PlatformFileError error,
                      const FileEntryList& file_list,
                      bool has_more) {
  EXPECT_TRUE(!*completed);
  *completed = !has_more;
  for (FileEntryList::const_iterator itr = file_list.begin();
       itr != file_list.end(); ++itr)
    EXPECT_TRUE(content->insert(itr->name).second);
}

void PopulateDirectoryWithTestCases(const FilePath& dir,
                                    const FilteringTestCase* test_cases,
                                    size_t n) {
  for (size_t i = 0; i < n; ++i) {
    FilePath path = dir.Append(test_cases[i].path);
    if (test_cases[i].is_directory) {
      ASSERT_TRUE(file_util::CreateDirectory(path));
    } else {
      bool created = false;
      ASSERT_EQ(base::PLATFORM_FILE_OK,
                NativeFileUtil::EnsureFileExists(path, &created));
      ASSERT_TRUE(created);
    }
  }
}

}  // namespace

class NativeMediaFileUtilTest : public testing::Test {
 public:
  NativeMediaFileUtilTest()
      : file_util_(NULL) {
  }

  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(file_util::CreateDirectory(root_path()));

    scoped_refptr<quota::SpecialStoragePolicy> storage_policy =
        new quota::MockSpecialStoragePolicy();

    file_system_context_ =
        new FileSystemContext(
            FileSystemTaskRunners::CreateMockTaskRunners(),
            storage_policy,
            NULL,
            data_dir_.path(),
            CreateAllowFileAccessOptions());

    file_util_ = file_system_context_->GetFileUtil(kFileSystemTypeNativeMedia);

    filesystem_id_ = isolated_context()->RegisterFileSystemForPath(
        kFileSystemTypeNativeMedia, root_path(), NULL);

    isolated_context()->AddReference(filesystem_id_);
  }

  void TearDown() {
    isolated_context()->RemoveReference(filesystem_id_);
    file_system_context_ = NULL;
  }

 protected:
  FileSystemContext* file_system_context() {
    return file_system_context_.get();
  }

  FileSystemURL CreateURL(const FilePath::CharType* test_case_path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        origin(),
        fileapi::kFileSystemTypeIsolated,
        GetVirtualPath(test_case_path));
  }

  IsolatedContext* isolated_context() {
    return IsolatedContext::GetInstance();
  }

  FilePath root_path() {
    return data_dir_.path().Append(FPL("Media Directory"));
  }

  FilePath GetVirtualPath(const FilePath::CharType* test_case_path) {
    return FilePath::FromUTF8Unsafe(filesystem_id_).
               Append(FPL("Media Directory")).
               Append(FilePath(test_case_path));
  }

  FileSystemFileUtil* file_util() {
    return file_util_;
  }

  GURL origin() {
    return GURL("http://example.com");
  }

  fileapi::FileSystemType type() {
    return kFileSystemTypeNativeMedia;
  }

  FileSystemOperation* NewOperation(const FileSystemURL& url) {
    return file_system_context_->CreateFileSystemOperation(url, NULL);
  }

 private:
  MessageLoop message_loop_;

  base::ScopedTempDir data_dir_;
  scoped_refptr<FileSystemContext> file_system_context_;

  FileSystemFileUtil* file_util_;
  std::string filesystem_id_;

  DISALLOW_COPY_AND_ASSIGN(NativeMediaFileUtilTest);
};

TEST_F(NativeMediaFileUtilTest, DirectoryExistsAndFileExistsFiltering) {
  PopulateDirectoryWithTestCases(root_path(),
                                 kFilteringTestCases,
                                 arraysize(kFilteringTestCases));

  for (size_t i = 0; i < arraysize(kFilteringTestCases); ++i) {
    FileSystemURL url = CreateURL(kFilteringTestCases[i].path);
    FileSystemOperation* operation = NewOperation(url);

    base::PlatformFileError expectation =
        kFilteringTestCases[i].visible ?
        base::PLATFORM_FILE_OK :
        base::PLATFORM_FILE_ERROR_NOT_FOUND;

    std::string test_name =
        base::StringPrintf("DirectoryExistsAndFileExistsFiltering %" PRIuS, i);
    if (kFilteringTestCases[i].is_directory) {
      operation->DirectoryExists(
          url, base::Bind(&ExpectEqHelper, test_name, expectation));
    } else {
      operation->FileExists(
          url, base::Bind(&ExpectEqHelper, test_name, expectation));
    }
    MessageLoop::current()->RunUntilIdle();
  }
}

TEST_F(NativeMediaFileUtilTest, ReadDirectoryFiltering) {
  PopulateDirectoryWithTestCases(root_path(),
                                 kFilteringTestCases,
                                 arraysize(kFilteringTestCases));

  std::set<FilePath::StringType> content;
  FileSystemURL url = CreateURL(FPL(""));
  bool completed = false;
  NewOperation(url)->ReadDirectory(
      url, base::Bind(&DidReadDirectory, &content, &completed));
  MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(completed);
  EXPECT_EQ(5u, content.size());

  for (size_t i = 0; i < arraysize(kFilteringTestCases); ++i) {
    FilePath::StringType name =
        FilePath(kFilteringTestCases[i].path).BaseName().value();
    std::set<FilePath::StringType>::const_iterator found = content.find(name);
    EXPECT_EQ(kFilteringTestCases[i].visible, found != content.end());
  }
}

TEST_F(NativeMediaFileUtilTest, CreateFileAndCreateDirectoryFiltering) {
  // Run the loop twice. The second loop attempts to create files that are
  // pre-existing. Though the result should be the same.
  for (int loop_count = 0; loop_count < 2; ++loop_count) {
    for (size_t i = 0; i < arraysize(kFilteringTestCases); ++i) {
      FileSystemURL root_url = CreateURL(FPL(""));
      FileSystemOperation* operation = NewOperation(root_url);

      FileSystemURL url = CreateURL(kFilteringTestCases[i].path);

      std::string test_name = base::StringPrintf(
          "CreateFileAndCreateDirectoryFiltering run %d, test %" PRIuS,
          loop_count, i);
      base::PlatformFileError expectation =
          kFilteringTestCases[i].visible ?
          base::PLATFORM_FILE_OK :
          base::PLATFORM_FILE_ERROR_SECURITY;
      if (kFilteringTestCases[i].is_directory) {
        operation->CreateDirectory(
            url, false, false,
            base::Bind(&ExpectEqHelper, test_name, expectation));
      } else {
        operation->CreateFile(
            url, false, base::Bind(&ExpectEqHelper, test_name, expectation));
      }
      MessageLoop::current()->RunUntilIdle();
    }
  }
}

TEST_F(NativeMediaFileUtilTest, CopySourceFiltering) {
  FilePath dest_path = root_path().AppendASCII("dest");
  FileSystemURL dest_url = CreateURL(FPL("dest"));

  // Run the loop twice. The first run has no source files. The second run does.
  for (int loop_count = 0; loop_count < 2; ++loop_count) {
    if (loop_count == 1) {
      PopulateDirectoryWithTestCases(root_path(),
                                     kFilteringTestCases,
                                     arraysize(kFilteringTestCases));
    }
    for (size_t i = 0; i < arraysize(kFilteringTestCases); ++i) {
      // Always start with an empty destination directory.
      // Copying to a non-empty destination directory is an invalid operation.
      ASSERT_TRUE(file_util::Delete(dest_path, true));
      ASSERT_TRUE(file_util::CreateDirectory(dest_path));

      FileSystemURL root_url = CreateURL(FPL(""));
      FileSystemOperation* operation = NewOperation(root_url);

      FileSystemURL url = CreateURL(kFilteringTestCases[i].path);

      std::string test_name = base::StringPrintf(
          "CopySourceFiltering run %d test %" PRIuS, loop_count, i);
      base::PlatformFileError expectation = base::PLATFORM_FILE_OK;
      if (loop_count == 0 || !kFilteringTestCases[i].visible) {
        // If the source does not exist or is not visible.
        expectation = base::PLATFORM_FILE_ERROR_NOT_FOUND;
      } else if (!kFilteringTestCases[i].is_directory) {
        // Cannot copy a visible file to a directory.
        expectation = base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
      }
      operation->Copy(
          url, dest_url, base::Bind(&ExpectEqHelper, test_name, expectation));
      MessageLoop::current()->RunUntilIdle();
    }
  }
}

TEST_F(NativeMediaFileUtilTest, CopyDestFiltering) {
  // Run the loop twice. The first run has no destination files.
  // The second run does.
  for (int loop_count = 0; loop_count < 2; ++loop_count) {
    if (loop_count == 1) {
      // Reset the test directory between the two loops to remove old
      // directories and create new ones that should pre-exist.
      ASSERT_TRUE(file_util::Delete(root_path(), true));
      ASSERT_TRUE(file_util::CreateDirectory(root_path()));
      PopulateDirectoryWithTestCases(root_path(),
                                     kFilteringTestCases,
                                     arraysize(kFilteringTestCases));
    }

    // Always create a dummy source data file.
    FilePath src_path = root_path().AppendASCII("foo.jpg");
    FileSystemURL src_url = CreateURL(FPL("foo.jpg"));
    static const char kDummyData[] = "dummy";
    ASSERT_TRUE(file_util::WriteFile(src_path, kDummyData, strlen(kDummyData)));

    for (size_t i = 0; i < arraysize(kFilteringTestCases); ++i) {
      if (loop_count == 0 && kFilteringTestCases[i].is_directory) {
        // These directories do not exist in this case, so Copy() will not
        // treat them as directories. Thus invalidating these test cases.
        // Continue now to avoid creating a new |operation| below that goes
        // unused.
        continue;
      }
      FileSystemURL root_url = CreateURL(FPL(""));
      FileSystemOperation* operation = NewOperation(root_url);

      FileSystemURL url = CreateURL(kFilteringTestCases[i].path);

      std::string test_name = base::StringPrintf(
          "CopyDestFiltering run %d test %" PRIuS, loop_count, i);
      base::PlatformFileError expectation;
      if (loop_count == 0) {
        // The destination path is a file here. The directory case has been
        // handled above.
        // If the destination path does not exist and is not visible, then
        // creating it would be a security violation.
        expectation =
            kFilteringTestCases[i].visible ?
            base::PLATFORM_FILE_OK :
            base::PLATFORM_FILE_ERROR_SECURITY;
      } else {
        if (!kFilteringTestCases[i].visible) {
          // If the destination path exist and is not visible, then to the copy
          // operation, it looks like the file needs to be created, which is a
          // security violation.
          expectation = base::PLATFORM_FILE_ERROR_SECURITY;
        } else if (kFilteringTestCases[i].is_directory) {
          // Cannot copy a file to a directory.
          expectation = base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
        } else {
          // Copying from a file to a visible file that exists is ok.
          expectation = base::PLATFORM_FILE_OK;
        }
      }
      operation->Copy(
          src_url, url, base::Bind(&ExpectEqHelper, test_name, expectation));
      MessageLoop::current()->RunUntilIdle();
    }
  }
}

TEST_F(NativeMediaFileUtilTest, MoveSourceFiltering) {
  FilePath dest_path = root_path().AppendASCII("dest");
  FileSystemURL dest_url = CreateURL(FPL("dest"));

  // Run the loop twice. The first run has no source files. The second run does.
  for (int loop_count = 0; loop_count < 2; ++loop_count) {
    if (loop_count == 1) {
      PopulateDirectoryWithTestCases(root_path(),
                                     kFilteringTestCases,
                                     arraysize(kFilteringTestCases));
    }
    for (size_t i = 0; i < arraysize(kFilteringTestCases); ++i) {
      // Always start with an empty destination directory.
      // Moving to a non-empty destination directory is an invalid operation.
      ASSERT_TRUE(file_util::Delete(dest_path, true));
      ASSERT_TRUE(file_util::CreateDirectory(dest_path));

      FileSystemURL root_url = CreateURL(FPL(""));
      FileSystemOperation* operation = NewOperation(root_url);

      FileSystemURL url = CreateURL(kFilteringTestCases[i].path);

      std::string test_name = base::StringPrintf(
          "MoveSourceFiltering run %d test %" PRIuS, loop_count, i);
      base::PlatformFileError expectation = base::PLATFORM_FILE_OK;
      if (loop_count == 0 || !kFilteringTestCases[i].visible) {
        // If the source does not exist or is not visible.
        expectation = base::PLATFORM_FILE_ERROR_NOT_FOUND;
      } else if (!kFilteringTestCases[i].is_directory) {
        // Cannot move a visible file to a directory.
        expectation = base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
      }
      operation->Move(
          url, dest_url, base::Bind(&ExpectEqHelper, test_name, expectation));
      MessageLoop::current()->RunUntilIdle();
    }
  }
}

TEST_F(NativeMediaFileUtilTest, MoveDestFiltering) {
  // Run the loop twice. The first run has no destination files.
  // The second run does.
  for (int loop_count = 0; loop_count < 2; ++loop_count) {
    if (loop_count == 1) {
      // Reset the test directory between the two loops to remove old
      // directories and create new ones that should pre-exist.
      ASSERT_TRUE(file_util::Delete(root_path(), true));
      ASSERT_TRUE(file_util::CreateDirectory(root_path()));
      PopulateDirectoryWithTestCases(root_path(),
                                     kFilteringTestCases,
                                     arraysize(kFilteringTestCases));
    }

    for (size_t i = 0; i < arraysize(kFilteringTestCases); ++i) {
      if (loop_count == 0 && kFilteringTestCases[i].is_directory) {
        // These directories do not exist in this case, so Copy() will not
        // treat them as directories. Thus invalidating these test cases.
        // Continue now to avoid creating a new |operation| below that goes
        // unused.
        continue;
      }

      // Create the source file for every test case because it might get moved.
      FilePath src_path = root_path().AppendASCII("foo.jpg");
      FileSystemURL src_url = CreateURL(FPL("foo.jpg"));
      static const char kDummyData[] = "dummy";
      ASSERT_TRUE(
          file_util::WriteFile(src_path, kDummyData, strlen(kDummyData)));

      FileSystemURL root_url = CreateURL(FPL(""));
      FileSystemOperation* operation = NewOperation(root_url);

      FileSystemURL url = CreateURL(kFilteringTestCases[i].path);

      std::string test_name = base::StringPrintf(
          "MoveDestFiltering run %d test %" PRIuS, loop_count, i);
      base::PlatformFileError expectation;
      if (loop_count == 0) {
        // The destination path is a file here. The directory case has been
        // handled above.
        // If the destination path does not exist and is not visible, then
        // creating it would be a security violation.
        expectation =
            kFilteringTestCases[i].visible ?
            base::PLATFORM_FILE_OK :
            base::PLATFORM_FILE_ERROR_SECURITY;
      } else {
        if (!kFilteringTestCases[i].visible) {
          // If the destination path exist and is not visible, then to the move
          // operation, it looks like the file needs to be created, which is a
          // security violation.
          expectation = base::PLATFORM_FILE_ERROR_SECURITY;
        } else if (kFilteringTestCases[i].is_directory) {
          // Cannot move a file to a directory.
          expectation = base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
        } else {
          // Moving from a file to a visible file that exists is ok.
          expectation = base::PLATFORM_FILE_OK;
        }
      }
      operation->Move(
          src_url, url, base::Bind(&ExpectEqHelper, test_name, expectation));
      MessageLoop::current()->RunUntilIdle();
    }
  }
}

TEST_F(NativeMediaFileUtilTest, GetMetadataFiltering) {
  // Run the loop twice. The first run has no files. The second run does.
  for (int loop_count = 0; loop_count < 2; ++loop_count) {
    if (loop_count == 1) {
      PopulateDirectoryWithTestCases(root_path(),
                                     kFilteringTestCases,
                                     arraysize(kFilteringTestCases));
    }
    for (size_t i = 0; i < arraysize(kFilteringTestCases); ++i) {
      FileSystemURL root_url = CreateURL(FPL(""));
      FileSystemOperation* operation = NewOperation(root_url);

      FileSystemURL url = CreateURL(kFilteringTestCases[i].path);

      std::string test_name = base::StringPrintf(
          "GetMetadataFiltering run %d test %" PRIuS, loop_count, i);
      base::PlatformFileError expectation = base::PLATFORM_FILE_OK;
      if (loop_count == 0 || !kFilteringTestCases[i].visible) {
        // Cannot get metadata from files that do not exist or are not visible.
        expectation = base::PLATFORM_FILE_ERROR_NOT_FOUND;
      }
      operation->GetMetadata(url,
                             base::Bind(&ExpectMetadataEqHelper,
                                        test_name,
                                        expectation,
                                        kFilteringTestCases[i].is_directory));
      MessageLoop::current()->RunUntilIdle();
    }
  }
}

TEST_F(NativeMediaFileUtilTest, RemoveFiltering) {
  // Run the loop twice. The first run has no files. The second run does.
  for (int loop_count = 0; loop_count < 2; ++loop_count) {
    if (loop_count == 1) {
      PopulateDirectoryWithTestCases(root_path(),
                                     kFilteringTestCases,
                                     arraysize(kFilteringTestCases));
    }
    for (size_t i = 0; i < arraysize(kFilteringTestCases); ++i) {
      FileSystemURL root_url = CreateURL(FPL(""));
      FileSystemOperation* operation = NewOperation(root_url);

      FileSystemURL url = CreateURL(kFilteringTestCases[i].path);

      std::string test_name = base::StringPrintf(
          "RemoveFiltering run %d test %" PRIuS, loop_count, i);
      base::PlatformFileError expectation = base::PLATFORM_FILE_OK;
      if (loop_count == 0 || !kFilteringTestCases[i].visible) {
        // Cannot remove files that do not exist or are not visible.
        expectation = base::PLATFORM_FILE_ERROR_NOT_FOUND;
      }
      operation->Remove(
          url, false, base::Bind(&ExpectEqHelper, test_name, expectation));
      MessageLoop::current()->RunUntilIdle();
    }
  }
}

TEST_F(NativeMediaFileUtilTest, TruncateFiltering) {
  // Run the loop twice. The first run has no files. The second run does.
  for (int loop_count = 0; loop_count < 2; ++loop_count) {
    if (loop_count == 1) {
      PopulateDirectoryWithTestCases(root_path(),
                                     kFilteringTestCases,
                                     arraysize(kFilteringTestCases));
    }
    for (size_t i = 0; i < arraysize(kFilteringTestCases); ++i) {
      FileSystemURL root_url = CreateURL(FPL(""));
      FileSystemOperation* operation = NewOperation(root_url);

      FileSystemURL url = CreateURL(kFilteringTestCases[i].path);

      std::string test_name = base::StringPrintf(
          "TruncateFiltering run %d test %" PRIuS, loop_count, i);
      base::PlatformFileError expectation = base::PLATFORM_FILE_OK;
      if (loop_count == 0 || !kFilteringTestCases[i].visible) {
        // Cannot truncate files that do not exist or are not visible.
        expectation = base::PLATFORM_FILE_ERROR_NOT_FOUND;
      } else if (kFilteringTestCases[i].is_directory) {
        // Cannot truncate directories.
        expectation = base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
      }
      operation->Truncate(
          url, 0, base::Bind(&ExpectEqHelper, test_name, expectation));
      MessageLoop::current()->RunUntilIdle();
    }
  }
}

TEST_F(NativeMediaFileUtilTest, TouchFileFiltering) {
  base::Time time = base::Time::Now();

  // Run the loop twice. The first run has no files. The second run does.
  for (int loop_count = 0; loop_count < 2; ++loop_count) {
    if (loop_count == 1) {
      PopulateDirectoryWithTestCases(root_path(),
                                     kFilteringTestCases,
                                     arraysize(kFilteringTestCases));
    }
    for (size_t i = 0; i < arraysize(kFilteringTestCases); ++i) {
      FileSystemURL root_url = CreateURL(FPL(""));
      FileSystemOperation* operation = NewOperation(root_url);

      FileSystemURL url = CreateURL(kFilteringTestCases[i].path);

      std::string test_name = base::StringPrintf(
          "TouchFileFiltering run %d test %" PRIuS, loop_count, i);
      base::PlatformFileError expectation = base::PLATFORM_FILE_OK;
      if (loop_count == 0 || !kFilteringTestCases[i].visible) {
        // Files do not exists. Touch fails.
        expectation = base::PLATFORM_FILE_ERROR_FAILED;
      }
      operation->TouchFile(
          url, time, time, base::Bind(&ExpectEqHelper, test_name, expectation));
      MessageLoop::current()->RunUntilIdle();
    }
  }
}

}  // namespace fileapi
