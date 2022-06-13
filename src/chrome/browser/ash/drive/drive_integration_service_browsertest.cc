// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/drive/drive_integration_service.h"

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace drive {

class DriveIntegrationServiceBrowserTest : public InProcessBrowserTest {
 public:
  bool SetUpUserDataDirectory() override {
    return drive::SetUpUserDataDirectoryForDriveFsTest();
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_drive_integration_service_ = base::BindRepeating(
        &DriveIntegrationServiceBrowserTest::CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_ = std::make_unique<
        drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
        &create_drive_integration_service_);
  }

  drivefs::FakeDriveFs* GetFakeDriveFsForProfile(Profile* profile) {
    return &fake_drivefs_helpers_[profile]->fake_drivefs();
  }

 protected:
  virtual drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath mount_path = profile->GetPath().Append("drivefs");
    fake_drivefs_helpers_[profile] =
        std::make_unique<drive::FakeDriveFsHelper>(profile, mount_path);
    auto* integration_service = new drive::DriveIntegrationService(
        profile, std::string(), mount_path,
        fake_drivefs_helpers_[profile]->CreateFakeDriveFsListenerFactory());
    return integration_service;
  }

 private:
  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
  std::map<Profile*, std::unique_ptr<drive::FakeDriveFsHelper>>
      fake_drivefs_helpers_;
};

// Verify DriveIntegrationService is created during login.
IN_PROC_BROWSER_TEST_F(DriveIntegrationServiceBrowserTest,
                       CreatedDuringLogin) {
  EXPECT_TRUE(DriveIntegrationServiceFactory::FindForProfile(
      browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(DriveIntegrationServiceBrowserTest,
                       ClearCacheAndRemountFileSystem) {
  auto* drive_service =
      DriveIntegrationServiceFactory::FindForProfile(browser()->profile());
  base::FilePath cache_path = drive_service->GetDriveFsHost()->GetDataPath();
  base::FilePath log_folder_path = drive_service->GetDriveFsLogPath().DirName();
  base::FilePath cache_file;
  base::FilePath log_file;

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::CreateDirectory(cache_path));
    ASSERT_TRUE(base::CreateDirectory(log_folder_path));
    ASSERT_TRUE(base::CreateTemporaryFileInDir(cache_path, &cache_file));
    ASSERT_TRUE(base::CreateTemporaryFileInDir(log_folder_path, &log_file));
  }

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  drive_service->ClearCacheAndRemountFileSystem(
      base::BindLambdaForTesting([=](bool success) {
        base::ScopedAllowBlockingForTesting allow_blocking;
        EXPECT_TRUE(success);
        EXPECT_FALSE(base::PathExists(cache_file));
        EXPECT_TRUE(base::PathExists(log_file));
        quit_closure.Run();
      }));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(DriveIntegrationServiceBrowserTest,
                       DisableDrivePolicyTest) {
  // First make sure the pref is set to its default value which should permit
  // drive.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kDisableDrive, false);

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          browser()->profile());

  EXPECT_TRUE(integration_service);
  EXPECT_TRUE(integration_service->is_enabled());

  // ...next try to disable drive.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kDisableDrive, true);

  EXPECT_EQ(integration_service,
            drive::DriveIntegrationServiceFactory::FindForProfile(
                browser()->profile()));
  EXPECT_FALSE(integration_service->is_enabled());
}

IN_PROC_BROWSER_TEST_F(DriveIntegrationServiceBrowserTest,
                       SearchDriveByFileNameTest) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  drive::DriveIntegrationService* drive_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          browser()->profile());

  base::FilePath mount_path = drive_service->GetMountPointPath();
  ASSERT_TRUE(base::WriteFile(mount_path.Append("bar"), ""));
  ASSERT_TRUE(base::WriteFile(mount_path.Append("baz"), ""));
  auto base_time = base::Time::Now() - base::Seconds(10);
  auto earlier_time = base_time - base::Seconds(10);
  ASSERT_TRUE(base::TouchFile(mount_path.Append("bar"), base_time, base_time));
  ASSERT_TRUE(
      base::TouchFile(mount_path.Append("baz"), earlier_time, earlier_time));

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  drive_service->SearchDriveByFileName(
      "ba", 10, drivefs::mojom::QueryParameters::SortField::kLastViewedByMe,
      drivefs::mojom::QueryParameters::SortDirection::kAscending,
      drivefs::mojom::QueryParameters::QuerySource::kLocalOnly,
      base::BindLambdaForTesting(
          [=](FileError error,
              std::vector<drivefs::mojom::QueryItemPtr> items) {
            EXPECT_EQ(2u, items.size());
            EXPECT_EQ("baz", items[0]->path.BaseName().value());
            EXPECT_EQ("bar", items[1]->path.BaseName().value());
            quit_closure.Run();
          }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(DriveIntegrationServiceBrowserTest, GetThumbnailTest) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  drive::DriveIntegrationService* drive_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          browser()->profile());

  base::FilePath mount_path = drive_service->GetMountPointPath();
  ASSERT_TRUE(base::WriteFile(mount_path.Append("bar"), ""));

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  drive_service->GetThumbnail(
      base::FilePath("/bar"), true,
      base::BindLambdaForTesting(
          [=](const absl::optional<std::vector<uint8_t>>& image) {
            ASSERT_FALSE(image.has_value());
            quit_closure.Run();
          }));
  run_loop.Run();
}

class DriveIntegrationServiceWithGaiaDisabledBrowserTest
    : public DriveIntegrationServiceBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kDisableGaiaServices);
  }
};

IN_PROC_BROWSER_TEST_F(DriveIntegrationServiceWithGaiaDisabledBrowserTest,
                       DriveDisabled) {
  // First make sure the pref is set to its default value which would normally
  // permit drive.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kDisableDrive, false);

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          browser()->profile());

  ASSERT_TRUE(integration_service);
  EXPECT_FALSE(integration_service->is_enabled());
}

IN_PROC_BROWSER_TEST_F(DriveIntegrationServiceBrowserTest, GetMetadata) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  auto* drive_service =
      DriveIntegrationServiceFactory::FindForProfile(browser()->profile());

  base::FilePath mount_path = drive_service->GetMountPointPath();
  base::FilePath file_path;
  base::CreateTemporaryFileInDir(mount_path, &file_path);

  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    drive_service->GetMetadata(
        base::FilePath("/foo/bar"),
        base::BindLambdaForTesting(
            [=](FileError error, drivefs::mojom::FileMetadataPtr metadata_ptr) {
              EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
              quit_closure.Run();
            }));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    drive_service->GetMetadata(
        file_path,
        base::BindLambdaForTesting(
            [=](FileError error, drivefs::mojom::FileMetadataPtr metadata_ptr) {
              EXPECT_EQ(FILE_ERROR_OK, error);
              quit_closure.Run();
            }));
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(DriveIntegrationServiceBrowserTest,
                       LocateFilesByItemIds) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  auto* drive_service =
      DriveIntegrationServiceFactory::FindForProfile(browser()->profile());
  drivefs::FakeDriveFs* fake = GetFakeDriveFsForProfile(browser()->profile());

  base::FilePath mount_path = drive_service->GetMountPointPath();
  base::FilePath path;
  base::CreateTemporaryFileInDir(mount_path, &path);
  base::FilePath some_file("/");
  CHECK(mount_path.AppendRelativePath(path, &some_file));
  base::FilePath dir_path;
  base::CreateTemporaryDirInDir(mount_path, "tmp-", &dir_path);
  base::CreateTemporaryFileInDir(dir_path, &path);
  base::FilePath some_other_file("/");
  CHECK(mount_path.AppendRelativePath(path, &some_other_file));

  fake->SetMetadata(some_file, "text/plain", some_file.BaseName().value(),
                    false, false, {}, {}, "abc123");
  fake->SetMetadata(some_other_file, "text/plain", some_file.BaseName().value(),
                    false, false, {}, {}, "qwertyqwerty");

  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    drive_service->LocateFilesByItemIds(
        {"qwertyqwerty", "foobar"},
        base::BindLambdaForTesting(
            [=](absl::optional<std::vector<drivefs::mojom::FilePathOrErrorPtr>>
                    result) {
              ASSERT_EQ(2u, result->size());
              EXPECT_EQ(some_other_file,
                        base::FilePath("/").Append(result->at(0)->get_path()));
              EXPECT_EQ(FILE_ERROR_NOT_FOUND, result->at(1)->get_error());
              quit_closure.Run();
            }));
    run_loop.Run();
  }
}

class DriveIntegrationServiceWithPrefDisabledBrowserTest
    : public DriveIntegrationServiceBrowserTest {
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) override {
    profile->GetPrefs()->SetBoolean(prefs::kDisableDrive, true);
    return DriveIntegrationServiceBrowserTest::CreateDriveIntegrationService(
        profile);
  }
};

IN_PROC_BROWSER_TEST_F(DriveIntegrationServiceWithPrefDisabledBrowserTest,
                       RenableAndDisableDrive) {
  auto* profile = browser()->profile();
  auto* drive_service = DriveIntegrationServiceFactory::FindForProfile(profile);
  EXPECT_FALSE(drive_service->is_enabled());

  profile->GetPrefs()->SetBoolean(prefs::kDisableDrive, false);
  EXPECT_TRUE(drive_service->is_enabled());

  profile->GetPrefs()->SetBoolean(prefs::kDisableDrive, true);
  EXPECT_FALSE(drive_service->is_enabled());
}

}  // namespace drive
