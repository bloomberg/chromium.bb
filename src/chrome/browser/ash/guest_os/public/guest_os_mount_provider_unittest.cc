// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider.h"

#include <memory>

#include "ash/components/disks/disk_mount_manager.h"
#include "ash/components/disks/mock_disk_mount_manager.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/cros_disks/cros_disks_client.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::ash::disks::DiskMountManager;
using testing::_;

namespace {

// Creates a new VolumeManager for tests.
// By default, VolumeManager KeyedService is null for testing.
std::unique_ptr<KeyedService> BuildVolumeManager(
    content::BrowserContext* context) {
  return std::make_unique<file_manager::VolumeManager>(
      Profile::FromBrowserContext(context),
      nullptr /* drive_integration_service */,
      nullptr /* power_manager_client */, DiskMountManager::GetInstance(),
      nullptr /* file_system_provider_service */,
      file_manager::VolumeManager::GetMtpStorageInfoCallback());
}
}  // namespace

namespace guest_os {

// TODO(crbug/1293229): This is MockProvider2 because we already have a
// MockProvider and ODR will ruin our day if we have the same name. Should move
// this into a separate file and have both test suites use the same object,
// since both just need a placeholder.
class MockProvider2 : public GuestOsMountProvider {
 public:
  explicit MockProvider2(Profile* profile, crostini::ContainerId container_id)
      : profile_(profile), container_id_(container_id) {}
  std::string DisplayName() override { return "Ptery"; }
  Profile* profile() override { return profile_; }
  Profile* profile_;
  crostini::ContainerId ContainerId() override { return container_id_; }
  // TODO(crbug/1293229): Make ContainerId generic and in guest_os namespace.
  crostini::ContainerId container_id_;
};

class GuestOsMountProviderTest : public testing::Test {
 public:
  GuestOsMountProviderTest() {
    profile_ = std::make_unique<TestingProfile>();
    // DiskMountManager::InitializeForTesting takes ownership and works with
    // a raw pointer, hence the new with no matching delete.
    disk_manager_ = new ash::disks::MockDiskMountManager;
    provider_ = std::make_unique<MockProvider2>(profile_.get(), kContainerId);
    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(), base::BindRepeating(&BuildVolumeManager));

    DiskMountManager::InitializeForTesting(disk_manager_);
    volume_manager_ = file_manager::VolumeManagerFactory::Get(profile_.get());
  }

  GuestOsMountProviderTest(const GuestOsMountProviderTest&) = delete;
  GuestOsMountProviderTest& operator=(const GuestOsMountProviderTest&) = delete;

  ~GuestOsMountProviderTest() override {
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        kMountName);
    // Set an empty factory to shut down our testing factory.
    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(), BrowserContextKeyedServiceFactory::TestingFactory{});
    DiskMountManager::Shutdown();
    provider_.reset();
    profile_.reset();
  }

 protected:
  void NotifyMountEvent(
      const std::string& source_path,
      const std::string& source_format,
      const std::string& mount_label,
      const std::vector<std::string>& mount_options,
      chromeos::MountType type,
      chromeos::MountAccessMode access_mode,
      ash::disks::DiskMountManager::MountPathCallback callback) {
    auto event = DiskMountManager::MountEvent::MOUNTING;
    auto code = chromeos::MountError::MOUNT_ERROR_NONE;
    auto info = DiskMountManager::MountPointInfo(
        "sftp://0:0", "/media/fuse/" + kMountName,
        chromeos::MOUNT_TYPE_NETWORK_STORAGE, ash::disks::MOUNT_CONDITION_NONE);
    disk_manager_->NotifyMountEvent(event, code, info);
    std::move(callback).Run(code, info);
  }

  void ExpectMountCalls(int n) {
    std::vector<std::string> default_mount_options;
    EXPECT_CALL(*disk_manager_,
                MountPath("sftp://0:0", "", kMountName, default_mount_options,
                          chromeos::MOUNT_TYPE_NETWORK_STORAGE,
                          chromeos::MOUNT_ACCESS_MODE_READ_WRITE, _))
        .Times(n)
        .WillRepeatedly(
            Invoke(this, &GuestOsMountProviderTest::NotifyMountEvent));
  }

  // guestos_${UserHash}_${base64(kContainerId.ToString())}. Note that UserHash
  // is an empty string in these tests.
  const crostini::ContainerId kContainerId =
      crostini::ContainerId("cow", "ptery/daccy");
  const std::string kMountName = std::string{"guestos++cow+ptery%2Fdaccy"};

  content::BrowserTaskEnvironment task_environment_;
  ash::disks::MockDiskMountManager* disk_manager_;
  std::unique_ptr<TestingProfile> profile_;
  file_manager::VolumeManager* volume_manager_;
  std::unique_ptr<MockProvider2> provider_;
};

TEST_F(GuestOsMountProviderTest, MountDiskMountsDisk) {
  ExpectMountCalls(1);
  bool result = false;

  provider_->Mount(
      base::BindLambdaForTesting([&result](bool res) { result = res; }));
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(result);
  base::FilePath path;
  EXPECT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          kMountName, &path));
  EXPECT_EQ(base::FilePath("/media/fuse/" + kMountName), path);
  auto volume = volume_manager_->FindVolumeById("guest_os:" + kMountName);
  ASSERT_TRUE(volume);
  EXPECT_EQ(volume->type(), file_manager::VOLUME_TYPE_GUEST_OS);
  EXPECT_EQ(volume->volume_label(), provider_->DisplayName());
}

TEST_F(GuestOsMountProviderTest, MultipleCallsAreQueuedAndOnlyMountOnce) {
  ExpectMountCalls(1);
  int successes = 0;
  provider_->Mount(base::BindLambdaForTesting(
      [&successes](bool result) { successes += result; }));
  provider_->Mount(base::BindLambdaForTesting(
      [&successes](bool result) { successes += result; }));
  task_environment_.RunUntilIdle();
  provider_->Mount(base::BindLambdaForTesting(
      [&successes](bool result) { successes += result; }));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(successes, 3);
  base::FilePath path;
  EXPECT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          kMountName, &path));
  EXPECT_EQ(base::FilePath("/media/fuse/" + kMountName), path);
}

TEST_F(GuestOsMountProviderTest, CanRemountAfterUnmount) {
  ExpectMountCalls(2);
  EXPECT_CALL(*disk_manager_, UnmountPath)
      .WillOnce(testing::Invoke(
          [this](const std::string& mount_path,
                 DiskMountManager::UnmountPathCallback callback) {
            EXPECT_EQ(mount_path, "/media/fuse/" + kMountName);
            std::move(callback).Run(chromeos::MOUNT_ERROR_NONE);
          }));

  provider_->Mount(
      base::BindLambdaForTesting([](bool res) { EXPECT_TRUE(res); }));
  task_environment_.RunUntilIdle();
  provider_->Unmount();
  task_environment_.RunUntilIdle();
  provider_->Mount(
      base::BindLambdaForTesting([](bool res) { EXPECT_TRUE(res); }));
  task_environment_.RunUntilIdle();

  base::FilePath path;
  EXPECT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          kMountName, &path));
  EXPECT_EQ(base::FilePath("/media/fuse/" + kMountName), path);
}
}  // namespace guest_os
