
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/fileapi_util.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/file_manager/mount_test_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/drive/chromeos/drive_test_util.h"
#include "components/drive/chromeos/fake_file_system.h"
#include "components/drive/service/fake_drive_service.h"
#include "components/drive/service/test_util.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_service_manager_context.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace file_manager {
namespace util {
namespace {

// Passes the |result| to the |output| pointer.
void PassFileChooserFileInfoList(FileChooserFileInfoList* output,
                                 FileChooserFileInfoList result) {
  for (const auto& file : result)
    output->push_back(file->Clone());
}

// Creates the drive integration service for the |profile|.
drive::DriveIntegrationService* CreateDriveIntegrationService(
    const base::FilePath temp_dir,
    Profile* profile) {
  drive::FakeDriveService* const drive_service = new drive::FakeDriveService;
  if (!drive::test_util::SetUpTestEntries(drive_service))
    return nullptr;

  return new drive::DriveIntegrationService(
      profile, nullptr, drive_service,
      /* default mount name */ "", temp_dir,
      new drive::test_util::FakeFileSystem(drive_service));
}

TEST(FileManagerFileAPIUtilTest,
     ConvertSelectedFileInfoListToFileChooserFileInfoList) {
  // Prepare the test drive environment.
  content::TestBrowserThreadBundle threads;
  content::TestServiceManagerContext service_manager_context;
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());
  base::ScopedTempDir drive_cache_dir;
  ASSERT_TRUE(drive_cache_dir.CreateUniqueTempDir());
  drive::DriveIntegrationServiceFactory::FactoryCallback factory_callback(
      base::Bind(&CreateDriveIntegrationService, drive_cache_dir.GetPath()));
  drive::DriveIntegrationServiceFactory::ScopedFactoryForTest
      integration_service_factory_scope(&factory_callback);

  // Prepare the test profile.
  Profile* const profile = profile_manager.CreateTestingProfile("test-user");
  drive::DriveIntegrationService* const service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile);
  service->SetEnabled(true);
  test_util::WaitUntilDriveMountPointIsAdded(profile);
  ASSERT_TRUE(service->IsMounted());

  // Obtain the file system context.
  content::StoragePartition* const partition =
      content::BrowserContext::GetStoragePartitionForSite(
          profile, GURL("http://example.com"));
  ASSERT_TRUE(partition);
  storage::FileSystemContext* const context = partition->GetFileSystemContext();
  ASSERT_TRUE(context);

  // Prepare the test input.
  SelectedFileInfoList selected_info_list;

  // Native file.
  {
    ui::SelectedFileInfo info;
    info.file_path = base::FilePath(FILE_PATH_LITERAL("/native/File 1.txt"));
    info.local_path = base::FilePath(FILE_PATH_LITERAL("/native/File 1.txt"));
    info.display_name = "display_name";
    selected_info_list.push_back(info);
  }

  // Non-native file with cache.
  {
    ui::SelectedFileInfo info;
    info.file_path = base::FilePath(
        FILE_PATH_LITERAL("/special/drive-test-user-hash/root/File 1.txt"));
    info.local_path = base::FilePath(FILE_PATH_LITERAL("/native/cache/xxx"));
    info.display_name = "display_name";
    selected_info_list.push_back(info);
  }

  // Non-native file without.
  {
    ui::SelectedFileInfo info;
    info.file_path = base::FilePath(
        FILE_PATH_LITERAL("/special/drive-test-user-hash/root/File 1.txt"));
    selected_info_list.push_back(info);
  }

  // Run the test target.
  FileChooserFileInfoList result;
  ConvertSelectedFileInfoListToFileChooserFileInfoList(
      context, GURL("http://example.com"), selected_info_list,
      base::BindOnce(&PassFileChooserFileInfoList, &result));
  content::RunAllTasksUntilIdle();

  // Check the result.
  ASSERT_EQ(3u, result.size());

  EXPECT_TRUE(result[0]->is_native_file());
  EXPECT_EQ(FILE_PATH_LITERAL("/native/File 1.txt"),
            result[0]->get_native_file()->file_path.value());
  EXPECT_EQ(base::ASCIIToUTF16("display_name"),
            result[0]->get_native_file()->display_name);

  EXPECT_TRUE(result[1]->is_native_file());
  EXPECT_EQ(FILE_PATH_LITERAL("/native/cache/xxx"),
            result[1]->get_native_file()->file_path.value());
  EXPECT_EQ(base::ASCIIToUTF16("display_name"),
            result[1]->get_native_file()->display_name);

  EXPECT_TRUE(result[2]->is_file_system());
  EXPECT_TRUE(result[2]->get_file_system()->url.is_valid());
  const storage::FileSystemURL url =
      context->CrackURL(result[2]->get_file_system()->url);
  EXPECT_EQ(GURL("http://example.com"), url.origin());
  EXPECT_EQ(storage::kFileSystemTypeIsolated, url.mount_type());
  EXPECT_EQ(storage::kFileSystemTypeDrive, url.type());
  EXPECT_EQ(26u, result[2]->get_file_system()->length);
}

}  // namespace
}  // namespace util
}  // namespace file_manager
