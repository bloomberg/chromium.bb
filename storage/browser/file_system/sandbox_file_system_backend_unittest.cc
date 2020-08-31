// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_file_system_backend.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_features.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/sandbox_file_system_backend_delegate.h"
#include "storage/browser/test/test_file_system_options.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "url/gurl.h"
#include "url/origin.h"

// PS stands for path separator.
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
#define PS "\\"
#else
#define PS "/"
#endif

namespace storage {

namespace {

const struct RootPathTest {
  FileSystemType type;
  const char* origin_url;
  const char* expected_path;
} kRootPathTestCases[] = {
    {kFileSystemTypeTemporary, "http://foo:1/", "000" PS "t"},
    {kFileSystemTypePersistent, "http://foo:1/", "000" PS "p"},
    {kFileSystemTypeTemporary, "http://bar.com/", "001" PS "t"},
    {kFileSystemTypePersistent, "http://bar.com/", "001" PS "p"},
    {kFileSystemTypeTemporary, "https://foo:2/", "002" PS "t"},
    {kFileSystemTypePersistent, "https://foo:2/", "002" PS "p"},
    {kFileSystemTypeTemporary, "https://bar.com/", "003" PS "t"},
    {kFileSystemTypePersistent, "https://bar.com/", "003" PS "p"},
};

const struct RootPathFileURITest {
  FileSystemType type;
  const char* origin_url;
  const char* expected_path;
} kRootPathFileURITestCases[] = {
    {kFileSystemTypeTemporary, "file:///", "000" PS "t"},
    {kFileSystemTypePersistent, "file:///", "000" PS "p"}};

void DidOpenFileSystem(base::File::Error* error_out,
                       const GURL& origin_url,
                       const std::string& name,
                       base::File::Error error) {
  *error_out = error;
}

}  // namespace

class SandboxFileSystemBackendTest
    : public testing::Test,
      public ::testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    SetUpNewDelegate(CreateAllowFileAccessOptions());
    if (IsPersistentFileSystemEnabledIncognito()) {
      feature_list_.InitAndEnableFeature(
          features::kEnablePersistentFilesystemInIncognito);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kEnablePersistentFilesystemInIncognito);
    }
  }

  void SetUpNewDelegate(const FileSystemOptions& options) {
    incognito_env_override_ = leveldb_chrome::NewMemEnv("FileSystem");
    delegate_.reset(new SandboxFileSystemBackendDelegate(
        nullptr /* quota_manager_proxy */,
        base::ThreadTaskRunnerHandle::Get().get(), data_dir_.GetPath(),
        nullptr /* special_storage_policy */, options,
        options.is_in_memory() ? incognito_env_override_.get() : nullptr));
  }

  void SetUpNewBackend(const FileSystemOptions& options) {
    SetUpNewDelegate(options);
    backend_.reset(new SandboxFileSystemBackend(delegate_.get()));
  }

  SandboxFileSystemBackendDelegate::OriginEnumerator* CreateOriginEnumerator()
      const {
    return backend_->CreateOriginEnumerator();
  }

  void CreateOriginTypeDirectory(const char* origin_url, FileSystemType type) {
    base::FilePath target = delegate_->GetBaseDirectoryForOriginAndType(
        url::Origin::Create(GURL(origin_url)), type, true);
    ASSERT_TRUE(!target.empty());
    ASSERT_TRUE(base::DirectoryExists(target));
  }

  bool GetRootPath(const char* origin_url,
                   FileSystemType type,
                   OpenFileSystemMode mode,
                   base::FilePath* root_path) {
    base::File::Error error = base::File::FILE_OK;
    backend_->ResolveURL(
        FileSystemURL::CreateForTest(url::Origin::Create(GURL(origin_url)),
                                     type, base::FilePath()),
        mode, base::BindOnce(&DidOpenFileSystem, &error));
    base::RunLoop().RunUntilIdle();
    if (error != base::File::FILE_OK)
      return false;
    base::FilePath returned_root_path =
        delegate_->GetBaseDirectoryForOriginAndType(
            url::Origin::Create(GURL(origin_url)), type,
            /*create=*/false);
    if (root_path)
      *root_path = returned_root_path;
    return !returned_root_path.empty();
  }

  base::FilePath file_system_path() const {
    return data_dir_.GetPath().Append(
        SandboxFileSystemBackendDelegate::kFileSystemDirectory);
  }

  bool IsPersistentFileSystemEnabledIncognito() const { return GetParam(); }

  std::unique_ptr<leveldb::Env> incognito_env_override_;
  base::ScopedTempDir data_dir_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SandboxFileSystemBackendDelegate> delegate_;
  std::unique_ptr<SandboxFileSystemBackend> backend_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, SandboxFileSystemBackendTest, ::testing::Bool());

TEST_P(SandboxFileSystemBackendTest, Empty) {
  SetUpNewBackend(CreateAllowFileAccessOptions());
  std::unique_ptr<SandboxFileSystemBackendDelegate::OriginEnumerator>
      enumerator(CreateOriginEnumerator());
  ASSERT_FALSE(enumerator->Next());
}

TEST_P(SandboxFileSystemBackendTest, EnumerateOrigins) {
  SetUpNewBackend(CreateAllowFileAccessOptions());
  const char* temporary_origins[] = {
      "http://www.bar.com/",       "http://www.foo.com/",
      "http://www.foo.com:1/",     "http://www.example.com:8080/",
      "http://www.google.com:80/",
  };
  const char* persistent_origins[] = {
      "http://www.bar.com/",
      "http://www.foo.com:8080/",
      "http://www.foo.com:80/",
  };
  size_t temporary_size = base::size(temporary_origins);
  size_t persistent_size = base::size(persistent_origins);
  std::set<url::Origin> temporary_set, persistent_set;
  for (size_t i = 0; i < temporary_size; ++i) {
    CreateOriginTypeDirectory(temporary_origins[i], kFileSystemTypeTemporary);
    temporary_set.insert(url::Origin::Create(GURL(temporary_origins[i])));
  }
  for (size_t i = 0; i < persistent_size; ++i) {
    CreateOriginTypeDirectory(persistent_origins[i], kFileSystemTypePersistent);
    persistent_set.insert(url::Origin::Create(GURL(persistent_origins[i])));
  }

  std::unique_ptr<SandboxFileSystemBackendDelegate::OriginEnumerator>
      enumerator(CreateOriginEnumerator());
  size_t temporary_actual_size = 0;
  size_t persistent_actual_size = 0;

  base::Optional<url::Origin> current;
  while ((current = enumerator->Next()).has_value()) {
    SCOPED_TRACE(testing::Message()
                 << "EnumerateOrigin " << current->Serialize());
    if (enumerator->HasFileSystemType(kFileSystemTypeTemporary)) {
      ASSERT_TRUE(temporary_set.find(current.value()) != temporary_set.end());
      ++temporary_actual_size;
    }
    if (enumerator->HasFileSystemType(kFileSystemTypePersistent)) {
      ASSERT_TRUE(persistent_set.find(current.value()) != persistent_set.end());
      ++persistent_actual_size;
    }
  }

  EXPECT_EQ(temporary_size, temporary_actual_size);
  EXPECT_EQ(persistent_size, persistent_actual_size);
}

TEST_P(SandboxFileSystemBackendTest, GetRootPathCreateAndExamine) {
  std::vector<base::FilePath> returned_root_path(
      base::size(kRootPathTestCases));
  SetUpNewBackend(CreateAllowFileAccessOptions());

  // Create a new root directory.
  for (size_t i = 0; i < base::size(kRootPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPath (create) #" << i << " "
                                    << kRootPathTestCases[i].expected_path);

    base::FilePath root_path;
    EXPECT_TRUE(GetRootPath(
        kRootPathTestCases[i].origin_url, kRootPathTestCases[i].type,
        OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT, &root_path));

    base::FilePath expected =
        file_system_path().AppendASCII(kRootPathTestCases[i].expected_path);
    EXPECT_EQ(expected.value(), root_path.value());
    EXPECT_TRUE(base::DirectoryExists(root_path));
    ASSERT_TRUE(returned_root_path.size() > i);
    returned_root_path[i] = root_path;
  }

  // Get the root directory with create=false and see if we get the
  // same directory.
  for (size_t i = 0; i < base::size(kRootPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPath (get) #" << i << " "
                                    << kRootPathTestCases[i].expected_path);

    base::FilePath root_path;
    EXPECT_TRUE(GetRootPath(kRootPathTestCases[i].origin_url,
                            kRootPathTestCases[i].type,
                            OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT, &root_path));
    ASSERT_TRUE(returned_root_path.size() > i);
    EXPECT_EQ(returned_root_path[i].value(), root_path.value());
  }
}

TEST_P(SandboxFileSystemBackendTest,
       GetRootPathCreateAndExamineWithNewBackend) {
  std::vector<base::FilePath> returned_root_path(
      base::size(kRootPathTestCases));
  SetUpNewBackend(CreateAllowFileAccessOptions());

  base::FilePath root_path1;
  EXPECT_TRUE(GetRootPath("http://foo.com:1/", kFileSystemTypeTemporary,
                          OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT, &root_path1));

  SetUpNewBackend(CreateDisallowFileAccessOptions());
  base::FilePath root_path2;
  EXPECT_TRUE(GetRootPath("http://foo.com:1/", kFileSystemTypeTemporary,
                          OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT, &root_path2));

  EXPECT_EQ(root_path1.value(), root_path2.value());
}

TEST_P(SandboxFileSystemBackendTest, GetRootPathGetWithoutCreate) {
  SetUpNewBackend(CreateDisallowFileAccessOptions());

  // Try to get a root directory without creating.
  for (size_t i = 0; i < base::size(kRootPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPath (create=false) #" << i << " "
                                    << kRootPathTestCases[i].expected_path);
    EXPECT_FALSE(GetRootPath(kRootPathTestCases[i].origin_url,
                             kRootPathTestCases[i].type,
                             OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT, nullptr));
  }
}

TEST_P(SandboxFileSystemBackendTest, GetRootPathInIncognito) {
  SetUpNewBackend(CreateIncognitoFileSystemOptions());

  // Try to get a root directory.
  for (size_t i = 0; i < base::size(kRootPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPath (incognito) #" << i << " "
                                    << kRootPathTestCases[i].expected_path);
    EXPECT_EQ(IsPersistentFileSystemEnabledIncognito() ||
                  kRootPathTestCases[i].type == kFileSystemTypeTemporary,
              GetRootPath(kRootPathTestCases[i].origin_url,
                          kRootPathTestCases[i].type,
                          OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT, nullptr));
  }
}

TEST_P(SandboxFileSystemBackendTest, GetRootPathFileURI) {
  SetUpNewBackend(CreateDisallowFileAccessOptions());
  for (size_t i = 0; i < base::size(kRootPathFileURITestCases); ++i) {
    SCOPED_TRACE(testing::Message()
                 << "RootPathFileURI (disallow) #" << i << " "
                 << kRootPathFileURITestCases[i].expected_path);
    EXPECT_FALSE(GetRootPath(kRootPathFileURITestCases[i].origin_url,
                             kRootPathFileURITestCases[i].type,
                             OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT, nullptr));
  }
}

TEST_P(SandboxFileSystemBackendTest, GetRootPathFileURIWithAllowFlag) {
  SetUpNewBackend(CreateAllowFileAccessOptions());
  for (size_t i = 0; i < base::size(kRootPathFileURITestCases); ++i) {
    SCOPED_TRACE(testing::Message()
                 << "RootPathFileURI (allow) #" << i << " "
                 << kRootPathFileURITestCases[i].expected_path);
    base::FilePath root_path;
    EXPECT_TRUE(GetRootPath(kRootPathFileURITestCases[i].origin_url,
                            kRootPathFileURITestCases[i].type,
                            OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                            &root_path));
    base::FilePath expected = file_system_path().AppendASCII(
        kRootPathFileURITestCases[i].expected_path);
    EXPECT_EQ(expected.value(), root_path.value());
    EXPECT_TRUE(base::DirectoryExists(root_path));
  }
}

}  // namespace storage
