// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smbfs_share.h"

#include <utility>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_factory.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_observer.h"
#include "chrome/browser/chromeos/smb_client/smb_url.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/components/smbfs/smbfs_host.h"
#include "chromeos/components/smbfs/smbfs_mounter.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/disks/mount_point.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AllOf;
using testing::Property;
using testing::Unused;

namespace chromeos {
namespace smb_client {
namespace {

constexpr char kSharePath[] = "smb://share/path";
constexpr char kDisplayName[] = "Public";
constexpr char kMountPath[] = "/share/mount/path";
constexpr char kFileName[] = "file_name.ext";

// Creates a new VolumeManager for tests.
// By default, VolumeManager KeyedService is null for testing.
std::unique_ptr<KeyedService> BuildVolumeManager(
    content::BrowserContext* context) {
  return std::make_unique<file_manager::VolumeManager>(
      Profile::FromBrowserContext(context),
      nullptr /* drive_integration_service */,
      nullptr /* power_manager_client */,
      chromeos::disks::DiskMountManager::GetInstance(),
      nullptr /* file_system_provider_service */,
      file_manager::VolumeManager::GetMtpStorageInfoCallback());
}

class MockVolumeManagerObsever : public file_manager::VolumeManagerObserver {
 public:
  MOCK_METHOD(void,
              OnVolumeMounted,
              (chromeos::MountError error_code,
               const file_manager::Volume& volume),
              (override));
  MOCK_METHOD(void,
              OnVolumeUnmounted,
              (chromeos::MountError error_code,
               const file_manager::Volume& volume),
              (override));
};

class MockSmbFsMounter : public smbfs::SmbFsMounter {
 public:
  MOCK_METHOD(void,
              Mount,
              (smbfs::SmbFsMounter::DoneCallback callback),
              (override));
};

class TestSmbFsImpl : public smbfs::mojom::SmbFs {
 public:
  MOCK_METHOD(void,
              RemoveSavedCredentials,
              (RemoveSavedCredentialsCallback),
              (override));

  MOCK_METHOD(void,
              DeleteRecursively,
              (const base::FilePath&, DeleteRecursivelyCallback),
              (override));
};

class SmbFsShareTest : public testing::Test {
 protected:
  void SetUp() override {
    chromeos::disks::DiskMountManager::InitializeForTesting(
        disk_mount_manager_);
    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&BuildVolumeManager));

    file_manager::VolumeManager::Get(&profile_)->AddObserver(&observer_);

    mounter_creation_callback_ = base::BindLambdaForTesting(
        [this](const std::string& share_path, const std::string& mount_dir_name,
               const SmbFsShare::MountOptions& options,
               smbfs::SmbFsHost::Delegate* delegate)
            -> std::unique_ptr<smbfs::SmbFsMounter> {
          EXPECT_EQ(share_path, kSharePath);
          return std::move(mounter_);
        });
  }

  void TearDown() override {
    file_manager::VolumeManager::Get(&profile_)->RemoveObserver(&observer_);
  }

  std::unique_ptr<smbfs::SmbFsHost> CreateSmbFsHost(
      SmbFsShare* share,
      mojo::Receiver<smbfs::mojom::SmbFs>* smbfs_receiver,
      mojo::Remote<smbfs::mojom::SmbFsDelegate>* delegate) {
    return std::make_unique<smbfs::SmbFsHost>(
        std::make_unique<chromeos::disks::MountPoint>(
            base::FilePath(kMountPath), disk_mount_manager_),
        share,
        mojo::Remote<smbfs::mojom::SmbFs>(
            smbfs_receiver->BindNewPipeAndPassRemote()),
        delegate->BindNewPipeAndPassReceiver());
  }

  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::REAL_IO_THREAD,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  file_manager::FakeDiskMountManager* disk_mount_manager_ =
      new file_manager::FakeDiskMountManager;
  TestingProfile profile_;
  MockVolumeManagerObsever observer_;

  SmbFsShare::MounterCreationCallback mounter_creation_callback_;
  std::unique_ptr<MockSmbFsMounter> mounter_ =
      std::make_unique<MockSmbFsMounter>();
  MockSmbFsMounter* raw_mounter_ = mounter_.get();
};

TEST_F(SmbFsShareTest, Mount) {
  TestSmbFsImpl smbfs;
  mojo::Receiver<smbfs::mojom::SmbFs> smbfs_receiver(&smbfs);
  mojo::Remote<smbfs::mojom::SmbFsDelegate> delegate;

  SmbFsShare share(&profile_, SmbUrl(kSharePath), kDisplayName, {});
  share.SetMounterCreationCallbackForTest(mounter_creation_callback_);

  EXPECT_CALL(*raw_mounter_, Mount(_))
      .WillOnce([this, &share, &smbfs_receiver,
                 &delegate](smbfs::SmbFsMounter::DoneCallback callback) {
        std::move(callback).Run(
            smbfs::mojom::MountError::kOk,
            CreateSmbFsHost(&share, &smbfs_receiver, &delegate));
      });
  EXPECT_CALL(
      observer_,
      OnVolumeMounted(
          chromeos::MOUNT_ERROR_NONE,
          AllOf(Property(&file_manager::Volume::type,
                         file_manager::VOLUME_TYPE_SMB),
                Property(&file_manager::Volume::mount_path,
                         base::FilePath(kMountPath)),
                Property(&file_manager::Volume::volume_label, kDisplayName))))
      .Times(1);
  EXPECT_CALL(observer_, OnVolumeUnmounted(
                             chromeos::MOUNT_ERROR_NONE,
                             AllOf(Property(&file_manager::Volume::type,
                                            file_manager::VOLUME_TYPE_SMB),
                                   Property(&file_manager::Volume::mount_path,
                                            base::FilePath(kMountPath)))))
      .Times(1);

  base::RunLoop run_loop;
  share.Mount(base::BindLambdaForTesting([&run_loop](SmbMountResult result) {
    EXPECT_EQ(result, SmbMountResult::kSuccess);
    run_loop.Quit();
  }));
  run_loop.Run();

  EXPECT_TRUE(share.IsMounted());
  EXPECT_EQ(share.share_url().ToString(), kSharePath);
  EXPECT_EQ(share.mount_path(), base::FilePath(kMountPath));

  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  base::FilePath virtual_path;
  EXPECT_TRUE(mount_points->GetVirtualPath(
      base::FilePath(kMountPath).Append(kFileName), &virtual_path));
}

TEST_F(SmbFsShareTest, MountFailure) {
  EXPECT_CALL(*raw_mounter_, Mount(_))
      .WillOnce([](smbfs::SmbFsMounter::DoneCallback callback) {
        std::move(callback).Run(smbfs::mojom::MountError::kTimeout, nullptr);
      });
  EXPECT_CALL(observer_, OnVolumeMounted(chromeos::MOUNT_ERROR_NONE, _))
      .Times(0);
  EXPECT_CALL(observer_, OnVolumeUnmounted(chromeos::MOUNT_ERROR_NONE, _))
      .Times(0);

  SmbFsShare share(&profile_, SmbUrl(kSharePath), kDisplayName, {});
  share.SetMounterCreationCallbackForTest(mounter_creation_callback_);

  base::RunLoop run_loop;
  share.Mount(base::BindLambdaForTesting([&run_loop](SmbMountResult result) {
    EXPECT_EQ(result, SmbMountResult::kAborted);
    run_loop.Quit();
  }));
  run_loop.Run();

  EXPECT_FALSE(share.IsMounted());
  EXPECT_EQ(share.share_url().ToString(), kSharePath);
  EXPECT_EQ(share.mount_path(), base::FilePath());
}

TEST_F(SmbFsShareTest, UnmountOnDisconnect) {
  TestSmbFsImpl smbfs;
  mojo::Receiver<smbfs::mojom::SmbFs> smbfs_receiver(&smbfs);
  mojo::Remote<smbfs::mojom::SmbFsDelegate> delegate;

  SmbFsShare share(&profile_, SmbUrl(kSharePath), kDisplayName, {});
  share.SetMounterCreationCallbackForTest(mounter_creation_callback_);

  EXPECT_CALL(*raw_mounter_, Mount(_))
      .WillOnce([this, &share, &smbfs_receiver,
                 &delegate](smbfs::SmbFsMounter::DoneCallback callback) {
        std::move(callback).Run(
            smbfs::mojom::MountError::kOk,
            CreateSmbFsHost(&share, &smbfs_receiver, &delegate));
      });

  EXPECT_CALL(
      observer_,
      OnVolumeMounted(
          chromeos::MOUNT_ERROR_NONE,
          AllOf(Property(&file_manager::Volume::type,
                         file_manager::VOLUME_TYPE_SMB),
                Property(&file_manager::Volume::mount_path,
                         base::FilePath(kMountPath)),
                Property(&file_manager::Volume::volume_label, kDisplayName))))
      .Times(1);
  base::RunLoop run_loop;
  EXPECT_CALL(observer_, OnVolumeUnmounted(
                             chromeos::MOUNT_ERROR_NONE,
                             AllOf(Property(&file_manager::Volume::type,
                                            file_manager::VOLUME_TYPE_SMB),
                                   Property(&file_manager::Volume::mount_path,
                                            base::FilePath(kMountPath)))))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  share.Mount(
      base::BindLambdaForTesting([&smbfs_receiver](SmbMountResult result) {
        EXPECT_EQ(result, SmbMountResult::kSuccess);

        // Disconnect the Mojo service which should trigger the unmount.
        smbfs_receiver.reset();
      }));
  run_loop.Run();
}

TEST_F(SmbFsShareTest, DisallowCredentialsDialogByDefault) {
  TestSmbFsImpl smbfs;
  mojo::Receiver<smbfs::mojom::SmbFs> smbfs_receiver(&smbfs);
  mojo::Remote<smbfs::mojom::SmbFsDelegate> delegate;

  SmbFsShare share(&profile_, SmbUrl(kSharePath), kDisplayName, {});
  share.SetMounterCreationCallbackForTest(mounter_creation_callback_);

  EXPECT_CALL(*raw_mounter_, Mount(_))
      .WillOnce([this, &share, &smbfs_receiver,
                 &delegate](smbfs::SmbFsMounter::DoneCallback callback) {
        std::move(callback).Run(
            smbfs::mojom::MountError::kOk,
            CreateSmbFsHost(&share, &smbfs_receiver, &delegate));
      });
  EXPECT_CALL(observer_, OnVolumeMounted(chromeos::MOUNT_ERROR_NONE, _))
      .Times(1);
  EXPECT_CALL(observer_, OnVolumeUnmounted(chromeos::MOUNT_ERROR_NONE, _))
      .Times(1);

  {
    base::RunLoop run_loop;
    share.Mount(base::BindLambdaForTesting([&run_loop](SmbMountResult result) {
      EXPECT_EQ(result, SmbMountResult::kSuccess);
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  base::RunLoop run_loop;
  delegate->RequestCredentials(base::BindLambdaForTesting(
      [&run_loop](smbfs::mojom::CredentialsPtr creds) {
        EXPECT_FALSE(creds);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(SmbFsShareTest, DisallowCredentialsDialogAfterTimeout) {
  TestSmbFsImpl smbfs;
  mojo::Receiver<smbfs::mojom::SmbFs> smbfs_receiver(&smbfs);
  mojo::Remote<smbfs::mojom::SmbFsDelegate> delegate;

  SmbFsShare share(&profile_, SmbUrl(kSharePath), kDisplayName, {});
  share.SetMounterCreationCallbackForTest(mounter_creation_callback_);

  EXPECT_CALL(*raw_mounter_, Mount(_))
      .WillOnce([this, &share, &smbfs_receiver,
                 &delegate](smbfs::SmbFsMounter::DoneCallback callback) {
        std::move(callback).Run(
            smbfs::mojom::MountError::kOk,
            CreateSmbFsHost(&share, &smbfs_receiver, &delegate));
      });
  EXPECT_CALL(observer_, OnVolumeMounted(chromeos::MOUNT_ERROR_NONE, _))
      .Times(1);
  EXPECT_CALL(observer_, OnVolumeUnmounted(chromeos::MOUNT_ERROR_NONE, _))
      .Times(1);

  {
    base::RunLoop run_loop;
    share.Mount(base::BindLambdaForTesting([&run_loop](SmbMountResult result) {
      EXPECT_EQ(result, SmbMountResult::kSuccess);
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  share.AllowCredentialsRequest();
  // Fast-forward time for the allow state to timeout. The timeout is 5 seconds,
  // so moving forward by 6 will ensure the timeout runs.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(6));

  base::RunLoop run_loop;
  delegate->RequestCredentials(base::BindLambdaForTesting(
      [&run_loop](smbfs::mojom::CredentialsPtr creds) {
        EXPECT_FALSE(creds);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(SmbFsShareTest, RemoveSavedCredentials) {
  TestSmbFsImpl smbfs;
  mojo::Receiver<smbfs::mojom::SmbFs> smbfs_receiver(&smbfs);
  mojo::Remote<smbfs::mojom::SmbFsDelegate> delegate;

  SmbFsShare share(&profile_, SmbUrl(kSharePath), kDisplayName, {});
  share.SetMounterCreationCallbackForTest(mounter_creation_callback_);

  EXPECT_CALL(*raw_mounter_, Mount(_))
      .WillOnce([this, &share, &smbfs_receiver,
                 &delegate](smbfs::SmbFsMounter::DoneCallback callback) {
        std::move(callback).Run(
            smbfs::mojom::MountError::kOk,
            CreateSmbFsHost(&share, &smbfs_receiver, &delegate));
      });
  EXPECT_CALL(observer_, OnVolumeMounted(chromeos::MOUNT_ERROR_NONE, _))
      .Times(1);
  EXPECT_CALL(observer_, OnVolumeUnmounted(chromeos::MOUNT_ERROR_NONE, _))
      .Times(1);

  {
    base::RunLoop run_loop;
    share.Mount(base::BindLambdaForTesting([&run_loop](SmbMountResult result) {
      EXPECT_EQ(result, SmbMountResult::kSuccess);
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  EXPECT_CALL(smbfs, RemoveSavedCredentials(_))
      .WillOnce(base::test::RunOnceCallback<0>(true /* success */));
  {
    base::RunLoop run_loop;
    share.RemoveSavedCredentials(
        base::BindLambdaForTesting([&run_loop](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

TEST_F(SmbFsShareTest, RemoveSavedCredentials_Disconnect) {
  TestSmbFsImpl smbfs;
  mojo::Receiver<smbfs::mojom::SmbFs> smbfs_receiver(&smbfs);
  mojo::Remote<smbfs::mojom::SmbFsDelegate> delegate;

  SmbFsShare share(&profile_, SmbUrl(kSharePath), kDisplayName, {});
  share.SetMounterCreationCallbackForTest(mounter_creation_callback_);

  EXPECT_CALL(*raw_mounter_, Mount(_))
      .WillOnce([this, &share, &smbfs_receiver,
                 &delegate](smbfs::SmbFsMounter::DoneCallback callback) {
        std::move(callback).Run(
            smbfs::mojom::MountError::kOk,
            CreateSmbFsHost(&share, &smbfs_receiver, &delegate));
      });
  EXPECT_CALL(observer_, OnVolumeMounted(chromeos::MOUNT_ERROR_NONE, _))
      .Times(1);
  EXPECT_CALL(observer_, OnVolumeUnmounted(chromeos::MOUNT_ERROR_NONE, _))
      .Times(1);

  {
    base::RunLoop run_loop;
    share.Mount(base::BindLambdaForTesting([&run_loop](SmbMountResult result) {
      EXPECT_EQ(result, SmbMountResult::kSuccess);
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  EXPECT_CALL(smbfs, RemoveSavedCredentials(_))
      .WillOnce([&smbfs_receiver](Unused) { smbfs_receiver.reset(); });
  {
    base::RunLoop run_loop;
    share.RemoveSavedCredentials(
        base::BindLambdaForTesting([&run_loop](bool success) {
          EXPECT_FALSE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

}  // namespace
}  // namespace smb_client
}  // namespace chromeos
