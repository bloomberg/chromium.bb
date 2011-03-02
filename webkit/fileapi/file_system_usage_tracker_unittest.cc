// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_usage_tracker.h"

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/scoped_callback_factory.h"
#include "googleurl/src/gurl.h"
#include "base/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_usage_cache.h"

using namespace fileapi;

namespace {

static const GURL kDummyURL1("http://www.dummy.org");
static const GURL kDummyURL2("http://www.example.com");

// Declared to shorten the variable names.
static const fileapi::FileSystemType kTemporary =
    fileapi::kFileSystemTypeTemporary;
static const fileapi::FileSystemType kPersistent =
    fileapi::kFileSystemTypePersistent;
static const int kUsageFileSize = FileSystemUsageCache::kUsageFileSize;

}

class FileSystemUsageTrackerTest : public testing::Test {
 public:
  FileSystemUsageTrackerTest()
      : callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  }

  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
  }

  struct TestFile {
    bool isDirectory;
    const char* name;
    int64 size;
    GURL origin_url;
    fileapi::FileSystemType type;
  };

 protected:
  FileSystemUsageTracker* NewUsageTracker(bool is_incognito) {
    return new FileSystemUsageTracker(
        base::MessageLoopProxy::CreateForCurrentThread(),
        data_dir_.path(), is_incognito);
  }

  int64 GetOriginUsage(FileSystemUsageTracker* tracker,
                       const GURL& origin_url,
                       fileapi::FileSystemType type) {
    tracker->GetOriginUsage(origin_url, type,
        callback_factory_.NewCallback(
            &FileSystemUsageTrackerTest::OnGetUsage));
    MessageLoop::current()->RunAllPending();
    return usage_;
  }

  FilePath GetOriginBasePath(const GURL& origin_url,
                             fileapi::FileSystemType type) {
    return
        FileSystemPathManager::GetFileSystemBaseDirectoryForOriginAndType(
            data_dir_.path().Append(
                FileSystemPathManager::kFileSystemDirectory),
            FileSystemPathManager::GetOriginIdentifierFromURL(origin_url),
            type);
  }

  bool CreateFileSystemDirectory(const char* dir_name,
                                 const GURL& origin_url,
                                 fileapi::FileSystemType type) {
    FilePath origin_base_path = GetOriginBasePath(origin_url, type);
    FilePath dir_path;
    if (dir_name != NULL)
      dir_path = origin_base_path.AppendASCII(dir_name);
    else
      dir_path = origin_base_path;
    if (dir_path.empty())
      return false;

    return file_util::CreateDirectory(dir_path);
  }

  bool CreateFileSystemFile(const char* file_name,
                            int64 file_size,
                            const GURL& origin_url,
                            fileapi::FileSystemType type) {
    FilePath origin_base_path = GetOriginBasePath(origin_url, type);
    FilePath file_path = origin_base_path.AppendASCII(file_name);

    if (file_path.empty())
      return false;

    int file_flags = base::PLATFORM_FILE_CREATE_ALWAYS |
                     base::PLATFORM_FILE_WRITE;
    base::PlatformFileError error_code;
    base::PlatformFile file =
        base::CreatePlatformFile(file_path, file_flags, NULL, &error_code);
    if (error_code != base::PLATFORM_FILE_OK)
      return false;

    bool succeeded;
    succeeded = base::TruncatePlatformFile(file, file_size);
    succeeded = succeeded && base::ClosePlatformFile(file);
    return succeeded;
  }

  void CreateFiles(const TestFile* files, int num_files) {
    for (int i = 0; i < num_files; i++) {
      if (files[i].isDirectory) {
        ASSERT_EQ(true, CreateFileSystemDirectory(
            files[i].name, files[i].origin_url, files[i].type));
      } else {
        ASSERT_EQ(true, CreateFileSystemFile(
            files[i].name, files[i].size, files[i].origin_url, files[i].type));
      }
    }
  }

 private:
  void OnGetUsage(int64 usage) {
    usage_ = usage;
  }

  ScopedTempDir data_dir_;
  base::ScopedCallbackFactory<FileSystemUsageTrackerTest> callback_factory_;
  int64 usage_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemUsageTrackerTest);
};

TEST_F(FileSystemUsageTrackerTest, NoFileSystemTest) {
  scoped_ptr<FileSystemUsageTracker> tracker(NewUsageTracker(false));

  EXPECT_EQ(0,
      GetOriginUsage(tracker.get(), kDummyURL1, kTemporary));
}

TEST_F(FileSystemUsageTrackerTest, NoFileTest) {
  scoped_ptr<FileSystemUsageTracker> tracker(NewUsageTracker(false));
  TestFile files[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
  };
  CreateFiles(files, ARRAYSIZE_UNSAFE(files));

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(kUsageFileSize,
        GetOriginUsage(tracker.get(), kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemUsageTrackerTest, OneFileTest) {
  scoped_ptr<FileSystemUsageTracker> tracker(NewUsageTracker(false));
  TestFile files[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {false, "foo", 4921, kDummyURL1, kTemporary},
  };
  CreateFiles(files, ARRAYSIZE_UNSAFE(files));

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(4921 + kUsageFileSize,
        GetOriginUsage(tracker.get(), kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemUsageTrackerTest, TwoFilesTest) {
  scoped_ptr<FileSystemUsageTracker> tracker(NewUsageTracker(false));
  TestFile files[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {false, "foo", 10310, kDummyURL1, kTemporary},
    {false, "bar", 41, kDummyURL1, kTemporary},
  };
  CreateFiles(files, ARRAYSIZE_UNSAFE(files));

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(10310 + 41 + kUsageFileSize,
        GetOriginUsage(tracker.get(), kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemUsageTrackerTest, EmptyFilesTest) {
  scoped_ptr<FileSystemUsageTracker> tracker(NewUsageTracker(false));
  TestFile files[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {false, "foo", 0, kDummyURL1, kTemporary},
    {false, "bar", 0, kDummyURL1, kTemporary},
    {false, "baz", 0, kDummyURL1, kTemporary},
  };
  CreateFiles(files, ARRAYSIZE_UNSAFE(files));

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(kUsageFileSize,
        GetOriginUsage(tracker.get(), kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemUsageTrackerTest, SubDirectoryTest) {
  scoped_ptr<FileSystemUsageTracker> tracker(NewUsageTracker(false));
  TestFile files[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {true, "dirtest", 0, kDummyURL1, kTemporary},
    {false, "dirtest/foo", 11921, kDummyURL1, kTemporary},
    {false, "bar", 4814, kDummyURL1, kTemporary},
  };
  CreateFiles(files, ARRAYSIZE_UNSAFE(files));

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(11921 + 4814 + kUsageFileSize,
        GetOriginUsage(tracker.get(), kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemUsageTrackerTest, MultiTypeTest) {
  scoped_ptr<FileSystemUsageTracker> tracker(NewUsageTracker(false));
  TestFile files[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {true, "dirtest", 0, kDummyURL1, kTemporary},
    {false, "dirtest/foo", 133, kDummyURL1, kTemporary},
    {false, "bar", 14, kDummyURL1, kTemporary},
    {true, NULL, 0, kDummyURL1, kPersistent},
    {true, "dirtest", 0, kDummyURL1, kPersistent},
    {false, "dirtest/foo", 193, kDummyURL1, kPersistent},
    {false, "bar", 9, kDummyURL1, kPersistent},
  };
  CreateFiles(files, ARRAYSIZE_UNSAFE(files));

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(133 + 14 + kUsageFileSize,
        GetOriginUsage(tracker.get(), kDummyURL1, kTemporary));
    EXPECT_EQ(193 + 9 + kUsageFileSize,
        GetOriginUsage(tracker.get(), kDummyURL1, kPersistent));
  }
}

TEST_F(FileSystemUsageTrackerTest, MultiDomainTest) {
  scoped_ptr<FileSystemUsageTracker> tracker(NewUsageTracker(false));
  TestFile files[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {true, "dir1", 0, kDummyURL1, kTemporary},
    {false, "dir1/foo", 1331, kDummyURL1, kTemporary},
    {false, "bar", 134, kDummyURL1, kTemporary},
    {true, NULL, 0, kDummyURL1, kPersistent},
    {true, "dir2", 0, kDummyURL1, kPersistent},
    {false, "dir2/foo", 1903, kDummyURL1, kPersistent},
    {false, "bar", 19, kDummyURL1, kPersistent},
    {true, NULL, 0, kDummyURL2, kTemporary},
    {true, "dom", 0, kDummyURL2, kTemporary},
    {false, "dom/fan", 1319, kDummyURL2, kTemporary},
    {false, "bar", 113, kDummyURL2, kTemporary},
    {true, NULL, 0, kDummyURL2, kPersistent},
    {true, "dom", 0, kDummyURL2, kPersistent},
    {false, "dom/fan", 2013, kDummyURL2, kPersistent},
    {false, "baz", 18, kDummyURL2, kPersistent},
  };
  CreateFiles(files, ARRAYSIZE_UNSAFE(files));

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(1331 + 134 + kUsageFileSize,
        GetOriginUsage(tracker.get(), kDummyURL1, kTemporary));
    EXPECT_EQ(1903 + 19 + kUsageFileSize,
        GetOriginUsage(tracker.get(), kDummyURL1, kPersistent));
    EXPECT_EQ(1319 + 113 + kUsageFileSize,
        GetOriginUsage(tracker.get(), kDummyURL2, kTemporary));
    EXPECT_EQ(2013 + 18 + kUsageFileSize,
        GetOriginUsage(tracker.get(), kDummyURL2, kPersistent));
  }
}
