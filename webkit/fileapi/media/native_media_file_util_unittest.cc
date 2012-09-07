// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
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

using namespace fileapi;

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

void ExpectEqHelper(base::PlatformFileError expected,
                    base::PlatformFileError actual) {
  EXPECT_EQ(expected, actual);
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

  IsolatedContext* isolated_context() {
    return IsolatedContext::GetInstance();
  }

  FilePath root_path() {
    return data_dir_.path().Append(FPL("Media Directory"));
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

  ScopedTempDir data_dir_;
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
    FilePath path = root_path().Append(kFilteringTestCases[i].path);
    FileSystemURL url(origin(), type(), path);
    FileSystemOperation* operation = NewOperation(url);

    base::PlatformFileError expectation =
        kFilteringTestCases[i].visible ?
        base::PLATFORM_FILE_OK :
        base::PLATFORM_FILE_ERROR_NOT_FOUND;

    if (kFilteringTestCases[i].is_directory)
      operation->DirectoryExists(url, base::Bind(&ExpectEqHelper, expectation));
    else
      operation->FileExists(url, base::Bind(&ExpectEqHelper, expectation));
    MessageLoop::current()->RunAllPending();
  }
}

TEST_F(NativeMediaFileUtilTest, ReadDirectoryFiltering) {
  PopulateDirectoryWithTestCases(root_path(),
                                 kFilteringTestCases,
                                 arraysize(kFilteringTestCases));

  std::set<FilePath::StringType> content;
  FileSystemURL url(origin(), type(), root_path());
  bool completed = false;
  NewOperation(url)->ReadDirectory(
      url, base::Bind(&DidReadDirectory, &content, &completed));
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(completed);
  EXPECT_EQ(5u, content.size());

  for (size_t i = 0; i < arraysize(kFilteringTestCases); ++i) {
    FilePath::StringType name =
        FilePath(kFilteringTestCases[i].path).BaseName().value();
    std::set<FilePath::StringType>::const_iterator found = content.find(name);
    EXPECT_EQ(kFilteringTestCases[i].visible, found != content.end());
  }
}
