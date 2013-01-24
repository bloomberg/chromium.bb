// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_context.h"

#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/external_mount_points.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/mock_file_system_options.h"
#include "webkit/quota/mock_quota_manager.h"
#include "webkit/quota/mock_special_storage_policy.h"

#define FPL(x) FILE_PATH_LITERAL(x)

#if defined(FILE_PATH_USES_DRIVE_LETTERS)
#define DRIVE FPL("C:")
#else
#define DRIVE
#endif

namespace fileapi {

namespace {

const char kTestOrigin[] = "http://chromium.org/";
const FilePath::CharType kVirtualPathNoRoot[] = FPL("root/file");

GURL CreateRawFileSystemURL(const std::string& type_str,
                            const std::string& fs_id) {
  std::string url_str = base::StringPrintf(
      "filesystem:http://chromium.org/%s/%s/root/file",
      type_str.c_str(),
      fs_id.c_str());
  return GURL(url_str);
}

class FileSystemContextTest : public testing::Test {
 public:
  FileSystemContextTest() {}

  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());

    scoped_refptr<quota::SpecialStoragePolicy> storage_policy(
        new quota::MockSpecialStoragePolicy());

    mock_quota_manager_ = new quota::MockQuotaManager(
        false /* is_incognito */,
        data_dir_.path(),
        base::MessageLoopProxy::current(),
        base::MessageLoopProxy::current(),
        storage_policy);

    file_system_context_ = new FileSystemContext(
        FileSystemTaskRunners::CreateMockTaskRunners(),
        storage_policy,
        mock_quota_manager_->proxy(),
        data_dir_.path(),
        CreateAllowFileAccessOptions());
  }

 protected:
  FileSystemContext* file_system_context() {
    return file_system_context_.get();
  }

  // Verifies a *valid* filesystem url has expected values.
  void ExpectFileSystemURLMatches(const FileSystemURL& url,
                                  const GURL& expect_origin,
                                  FileSystemType expect_mount_type,
                                  FileSystemType expect_type,
                                  const FilePath& expect_path,
                                  const FilePath& expect_virtual_path,
                                  const std::string& expect_filesystem_id) {
    ASSERT_TRUE(url.is_valid());

    EXPECT_EQ(expect_origin, url.origin());
    EXPECT_EQ(expect_mount_type, url.mount_type());
    EXPECT_EQ(expect_type, url.type());
    EXPECT_EQ(expect_path, url.path());
    EXPECT_EQ(expect_virtual_path, url.virtual_path());
    EXPECT_EQ(expect_filesystem_id, url.filesystem_id());
  }

 private:
  base::ScopedTempDir data_dir_;
  MessageLoop message_loop_;
  scoped_refptr<quota::MockQuotaManager> mock_quota_manager_;
  scoped_refptr<FileSystemContext> file_system_context_;
};

TEST_F(FileSystemContextTest, CrackFileSystemURL) {
  // Register an isolated mount point.
  std::string isolated_file_system_name = "root";
  const std::string kIsolatedFileSystemID =
      IsolatedContext::GetInstance()->RegisterFileSystemForPath(
          kFileSystemTypeNativeLocal,
          FilePath(DRIVE FPL("/test/isolated/root")),
          &isolated_file_system_name);
  // Register system external mount point.
  ASSERT_TRUE(ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      "system",
      kFileSystemTypeDrive,
      FilePath(DRIVE FPL("/test/sys/"))));
  // Register a system external mount point with the same name/id as the
  // registered isolated mount point.
  ASSERT_TRUE(ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      kIsolatedFileSystemID,
      kFileSystemTypeRestrictedNativeLocal,
      FilePath(DRIVE FPL("/test/system/isolated"))));

  struct TestCase {
    // Test case values.
    std::string root;
    std::string type_str;

    // Expected test results.
    bool expect_is_valid;
    FileSystemType expect_mount_type;
    FileSystemType expect_type;
    const FilePath::CharType* expect_path;
    bool expect_virtual_path_empty;
    std::string expect_filesystem_id;
  };

  const TestCase kTestCases[] = {
      // Following should not be handled by the url crackers:
      {
        "pers_mount", "persistent", true /* is_valid */,
        kFileSystemTypePersistent, kFileSystemTypePersistent,
        FPL("pers_mount/root/file"), true /* virtual path empty */,
        std::string()  /* filesystem id */
      },
      {
        "temp_mount", "temporary", true /* is_valid */,
        kFileSystemTypeTemporary, kFileSystemTypeTemporary,
        FPL("temp_mount/root/file"), true /* virtual path empty */,
        std::string()  /* filesystem id */
      },
      // Should be cracked by isolated mount points:
      {
        kIsolatedFileSystemID, "isolated", true /* is_valid */,
        kFileSystemTypeIsolated, kFileSystemTypeNativeLocal,
        DRIVE FPL("/test/isolated/root/file"), false /* virtual path empty */,
        kIsolatedFileSystemID
      },
      // Should be cracked by system mount points:
      {
        "system", "external", true /* is_valid */,
        kFileSystemTypeExternal, kFileSystemTypeDrive,
        DRIVE FPL("/test/sys/root/file"), false /* virtual path empty */,
        "system"
      },
      {
        kIsolatedFileSystemID, "external", true /* is_valid */,
        kFileSystemTypeExternal, kFileSystemTypeRestrictedNativeLocal,
        DRIVE FPL("/test/system/isolated/root/file"),
        false /* virtual path empty */,
        kIsolatedFileSystemID
      },
      // Test for invalid filesystem url (made invalid by adding invalid
      // filesystem type).
      {
        "sytem", "external", false /* is_valid */,
        // The rest of values will be ignored.
        kFileSystemTypeUnknown, kFileSystemTypeUnknown, FPL(""), true,
        std::string()
      },
      // Test for URL with non-existing filesystem id.
      {
        "invalid", "external", false /* is_valid */,
        // The rest of values will be ignored.
        kFileSystemTypeUnknown, kFileSystemTypeUnknown, FPL(""), true,
        std::string()
      },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    const FilePath virtual_path =
        FilePath::FromUTF8Unsafe(kTestCases[i].root).Append(kVirtualPathNoRoot);

    GURL raw_url =
        CreateRawFileSystemURL(kTestCases[i].type_str, kTestCases[i].root);
    FileSystemURL cracked_url = file_system_context()->CrackURL(raw_url);

    SCOPED_TRACE(testing::Message() << "Test case " << i << ": "
                                    << "Cracking URL: " << raw_url);

    EXPECT_EQ(kTestCases[i].expect_is_valid, cracked_url.is_valid());
    if (!kTestCases[i].expect_is_valid)
      continue;

    ExpectFileSystemURLMatches(
        cracked_url,
        GURL(kTestOrigin),
        kTestCases[i].expect_mount_type,
        kTestCases[i].expect_type,
        FilePath(kTestCases[i].expect_path).NormalizePathSeparators(),
        kTestCases[i].expect_virtual_path_empty ?
            FilePath() : virtual_path.NormalizePathSeparators(),
        kTestCases[i].expect_filesystem_id);
  }

  IsolatedContext::GetInstance()->RevokeFileSystemByPath(
      FilePath(DRIVE FPL("/test/isolated/root")));
  ExternalMountPoints::GetSystemInstance()->RevokeFileSystem("system");
  ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
      kIsolatedFileSystemID);
}

}  // namespace

}  // namespace fileapi

