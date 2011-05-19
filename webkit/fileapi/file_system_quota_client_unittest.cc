// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/scoped_temp_dir.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_quota_client.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/quota/quota_types.h"

namespace fileapi {
namespace {

const char kDummyURL1[] = "http://www.dummy.org";
const char kDummyURL2[] = "http://www.example.com";
const char kDummyURL3[] = "http://www.bleh";

// Declared to shorten the variable names.
const quota::StorageType kTemporary = quota::kStorageTypeTemporary;
const quota::StorageType kPersistent = quota::kStorageTypePersistent;
const int kUsageFileSize = FileSystemUsageCache::kUsageFileSize;

class MockFileSystemPathManager : public FileSystemPathManager {
 public:
  explicit MockFileSystemPathManager(const FilePath& filesystem_path)
      : FileSystemPathManager(base::MessageLoopProxy::CreateForCurrentThread(),
                              filesystem_path, NULL, false, true) {}
};

}  // namespace

class FileSystemQuotaClientTest : public testing::Test {
 public:
  FileSystemQuotaClientTest()
      : callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        additional_callback_count_(0),
        deletion_status_(quota::kQuotaStatusUnknown) {
  }

  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    file_system_context_ =
        new FileSystemContext(
            base::MessageLoopProxy::CreateForCurrentThread(),
            base::MessageLoopProxy::CreateForCurrentThread(),
            NULL, NULL,
            FilePath(), false /* is_incognito */,
            false, true,
            new MockFileSystemPathManager(data_dir_.path()));
  }

  struct TestFile {
    bool isDirectory;
    const char* name;
    int64 size;
    const char* origin_url;
    quota::StorageType type;
  };

 protected:
  FileSystemQuotaClient* NewQuotaClient(bool is_incognito) {
    return new FileSystemQuotaClient(
        base::MessageLoopProxy::CreateForCurrentThread(),
        file_system_context_, is_incognito);
  }

  void GetOriginUsageAsync(FileSystemQuotaClient* quota_client,
                           const char* origin_url,
                           quota::StorageType type) {
    quota_client->GetOriginUsage(GURL(origin_url), type,
        callback_factory_.NewCallback(
            &FileSystemQuotaClientTest::OnGetUsage));
  }

  int64 GetOriginUsage(FileSystemQuotaClient* quota_client,
                       const char* origin_url,
                       quota::StorageType type) {
    GetOriginUsageAsync(quota_client, origin_url, type);
    MessageLoop::current()->RunAllPending();
    return usage_;
  }

  const std::set<GURL>& GetOriginsForType(FileSystemQuotaClient* quota_client,
                                          quota::StorageType type) {
    origins_.clear();
    quota_client->GetOriginsForType(type,
        callback_factory_.NewCallback(
            &FileSystemQuotaClientTest::OnGetOrigins));
    MessageLoop::current()->RunAllPending();
    return origins_;
  }

  const std::set<GURL>& GetOriginsForHost(FileSystemQuotaClient* quota_client,
                                          quota::StorageType type,
                                          const char* host) {
    origins_.clear();
    quota_client->GetOriginsForHost(type, host,
        callback_factory_.NewCallback(
            &FileSystemQuotaClientTest::OnGetOrigins));
    MessageLoop::current()->RunAllPending();
    return origins_;
  }

  void RunAdditionalOriginUsageTask(FileSystemQuotaClient* quota_client,
                                    const char* origin_url,
                                    quota::StorageType type) {
    quota_client->GetOriginUsage(GURL(origin_url), type,
        callback_factory_.NewCallback(
            &FileSystemQuotaClientTest::OnGetAdditionalUsage));
  }

  FilePath GetOriginBasePath(const char* origin_url,
                             quota::StorageType type) {
    // Note: this test assumes sandbox_provider impl is used for
    // temporary and persistent filesystem.
    return file_system_context_->path_manager()->sandbox_provider()->
        GetBaseDirectoryForOriginAndType(
            GURL(origin_url), QuotaStorageTypeToFileSystemType(type));
  }

  bool CreateFileSystemDirectory(const char* dir_name,
                                 const char* origin_url,
                                 quota::StorageType type) {
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
                            const char* origin_url,
                            quota::StorageType type) {
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
        ASSERT_TRUE(CreateFileSystemDirectory(
            files[i].name, files[i].origin_url, files[i].type));
      } else {
        ASSERT_TRUE(CreateFileSystemFile(
            files[i].name, files[i].size, files[i].origin_url, files[i].type));
      }
    }
  }

  void DeleteOriginData(FileSystemQuotaClient* quota_client,
                        const char* origin,
                        quota::StorageType type) {
    deletion_status_ = quota::kQuotaStatusUnknown;
    quota_client->DeleteOriginData(
        GURL(origin), type,
        callback_factory_.NewCallback(
            &FileSystemQuotaClientTest::OnDeleteOrigin));
  }

  int64 usage() const { return usage_; }
  quota::QuotaStatusCode status() { return deletion_status_; }
  int additional_callback_count() const { return additional_callback_count_; }
  void set_additional_callback_count(int count) {
    additional_callback_count_ = count;
  }

 private:
  void OnGetUsage(int64 usage) {
    usage_ = usage;
  }

  void OnGetOrigins(const std::set<GURL>& origins) {
    origins_ = origins;
  }

  void OnGetAdditionalUsage(int64 usage_unused) {
    ++additional_callback_count_;
  }

  void OnDeleteOrigin(quota::QuotaStatusCode status) {
    deletion_status_ = status;
  }

  ScopedTempDir data_dir_;
  scoped_refptr<FileSystemContext> file_system_context_;
  base::ScopedCallbackFactory<FileSystemQuotaClientTest> callback_factory_;
  int64 usage_;
  int additional_callback_count_;
  std::set<GURL> origins_;
  quota::QuotaStatusCode deletion_status_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemQuotaClientTest);
};

TEST_F(FileSystemQuotaClientTest, NoFileSystemTest) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));

  EXPECT_EQ(0, GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
}

TEST_F(FileSystemQuotaClientTest, NoFileTest) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const TestFile kFiles[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
  };
  CreateFiles(kFiles, ARRAYSIZE_UNSAFE(kFiles));

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(kUsageFileSize,
        GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemQuotaClientTest, OneFileTest) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const TestFile kFiles[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {false, "foo", 4921, kDummyURL1, kTemporary},
  };
  CreateFiles(kFiles, ARRAYSIZE_UNSAFE(kFiles));

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(4921 + kUsageFileSize,
        GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemQuotaClientTest, TwoFilesTest) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const TestFile kFiles[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {false, "foo", 10310, kDummyURL1, kTemporary},
    {false, "bar", 41, kDummyURL1, kTemporary},
  };
  CreateFiles(kFiles, ARRAYSIZE_UNSAFE(kFiles));

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(10310 + 41 + kUsageFileSize,
        GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemQuotaClientTest, EmptyFilesTest) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const TestFile kFiles[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {false, "foo", 0, kDummyURL1, kTemporary},
    {false, "bar", 0, kDummyURL1, kTemporary},
    {false, "baz", 0, kDummyURL1, kTemporary},
  };
  CreateFiles(kFiles, ARRAYSIZE_UNSAFE(kFiles));

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(kUsageFileSize,
        GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemQuotaClientTest, SubDirectoryTest) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const TestFile kFiles[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {true, "dirtest", 0, kDummyURL1, kTemporary},
    {false, "dirtest/foo", 11921, kDummyURL1, kTemporary},
    {false, "bar", 4814, kDummyURL1, kTemporary},
  };
  CreateFiles(kFiles, ARRAYSIZE_UNSAFE(kFiles));

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(11921 + 4814 + kUsageFileSize,
        GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
  }
}

TEST_F(FileSystemQuotaClientTest, MultiTypeTest) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const TestFile kFiles[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {true, "dirtest", 0, kDummyURL1, kTemporary},
    {false, "dirtest/foo", 133, kDummyURL1, kTemporary},
    {false, "bar", 14, kDummyURL1, kTemporary},
    {true, NULL, 0, kDummyURL1, kPersistent},
    {true, "dirtest", 0, kDummyURL1, kPersistent},
    {false, "dirtest/foo", 193, kDummyURL1, kPersistent},
    {false, "bar", 9, kDummyURL1, kPersistent},
  };
  CreateFiles(kFiles, ARRAYSIZE_UNSAFE(kFiles));

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(133 + 14 + kUsageFileSize,
        GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
    EXPECT_EQ(193 + 9 + kUsageFileSize,
        GetOriginUsage(quota_client.get(), kDummyURL1, kPersistent));
  }
}

TEST_F(FileSystemQuotaClientTest, MultiDomainTest) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const TestFile kFiles[] = {
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
  CreateFiles(kFiles, ARRAYSIZE_UNSAFE(kFiles));

  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(1331 + 134 + kUsageFileSize,
        GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
    EXPECT_EQ(1903 + 19 + kUsageFileSize,
        GetOriginUsage(quota_client.get(), kDummyURL1, kPersistent));
    EXPECT_EQ(1319 + 113 + kUsageFileSize,
        GetOriginUsage(quota_client.get(), kDummyURL2, kTemporary));
    EXPECT_EQ(2013 + 18 + kUsageFileSize,
        GetOriginUsage(quota_client.get(), kDummyURL2, kPersistent));
  }
}

TEST_F(FileSystemQuotaClientTest, GetUsage_MultipleTasks) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const TestFile kFiles[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {false, "foo",   11, kDummyURL1, kTemporary},
    {false, "bar",   22, kDummyURL1, kTemporary},
  };
  CreateFiles(kFiles, ARRAYSIZE_UNSAFE(kFiles));

  // Dispatching three GetUsage tasks.
  set_additional_callback_count(0);
  GetOriginUsageAsync(quota_client.get(), kDummyURL1, kTemporary);
  RunAdditionalOriginUsageTask(quota_client.get(), kDummyURL1, kTemporary);
  RunAdditionalOriginUsageTask(quota_client.get(), kDummyURL1, kTemporary);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(11 + 22 + kUsageFileSize, usage());
  EXPECT_EQ(2, additional_callback_count());

  // Once more, in a different order.
  set_additional_callback_count(0);
  RunAdditionalOriginUsageTask(quota_client.get(), kDummyURL1, kTemporary);
  GetOriginUsageAsync(quota_client.get(), kDummyURL1, kTemporary);
  RunAdditionalOriginUsageTask(quota_client.get(), kDummyURL1, kTemporary);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(11 + 22 + kUsageFileSize, usage());
  EXPECT_EQ(2, additional_callback_count());
}

TEST_F(FileSystemQuotaClientTest, GetOriginsForType) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const TestFile kFiles[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {true, NULL, 0, kDummyURL2, kTemporary},
    {true, NULL, 0, kDummyURL3, kPersistent},
  };
  CreateFiles(kFiles, ARRAYSIZE_UNSAFE(kFiles));

  std::set<GURL> origins = GetOriginsForType(quota_client.get(), kTemporary);
  EXPECT_EQ(2U, origins.size());
  EXPECT_TRUE(origins.find(GURL(kDummyURL1)) != origins.end());
  EXPECT_TRUE(origins.find(GURL(kDummyURL2)) != origins.end());
  EXPECT_TRUE(origins.find(GURL(kDummyURL3)) == origins.end());
}

TEST_F(FileSystemQuotaClientTest, GetOriginsForHost) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const char* kURL1 = "http://foo.com/";
  const char* kURL2 = "https://foo.com/";
  const char* kURL3 = "http://foo.com:1/";
  const char* kURL4 = "http://foo2.com/";
  const char* kURL5 = "http://foo.com:2/";
  const TestFile kFiles[] = {
    {true, NULL, 0, kURL1, kTemporary},
    {true, NULL, 0, kURL2, kTemporary},
    {true, NULL, 0, kURL3, kTemporary},
    {true, NULL, 0, kURL4, kTemporary},
    {true, NULL, 0, kURL5, kPersistent},
  };
  CreateFiles(kFiles, ARRAYSIZE_UNSAFE(kFiles));

  std::set<GURL> origins = GetOriginsForHost(
      quota_client.get(), kTemporary, "foo.com");
  EXPECT_EQ(3U, origins.size());
  EXPECT_TRUE(origins.find(GURL(kURL1)) != origins.end());
  EXPECT_TRUE(origins.find(GURL(kURL2)) != origins.end());
  EXPECT_TRUE(origins.find(GURL(kURL3)) != origins.end());
  EXPECT_TRUE(origins.find(GURL(kURL4)) == origins.end());  // Different host.
  EXPECT_TRUE(origins.find(GURL(kURL5)) == origins.end());  // Different type.
}

TEST_F(FileSystemQuotaClientTest, IncognitoTest) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(true));
  const TestFile kFiles[] = {
    {true, NULL, 0, kDummyURL1, kTemporary},
    {false, "foo", 10, kDummyURL1, kTemporary},
  };
  CreateFiles(kFiles, ARRAYSIZE_UNSAFE(kFiles));

  // Having files in the usual directory wouldn't affect the result
  // queried in incognito mode.
  EXPECT_EQ(0, GetOriginUsage(quota_client.get(), kDummyURL1, kTemporary));
  EXPECT_EQ(0, GetOriginUsage(quota_client.get(), kDummyURL1, kPersistent));

  std::set<GURL> origins = GetOriginsForType(quota_client.get(), kTemporary);
  EXPECT_EQ(0U, origins.size());
  origins = GetOriginsForHost(quota_client.get(), kTemporary, "www.dummy.org");
  EXPECT_EQ(0U, origins.size());
}

TEST_F(FileSystemQuotaClientTest, DeleteOriginTest) {
  scoped_ptr<FileSystemQuotaClient> quota_client(NewQuotaClient(false));
  const TestFile kFiles[] = {
    {true, NULL,  0, "http://foo.com/",  kTemporary},
    {false, "a",  1, "http://foo.com/",  kTemporary},
    {true, NULL,  0, "https://foo.com/", kTemporary},
    {false, "b",  2, "https://foo.com/", kTemporary},
    {true, NULL,  0, "http://foo.com/",  kPersistent},
    {false, "c",  4, "http://foo.com/",  kPersistent},
    {true, NULL,  0, "http://bar.com/",  kTemporary},
    {false, "d",  8, "http://bar.com/",  kTemporary},
    {true, NULL,  0, "http://bar.com/",  kPersistent},
    {false, "e", 16, "http://bar.com/",  kPersistent},
    {true, NULL,  0, "https://bar.com/", kPersistent},
    {false, "f", 32, "https://bar.com/", kPersistent},
    {true, NULL,  0, "https://bar.com/", kTemporary},
    {false, "g", 64, "https://bar.com/", kTemporary},
  };
  CreateFiles(kFiles, ARRAYSIZE_UNSAFE(kFiles));

  DeleteOriginData(quota_client.get(), "http://foo.com/", kTemporary);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(quota::kQuotaStatusOk, status());

  DeleteOriginData(quota_client.get(), "http://bar.com/", kPersistent);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(quota::kQuotaStatusOk, status());

  DeleteOriginData(quota_client.get(), "http://buz.com/", kTemporary);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(quota::kQuotaStatusOk, status());

  EXPECT_EQ(0, GetOriginUsage(
      quota_client.get(), "http://foo.com/", kTemporary));
  EXPECT_EQ(0, GetOriginUsage(
      quota_client.get(), "http://bar.com/", kPersistent));
  EXPECT_EQ(0, GetOriginUsage(
      quota_client.get(), "http://buz.com/", kTemporary));

  EXPECT_EQ(2 + kUsageFileSize,
            GetOriginUsage(quota_client.get(),
                           "https://foo.com/",
                           kTemporary));
  EXPECT_EQ(4 + kUsageFileSize,
            GetOriginUsage(quota_client.get(),
                           "http://foo.com/",
                           kPersistent));
  EXPECT_EQ(8 + kUsageFileSize,
            GetOriginUsage(quota_client.get(),
                           "http://bar.com/",
                           kTemporary));
  EXPECT_EQ(32 + kUsageFileSize,
            GetOriginUsage(quota_client.get(),
                           "https://bar.com/",
                           kPersistent));
  EXPECT_EQ(64 + kUsageFileSize,
            GetOriginUsage(quota_client.get(),
                           "https://bar.com/",
                           kTemporary));
}

}  // namespace fileapi
