// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/chromeos/fileapi/cros_mount_point_provider.h"

#include <set>

#include "base/files/file_path.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "googleurl/src/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/external_mount_points.h"
#include "webkit/fileapi/file_permission_policy.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/quota/mock_special_storage_policy.h"

#define FPL(x) FILE_PATH_LITERAL(x)

using fileapi::ExternalMountPoints;
using fileapi::FileSystemURL;

namespace {

FileSystemURL CreateFileSystemURL(const std::string& extension,
                                  const char* path,
                                  ExternalMountPoints* mount_points) {
  return mount_points->CreateCrackedFileSystemURL(
      GURL("chrome-extension://" + extension + "/"),
      fileapi::kFileSystemTypeExternal,
      base::FilePath::FromUTF8Unsafe(path));
}

TEST(CrosMountPointProviderTest, DefaultMountPoints) {
  scoped_refptr<quota::SpecialStoragePolicy> storage_policy =
      new quota::MockSpecialStoragePolicy();
  scoped_refptr<fileapi::ExternalMountPoints> mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());
  chromeos::CrosMountPointProvider provider(
      storage_policy,
      mount_points.get(),
      fileapi::ExternalMountPoints::GetSystemInstance());
  std::vector<base::FilePath> root_dirs = provider.GetRootDirectories();
  std::set<base::FilePath> root_dirs_set(root_dirs.begin(), root_dirs.end());

  // By default there should be 3 mount points (in system mount points):
  EXPECT_EQ(3u, root_dirs.size());
  EXPECT_TRUE(root_dirs_set.count(
      chromeos::CrosDisksClient::GetRemovableDiskMountPoint()));
  EXPECT_TRUE(root_dirs_set.count(
      chromeos::CrosDisksClient::GetArchiveMountPoint()));
  EXPECT_TRUE(root_dirs_set.count(base::FilePath(FPL("/usr/share/oem"))));
}

TEST(CrosMountPointProviderTest, GetRootDirectories) {
  scoped_refptr<quota::SpecialStoragePolicy> storage_policy =
      new quota::MockSpecialStoragePolicy();
  scoped_refptr<fileapi::ExternalMountPoints> mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());

  scoped_refptr<fileapi::ExternalMountPoints> system_mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());

  chromeos::CrosMountPointProvider provider(
      storage_policy,
      mount_points.get(),
      system_mount_points.get());

  const size_t initial_root_dirs_size = provider.GetRootDirectories().size();

  // Register 'local' test mount points.
  mount_points->RegisterFileSystem("c",
                                   fileapi::kFileSystemTypeNativeLocal,
                                   base::FilePath(FPL("/a/b/c")));
  mount_points->RegisterFileSystem("d",
                                   fileapi::kFileSystemTypeNativeLocal,
                                   base::FilePath(FPL("/b/c/d")));

  // Register system test mount points.
  system_mount_points->RegisterFileSystem("d",
                                          fileapi::kFileSystemTypeNativeLocal,
                                          base::FilePath(FPL("/g/c/d")));
  system_mount_points->RegisterFileSystem("e",
                                          fileapi::kFileSystemTypeNativeLocal,
                                          base::FilePath(FPL("/g/d/e")));

  std::vector<base::FilePath> root_dirs = provider.GetRootDirectories();
  std::set<base::FilePath> root_dirs_set(root_dirs.begin(), root_dirs.end());
  EXPECT_EQ(initial_root_dirs_size + 4, root_dirs.size());
  EXPECT_TRUE(root_dirs_set.count(base::FilePath(FPL("/a/b/c"))));
  EXPECT_TRUE(root_dirs_set.count(base::FilePath(FPL("/b/c/d"))));
  EXPECT_TRUE(root_dirs_set.count(base::FilePath(FPL("/g/c/d"))));
  EXPECT_TRUE(root_dirs_set.count(base::FilePath(FPL("/g/d/e"))));
}

TEST(CrosMountPointProviderTest, AccessPermissions) {
  const int kPermission = fileapi::kReadFilePermissions;

  url_util::AddStandardScheme("chrome-extension");

  scoped_refptr<quota::MockSpecialStoragePolicy> storage_policy =
      new quota::MockSpecialStoragePolicy();
  scoped_refptr<fileapi::ExternalMountPoints> mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());
  scoped_refptr<fileapi::ExternalMountPoints> system_mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());
  chromeos::CrosMountPointProvider provider(
      storage_policy,
      mount_points.get(),
      system_mount_points.get());

  std::string extension("ddammdhioacbehjngdmkjcjbnfginlla");

  storage_policy->AddFileHandler(extension);

  // Initialize mount points.
  ASSERT_TRUE(system_mount_points->RegisterFileSystem(
      "system",
      fileapi::kFileSystemTypeNativeLocal,
      base::FilePath(FPL("/g/system"))));
  ASSERT_TRUE(mount_points->RegisterFileSystem(
      "removable",
      fileapi::kFileSystemTypeNativeLocal,
      base::FilePath(FPL("/media/removable"))));
  ASSERT_TRUE(mount_points->RegisterFileSystem(
      "oem",
      fileapi::kFileSystemTypeRestrictedNativeLocal,
      base::FilePath(FPL("/usr/share/oem"))));

  // Provider specific mount point access.
  EXPECT_EQ(
      fileapi::FILE_PERMISSION_ALWAYS_DENY,
      provider.GetPermissionPolicy(
          CreateFileSystemURL(extension, "removable/foo", mount_points.get()),
          kPermission));

  provider.GrantFileAccessToExtension(extension,
                                      base::FilePath(FPL("removable/foo")));
  EXPECT_EQ(
      fileapi::FILE_PERMISSION_USE_FILE_PERMISSION,
      provider.GetPermissionPolicy(
          CreateFileSystemURL(extension, "removable/foo", mount_points.get()),
          kPermission));
  EXPECT_EQ(
      fileapi::FILE_PERMISSION_ALWAYS_DENY,
      provider.GetPermissionPolicy(
          CreateFileSystemURL(extension, "removable/foo1", mount_points.get()),
          kPermission));

  // System mount point access.
  EXPECT_EQ(
      fileapi::FILE_PERMISSION_ALWAYS_DENY,
      provider.GetPermissionPolicy(
          CreateFileSystemURL(extension, "system/foo",
                              system_mount_points.get()),
          kPermission));

  provider.GrantFileAccessToExtension(extension,
                                      base::FilePath(FPL("system/foo")));
  EXPECT_EQ(
      fileapi::FILE_PERMISSION_USE_FILE_PERMISSION,
      provider.GetPermissionPolicy(
          CreateFileSystemURL(extension, "system/foo",
                              system_mount_points.get()),
          kPermission));
  EXPECT_EQ(
      fileapi::FILE_PERMISSION_ALWAYS_DENY,
      provider.GetPermissionPolicy(
          CreateFileSystemURL(extension, "system/foo1",
                              system_mount_points.get()),
          kPermission));

  // oem is restricted file system.
  provider.GrantFileAccessToExtension(extension, base::FilePath(FPL("oem/foo")));
  // The extension should not be able to access the file even if
  // GrantFileAccessToExtension was called.
  EXPECT_EQ(
      fileapi::FILE_PERMISSION_ALWAYS_DENY,
      provider.GetPermissionPolicy(
          CreateFileSystemURL(extension, "oem/foo", mount_points.get()),
          kPermission));

  provider.GrantFullAccessToExtension(extension);
  // The extension should be able to access restricted file system after it was
  // granted full access.
  EXPECT_EQ(
      fileapi::FILE_PERMISSION_USE_FILE_PERMISSION,
      provider.GetPermissionPolicy(
          CreateFileSystemURL(extension, "oem/foo", mount_points.get()),
          kPermission));
  // The extension which was granted full access should be able to access any
  // path on current file systems.
  EXPECT_EQ(
      fileapi::FILE_PERMISSION_USE_FILE_PERMISSION,
      provider.GetPermissionPolicy(
          CreateFileSystemURL(extension, "removable/foo1", mount_points.get()),
          kPermission));
  EXPECT_EQ(
      fileapi::FILE_PERMISSION_USE_FILE_PERMISSION,
      provider.GetPermissionPolicy(
          CreateFileSystemURL(extension, "system/foo1",
                              system_mount_points.get()),
          kPermission));

  // The extension cannot access new mount points.
  // TODO(tbarzic): This should probably be changed.
  ASSERT_TRUE(mount_points->RegisterFileSystem(
      "test",
      fileapi::kFileSystemTypeNativeLocal,
      base::FilePath(FPL("/foo/test"))));
  EXPECT_EQ(
      fileapi::FILE_PERMISSION_ALWAYS_DENY,
      provider.GetPermissionPolicy(
          CreateFileSystemURL(extension, "test_/foo", mount_points.get()),
          kPermission));

  provider.RevokeAccessForExtension(extension);
  EXPECT_EQ(
      fileapi::FILE_PERMISSION_ALWAYS_DENY,
      provider.GetPermissionPolicy(
          CreateFileSystemURL(extension, "removable/foo", mount_points.get()),
          kPermission));

  fileapi::FileSystemURL internal_url = FileSystemURL::CreateForTest(
      GURL("chrome://foo"),
      fileapi::kFileSystemTypeExternal,
      base::FilePath(FPL("removable/")));
  // Internal WebUI should have full access.
  EXPECT_EQ(
      fileapi::FILE_PERMISSION_ALWAYS_ALLOW,
      provider.GetPermissionPolicy(internal_url, kPermission));
}

TEST(CrosMountPointProvider, GetVirtualPathConflictWithSystemPoints) {
  scoped_refptr<quota::MockSpecialStoragePolicy> storage_policy =
      new quota::MockSpecialStoragePolicy();
  scoped_refptr<fileapi::ExternalMountPoints> mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());
  scoped_refptr<fileapi::ExternalMountPoints> system_mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());
  chromeos::CrosMountPointProvider provider(storage_policy,
      mount_points.get(),
      system_mount_points.get());

  const fileapi::FileSystemType type = fileapi::kFileSystemTypeNativeLocal;

  // Provider specific mount points.
  ASSERT_TRUE(
      mount_points->RegisterFileSystem("b", type, base::FilePath(FPL("/a/b"))));
  ASSERT_TRUE(
      mount_points->RegisterFileSystem("y", type, base::FilePath(FPL("/z/y"))));
  ASSERT_TRUE(
      mount_points->RegisterFileSystem("n", type, base::FilePath(FPL("/m/n"))));

  // System mount points
  ASSERT_TRUE(system_mount_points->RegisterFileSystem(
      "gb", type, base::FilePath(FPL("/a/b"))));
  ASSERT_TRUE(
      system_mount_points->RegisterFileSystem("gz", type, base::FilePath(FPL("/z"))));
  ASSERT_TRUE(system_mount_points->RegisterFileSystem(
       "gp", type, base::FilePath(FPL("/m/n/o/p"))));

  struct TestCase {
    const base::FilePath::CharType* const local_path;
    bool success;
    const base::FilePath::CharType* const virtual_path;
  };

  const TestCase kTestCases[] = {
    // Same paths in both mount points.
    { FPL("/a/b/c/d"), true, FPL("b/c/d") },
    // System mount points path more specific.
    { FPL("/m/n/o/p/r/s"), true, FPL("n/o/p/r/s") },
    // System mount points path less specific.
    { FPL("/z/y/x"), true, FPL("y/x") },
    // Only system mount points path matches.
    { FPL("/z/q/r/s"), true, FPL("gz/q/r/s") },
    // No match.
    { FPL("/foo/xxx"), false, FPL("") },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    // Initialize virtual path with a value.
    base::FilePath virtual_path(FPL("/mount"));
    base::FilePath local_path(kTestCases[i].local_path);
    EXPECT_EQ(kTestCases[i].success,
              provider.GetVirtualPath(local_path, &virtual_path))
        << "Resolving " << kTestCases[i].local_path;

    // There are no guarantees for |virtual_path| value if |GetVirtualPath|
    // fails.
    if (!kTestCases[i].success)
      continue;

    base::FilePath expected_virtual_path(kTestCases[i].virtual_path);
    EXPECT_EQ(expected_virtual_path, virtual_path)
        << "Resolving " << kTestCases[i].local_path;
  }
}

}  // namespace
