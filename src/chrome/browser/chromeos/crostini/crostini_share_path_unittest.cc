// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_share_path.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cicerone_client.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "chromeos/dbus/fake_seneschal_client.h"
#include "chromeos/dbus/seneschal/seneschal_service.pb.h"
#include "components/account_id/account_id.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

class CrostiniSharePathTest : public testing::Test {
 public:
  const bool PERSIST_YES = true;
  const bool PERSIST_NO = false;
  enum class Persist { NO, YES };
  enum class SeneschalClientCalled { NO, YES };
  enum class Success { NO, YES };

  void SharePathCallback(
      const std::string& expected_vm_name,
      Persist expected_persist,
      SeneschalClientCalled expected_seneschal_client_called,
      const vm_tools::seneschal::SharePathRequest::StorageLocation*
          expected_seneschal_storage_location,
      const std::string& expected_seneschal_path,
      Success expected_success,
      const std::string& expected_failure_reason,
      const base::FilePath& container_path,
      bool success,
      std::string failure_reason) {
    const base::DictionaryValue* prefs =
        profile()->GetPrefs()->GetDictionary(prefs::kGuestOSPathsSharedToVms);
    EXPECT_TRUE(prefs->HasKey(shared_path_.value()));
    EXPECT_EQ(prefs->FindKey(shared_path_.value())->GetList().size(), 1U);
    EXPECT_EQ(prefs->FindKey(shared_path_.value())->GetList()[0].GetString(),
              kCrostiniDefaultVmName);
    if (expected_persist == Persist::YES) {
      EXPECT_EQ(prefs->size(), 2U);
      EXPECT_TRUE(prefs->HasKey(share_path_.value()));
      EXPECT_EQ(prefs->FindKey(share_path_.value())->GetList().size(), 1U);
      EXPECT_EQ(prefs->FindKey(share_path_.value())->GetList()[0].GetString(),
                expected_vm_name);
    } else {
      EXPECT_EQ(prefs->size(), 1U);
    }
    EXPECT_EQ(fake_seneschal_client_->share_path_called(),
              expected_seneschal_client_called == SeneschalClientCalled::YES);
    if (expected_seneschal_client_called == SeneschalClientCalled::YES) {
      EXPECT_EQ(
          fake_seneschal_client_->last_share_path_request().storage_location(),
          *expected_seneschal_storage_location);
      EXPECT_EQ(fake_seneschal_client_->last_share_path_request()
                    .shared_path()
                    .path(),
                expected_seneschal_path);
    }
    EXPECT_EQ(success, expected_success == Success::YES);
    EXPECT_EQ(failure_reason, expected_failure_reason);
    run_loop()->Quit();
  }

  void MountEventSharePathCallback(
      const std::string& expected_operation,
      const base::FilePath& expected_path,
      const std::string& expected_vm_name,
      Persist expected_persist,
      SeneschalClientCalled expected_seneschal_client_called,
      const vm_tools::seneschal::SharePathRequest::StorageLocation*
          expected_seneschal_storage_location,
      const std::string& expected_seneschal_path,
      Success expected_success,
      const std::string& expected_failure_reason,
      const std::string& operation,
      const base::FilePath& cros_path,
      const base::FilePath& container_path,
      bool success,
      std::string failure_reason) {
    EXPECT_EQ(expected_operation, operation);
    EXPECT_EQ(expected_path, cros_path);
    SharePathCallback(
        expected_vm_name, expected_persist, expected_seneschal_client_called,
        expected_seneschal_storage_location, expected_seneschal_path,
        expected_success, expected_failure_reason, container_path, success,
        failure_reason);
  }

  void SharePersistedPathsCallback(bool success, std::string failure_reason) {
    EXPECT_TRUE(success);
    EXPECT_EQ(profile()
                  ->GetPrefs()
                  ->GetDictionary(prefs::kGuestOSPathsSharedToVms)
                  ->size(),
              2U);
    run_loop()->Quit();
  }

  void SharePathErrorVmNotRunningCallback(base::OnceClosure closure,
                                          bool success,
                                          std::string failure_reason) {
    EXPECT_FALSE(fake_seneschal_client_->share_path_called());
    EXPECT_EQ(success, false);
    EXPECT_EQ(failure_reason, "Cannot share, VM not running");
    std::move(closure).Run();
  }

  void UnsharePathCallback(
      const base::FilePath& path,
      Persist expected_persist,
      SeneschalClientCalled expected_seneschal_client_called,
      const std::string& expected_seneschal_path,
      Success expected_success,
      const std::string& expected_failure_reason,
      bool success,
      std::string failure_reason) {
    const base::DictionaryValue* prefs =
        profile()->GetPrefs()->GetDictionary(prefs::kGuestOSPathsSharedToVms);
    if (expected_persist == Persist::YES) {
      EXPECT_TRUE(prefs->HasKey(path.value()));
    } else {
      EXPECT_FALSE(prefs->HasKey(path.value()));
    }
    EXPECT_EQ(fake_seneschal_client_->unshare_path_called(),
              expected_seneschal_client_called == SeneschalClientCalled::YES);
    if (expected_seneschal_client_called == SeneschalClientCalled::YES) {
      EXPECT_EQ(fake_seneschal_client_->last_unshare_path_request().path(),
                expected_seneschal_path);
    }
    EXPECT_EQ(success, expected_success == Success::YES);
    EXPECT_EQ(failure_reason, expected_failure_reason);
    run_loop()->Quit();
  }

  void MountEventUnsharePathCallback(
      const std::string expected_operation,
      const base::FilePath& expected_path,
      Persist expected_persist,
      SeneschalClientCalled expected_seneschal_client_called,
      const std::string& expected_seneschal_path,
      Success expected_success,
      const std::string& expected_failure_reason,
      const std::string& operation,
      const base::FilePath& cros_path,
      const base::FilePath& container_path,
      bool success,
      std::string failure_reason) {
    EXPECT_EQ(expected_operation, operation);
    EXPECT_EQ(expected_path, cros_path);
    UnsharePathCallback(cros_path, expected_persist,
                        expected_seneschal_client_called,
                        expected_seneschal_path, expected_success,
                        expected_failure_reason, success, failure_reason);
  }

  CrostiniSharePathTest()
      : test_browser_thread_bundle_(
            content::TestBrowserThreadBundle::REAL_IO_THREAD) {
    chromeos::DBusThreadManager::Initialize();
    fake_concierge_client_ = static_cast<chromeos::FakeConciergeClient*>(
        chromeos::DBusThreadManager::Get()->GetConciergeClient());
    fake_seneschal_client_ = static_cast<chromeos::FakeSeneschalClient*>(
        chromeos::DBusThreadManager::Get()->GetSeneschalClient());
  }

  ~CrostiniSharePathTest() override { chromeos::DBusThreadManager::Shutdown(); }

  void SetUpVolume() {
    // Setup Downloads and path to share, which depend on MyFilesVolume flag,
    // thus can't be on SetUp.
    storage::ExternalMountPoints* mount_points =
        storage::ExternalMountPoints::GetSystemInstance();
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        file_manager::util::GetDownloadsMountPointName(profile()));
    mount_points->RegisterFileSystem(
        file_manager::util::GetDownloadsMountPointName(profile()),
        storage::kFileSystemTypeNativeLocal, storage::FileSystemMountOption(),
        file_manager::util::GetMyFilesFolderForProfile(profile()));
    root_ = file_manager::util::GetMyFilesFolderForProfile(profile());
    share_path_ = root_.Append("path-to-share");
    shared_path_ = root_.Append("already-shared");
    DictionaryPrefUpdate update(profile()->GetPrefs(),
                                prefs::kGuestOSPathsSharedToVms);
    base::DictionaryValue* shared_paths = update.Get();
    base::Value termina(base::Value::Type::LIST);
    termina.GetList().emplace_back(base::Value(kCrostiniDefaultVmName));
    shared_paths->SetKey(shared_path_.value(), std::move(termina));
    volume_downloads_ = file_manager::Volume::CreateForDownloads(root_);
    crostini_share_path_->RegisterSharedPath(kCrostiniDefaultVmName,
                                             shared_path_);
  }

  void SetUp() override {
    run_loop_ = std::make_unique<base::RunLoop>();
    profile_ = std::make_unique<TestingProfile>();
    crostini_share_path_ = CrostiniSharePath::GetForProfile(profile());
    crostini_share_path_->set_no_file_watchers_for_testing();

    // Setup for DriveFS.
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<chromeos::FakeChromeUserManager>());
    account_id_ = AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), "12345");
    GetFakeUserManager()->AddUser(account_id_);
    profile()->GetPrefs()->SetString(drive::prefs::kDriveFsProfileSalt, "a");
    drivefs_ =
        base::FilePath("/media/fuse/drivefs-84675c855b63e12f384d45f033826980");


    // Create 'vm-running' VM instance which is running.
    CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
        "vm-running");
  }

  void TearDown() override {
    run_loop_.reset();
    scoped_user_manager_.reset();
    profile_.reset();
  }

  chromeos::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

 protected:
  base::RunLoop* run_loop() { return run_loop_.get(); }
  Profile* profile() { return profile_.get(); }
  base::FilePath root_;
  base::FilePath share_path_;
  base::FilePath shared_path_;
  base::FilePath drivefs_;
  std::unique_ptr<file_manager::Volume> volume_downloads_;

  // Owned by chromeos::DBusThreadManager
  chromeos::FakeSeneschalClient* fake_seneschal_client_;
  chromeos::FakeConciergeClient* fake_concierge_client_;

  std::unique_ptr<base::RunLoop>
      run_loop_;  // run_loop_ must be created on the UI thread.
  std::unique_ptr<TestingProfile> profile_;
  CrostiniSharePath* crostini_share_path_;
  base::test::ScopedFeatureList features_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  AccountId account_id_;

 private:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  DISALLOW_COPY_AND_ASSIGN(CrostiniSharePathTest);
};

TEST_F(CrostiniSharePathTest, SuccessDownloadsRoot) {
  features_.InitWithFeatures({}, {chromeos::features::kMyFilesVolume});
  SetUpVolume();
  crostini_share_path_->SharePath(
      "vm-running", root_, PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DOWNLOADS, "",
                     Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SuccessMyFilesRoot) {
  features_.InitWithFeatures({chromeos::features::kMyFilesVolume}, {});
  SetUpVolume();
  base::FilePath my_files =
      file_manager::util::GetMyFilesFolderForProfile(profile());
  crostini_share_path_->SharePath(
      "vm-running", my_files, PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::MY_FILES, "",
                     Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SuccessNoPersist) {
  features_.InitWithFeatures({chromeos::features::kMyFilesVolume}, {});
  SetUpVolume();
  crostini_share_path_->SharePath(
      "vm-running", share_path_, PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::MY_FILES,
                     "path-to-share", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SuccessPersist) {
  features_.InitWithFeatures({chromeos::features::kMyFilesVolume}, {});
  SetUpVolume();
  crostini_share_path_->SharePath(
      "vm-running", share_path_, PERSIST_YES,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::YES,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::MY_FILES,
                     "path-to-share", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SuccessDriveFsMyDrive) {
  features_.InitWithFeatures({chromeos::features::kDriveFs}, {});
  SetUpVolume();
  crostini_share_path_->SharePath(
      "vm-running", drivefs_.Append("root").Append("my"), PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DRIVEFS_MY_DRIVE,
                     "my", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, FailureDriveFsDisabled) {
  features_.InitWithFeatures({}, {chromeos::features::kDriveFs});
  SetUpVolume();
  crostini_share_path_->SharePath(
      "vm-running", drivefs_.Append("root").Append("my"), PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "my", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SuccessDriveFsMyDriveRoot) {
  features_.InitWithFeatures({chromeos::features::kDriveFs}, {});
  SetUpVolume();
  crostini_share_path_->SharePath(
      "vm-running", drivefs_.Append("root"), PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DRIVEFS_MY_DRIVE,
                     "", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, FailDriveFsRoot) {
  features_.InitWithFeatures({chromeos::features::kDriveFs}, {});
  SetUpVolume();
  crostini_share_path_->SharePath(
      "vm-running", drivefs_, PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SuccessDriveFsTeamDrives) {
  features_.InitWithFeatures({chromeos::features::kDriveFs}, {});
  SetUpVolume();
  crostini_share_path_->SharePath(
      "vm-running", drivefs_.Append("team_drives").Append("team"), PERSIST_NO,
      base::BindOnce(
          &CrostiniSharePathTest::SharePathCallback, base::Unretained(this),
          "vm-running", Persist::NO, SeneschalClientCalled::YES,
          &vm_tools::seneschal::SharePathRequest::DRIVEFS_TEAM_DRIVES, "team",
          Success::YES, ""));
  run_loop()->Run();
}

// TODO(crbug.com/917920): Enable when DriveFS enforces allowed write paths.
TEST_F(CrostiniSharePathTest, DISABLED_SuccessDriveFsComputersGrandRoot) {
  features_.InitWithFeatures({chromeos::features::kDriveFs}, {});
  SetUpVolume();
  crostini_share_path_->SharePath(
      "vm-running", drivefs_.Append("Computers"), PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DRIVEFS_COMPUTERS,
                     "pc", Success::YES, ""));
  run_loop()->Run();
}

// TODO(crbug.com/917920): Remove when DriveFS enforces allowed write paths.
TEST_F(CrostiniSharePathTest, Bug917920DriveFsComputersGrandRoot) {
  features_.InitWithFeatures({chromeos::features::kDriveFs}, {});
  SetUpVolume();
  crostini_share_path_->SharePath(
      "vm-running", drivefs_.Append("Computers"), PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

// TODO(crbug.com/917920): Enable when DriveFS enforces allowed write paths.
TEST_F(CrostiniSharePathTest, DISABLED_SuccessDriveFsComputerRoot) {
  features_.InitWithFeatures({chromeos::features::kDriveFs}, {});
  SetUpVolume();
  crostini_share_path_->SharePath(
      "vm-running", drivefs_.Append("Computers").Append("pc"), PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DRIVEFS_COMPUTERS,
                     "pc", Success::YES, ""));
  run_loop()->Run();
}

// TODO(crbug.com/917920): Remove when DriveFS enforces allowed write paths.
TEST_F(CrostiniSharePathTest, Bug917920DriveFsComputerRoot) {
  features_.InitWithFeatures({chromeos::features::kDriveFs}, {});
  SetUpVolume();
  crostini_share_path_->SharePath(
      "vm-running", drivefs_.Append("Computers").Append("pc"), PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SuccessDriveFsComputersLevel3) {
  features_.InitWithFeatures({chromeos::features::kDriveFs}, {});
  SetUpVolume();
  crostini_share_path_->SharePath(
      "vm-running",
      drivefs_.Append("Computers").Append("pc").Append("SyncFolder"),
      PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::DRIVEFS_COMPUTERS,
                     "pc/SyncFolder", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, FailDriveFsTrash) {
  features_.InitWithFeatures({chromeos::features::kDriveFs}, {});
  SetUpVolume();
  crostini_share_path_->SharePath(
      "vm-running", drivefs_.Append(".Trash").Append("in-the-trash"),
      PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SuccessRemovable) {
  SetUpVolume();
  crostini_share_path_->SharePath(
      "vm-running", base::FilePath("/media/removable/MyUSB"), PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::REMOVABLE, "MyUSB",
                     Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, FailRemovableRoot) {
  SetUpVolume();
  crostini_share_path_->SharePath(
      "vm-running", base::FilePath("/media/removable"), PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SharePathErrorSeneschal) {
  features_.InitWithFeatures(
      {chromeos::features::kMyFilesVolume, features::kCrostini}, {});
  GetFakeUserManager()->LoginUser(account_id_);
  SetUpVolume();
  vm_tools::concierge::StartVmResponse start_vm_response;
  start_vm_response.set_status(vm_tools::concierge::VM_STATUS_RUNNING);
  start_vm_response.mutable_vm_info()->set_seneschal_server_handle(123);
  fake_concierge_client_->set_start_vm_response(start_vm_response);

  vm_tools::seneschal::SharePathResponse share_path_response;
  share_path_response.set_success(false);
  share_path_response.set_failure_reason("test failure");
  fake_seneschal_client_->set_share_path_response(share_path_response);

  crostini_share_path_->SharePath(
      "error-seneschal", share_path_, PERSIST_YES,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "error-seneschal", Persist::YES,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::MY_FILES,
                     "path-to-share", Success::NO, "test failure"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SharePathErrorPathNotAbsolute) {
  SetUpVolume();
  const base::FilePath path("not/absolute/dir");
  crostini_share_path_->SharePath(
      "vm-running", path, PERSIST_YES,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path must be absolute"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SharePathErrorReferencesParent) {
  SetUpVolume();
  const base::FilePath path("/path/../references/parent");
  crostini_share_path_->SharePath(
      "vm-running", path, PERSIST_NO,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path must be absolute"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SharePathErrorNotUnderDownloads) {
  SetUpVolume();
  const base::FilePath path("/not/under/downloads");
  crostini_share_path_->SharePath(
      "vm-running", path, PERSIST_YES,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-running", Persist::NO,
                     SeneschalClientCalled::NO, nullptr, "", Success::NO,
                     "Path is not allowed"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SharePathVmToBeRestarted) {
  features_.InitWithFeatures(
      {chromeos::features::kMyFilesVolume, features::kCrostini}, {});
  GetFakeUserManager()->LoginUser(account_id_);
  SetUpVolume();
  crostini_share_path_->SharePath(
      "vm-to-be-started", share_path_, PERSIST_YES,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "vm-to-be-started", Persist::YES,
                     SeneschalClientCalled::YES,
                     &vm_tools::seneschal::SharePathRequest::MY_FILES,
                     "path-to-share", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SharePathErrorVmCouldNotBeStarted) {
  SetUpVolume();
  vm_tools::concierge::StartVmResponse start_vm_response;
  start_vm_response.set_status(vm_tools::concierge::VM_STATUS_FAILURE);
  fake_concierge_client_->set_start_vm_response(start_vm_response);

  crostini_share_path_->SharePath(
      "error-vm-could-not-be-started", share_path_, PERSIST_YES,
      base::BindOnce(&CrostiniSharePathTest::SharePathCallback,
                     base::Unretained(this), "error-vm-could-not-be-started",
                     Persist::YES, SeneschalClientCalled::NO, nullptr, "",
                     Success::NO, "VM could not be started"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, SharePersistedPaths) {
  SetUpVolume();
  base::FilePath share_path2_ = root_.AppendASCII("path-to-share-2");
  ASSERT_TRUE(base::CreateDirectory(share_path2_));
  CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
      kCrostiniDefaultVmName);
  base::Value shared_paths(base::Value::Type::DICTIONARY);
  base::Value vms(base::Value::Type::LIST);
  vms.GetList().emplace_back(base::Value(kCrostiniDefaultVmName));
  shared_paths.SetKey(share_path_.value(), std::move(vms));
  base::Value vms2(base::Value::Type::LIST);
  vms2.GetList().emplace_back(base::Value(kCrostiniDefaultVmName));
  shared_paths.SetKey(share_path2_.value(), std::move(vms2));
  profile()->GetPrefs()->Set(prefs::kGuestOSPathsSharedToVms, shared_paths);
  crostini_share_path_->SharePersistedPaths(
      kCrostiniDefaultVmName,
      base::BindOnce(&CrostiniSharePathTest::SharePersistedPathsCallback,
                     base::Unretained(this)));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, RegisterPersistedPaths) {
  base::Value shared_paths(base::Value::Type::DICTIONARY);
  SetUpVolume();
  profile()->GetPrefs()->Set(prefs::kGuestOSPathsSharedToVms, shared_paths);

  crostini_share_path_->RegisterPersistedPath("v1", base::FilePath("/a/a/a"));
  const base::DictionaryValue* prefs =
      profile()->GetPrefs()->GetDictionary(prefs::kGuestOSPathsSharedToVms);
  EXPECT_EQ(prefs->size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList()[0].GetString(), "v1");

  // Adding the same path again for same VM should not cause any changes.
  crostini_share_path_->RegisterPersistedPath("v1", base::FilePath("/a/a/a"));
  prefs = profile()->GetPrefs()->GetDictionary(prefs::kGuestOSPathsSharedToVms);
  EXPECT_EQ(prefs->size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList().size(), 1U);

  // Adding the same path for a new VM adds to the vm list.
  crostini_share_path_->RegisterPersistedPath("v2", base::FilePath("/a/a/a"));
  EXPECT_EQ(prefs->size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList().size(), 2U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList()[0].GetString(), "v1");
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList()[1].GetString(), "v2");

  // Add more paths.
  crostini_share_path_->RegisterPersistedPath("v1", base::FilePath("/a/a/b"));
  crostini_share_path_->RegisterPersistedPath("v1", base::FilePath("/a/a/c"));
  crostini_share_path_->RegisterPersistedPath("v1", base::FilePath("/a/b/a"));
  crostini_share_path_->RegisterPersistedPath("v1", base::FilePath("/b/a/a"));
  EXPECT_EQ(prefs->size(), 5U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList().size(), 2U);
  EXPECT_EQ(prefs->FindKey("/a/a/b")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/a/c")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/b/a")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/b/a/a")->GetList().size(), 1U);

  // Adding /a/a should remove /a/a/a, /a/a/b, /a/a/c.
  crostini_share_path_->RegisterPersistedPath("v1", base::FilePath("/a/a"));
  EXPECT_EQ(prefs->size(), 4U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList()[0].GetString(), "v2");
  EXPECT_EQ(prefs->FindKey("/a/b/a")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/b/a/a")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/a")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/a")->GetList()[0].GetString(), "v1");

  // Adding /a should remove /a/a, /a/b/a.
  crostini_share_path_->RegisterPersistedPath("v1", base::FilePath("/a"));
  EXPECT_EQ(prefs->size(), 3U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList()[0].GetString(), "v2");
  EXPECT_EQ(prefs->FindKey("/b/a/a")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a")->GetList()[0].GetString(), "v1");

  // Adding / should remove all others.
  crostini_share_path_->RegisterPersistedPath("v1", base::FilePath("/"));
  EXPECT_EQ(prefs->size(), 2U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/a/a/a")->GetList()[0].GetString(), "v2");
  EXPECT_EQ(prefs->FindKey("/")->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey("/")->GetList()[0].GetString(), "v1");

  // Add / for v2.
  crostini_share_path_->RegisterPersistedPath("v2", base::FilePath("/"));
  EXPECT_EQ(prefs->size(), 1U);
  EXPECT_EQ(prefs->FindKey("/")->GetList().size(), 2U);
  EXPECT_EQ(prefs->FindKey("/")->GetList()[0].GetString(), "v1");
  EXPECT_EQ(prefs->FindKey("/")->GetList()[1].GetString(), "v2");
}

TEST_F(CrostiniSharePathTest, UnsharePathSuccess) {
  features_.InitWithFeatures({chromeos::features::kMyFilesVolume}, {});
  SetUpVolume();
  DictionaryPrefUpdate update(profile()->GetPrefs(),
                              prefs::kGuestOSPathsSharedToVms);
  base::DictionaryValue* shared_paths = update.Get();
  base::Value vms(base::Value::Type::LIST);
  vms.GetList().emplace_back(base::Value("vm-running"));
  shared_paths->SetKey(shared_path_.value(), std::move(vms));
  crostini_share_path_->UnsharePath(
      "vm-running", shared_path_, true,
      base::BindOnce(&CrostiniSharePathTest::UnsharePathCallback,
                     base::Unretained(this), shared_path_, Persist::NO,
                     SeneschalClientCalled::YES, "MyFiles/already-shared",
                     Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, UnsharePathRoot) {
  features_.InitWithFeatures({chromeos::features::kMyFilesVolume}, {});
  SetUpVolume();
  crostini_share_path_->UnsharePath(
      "vm-running", root_, true,
      base::BindOnce(&CrostiniSharePathTest::UnsharePathCallback,
                     base::Unretained(this), root_, Persist::NO,
                     SeneschalClientCalled::YES, "MyFiles", Success::YES, ""));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, UnsharePathVmNotRunning) {
  SetUpVolume();
  DictionaryPrefUpdate update(profile()->GetPrefs(),
                              prefs::kGuestOSPathsSharedToVms);
  base::DictionaryValue* shared_paths = update.Get();
  base::Value vms(base::Value::Type::LIST);
  vms.GetList().emplace_back(base::Value("vm-not-running"));
  shared_paths->SetKey(shared_path_.value(), std::move(vms));
  crostini_share_path_->UnsharePath(
      "vm-not-running", shared_path_, true,
      base::BindOnce(&CrostiniSharePathTest::UnsharePathCallback,
                     base::Unretained(this), shared_path_, Persist::NO,
                     SeneschalClientCalled::NO, "", Success::YES,
                     "VM not running"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, UnsharePathInvalidPath) {
  SetUpVolume();
  base::FilePath invalid("invalid/path");
  crostini_share_path_->UnsharePath(
      "vm-running", invalid, true,
      base::BindOnce(&CrostiniSharePathTest::UnsharePathCallback,
                     base::Unretained(this), invalid, Persist::NO,
                     SeneschalClientCalled::NO, "", Success::NO,
                     "Invalid path to unshare"));
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, MigratePersistedPathsToMultiVM) {
  SetUpVolume();
  base::ListValue shared_paths = base::ListValue();
  base::FilePath downloads_file = profile()->GetPath().Append("Downloads/file");
  shared_paths.AppendString(downloads_file.value());
  base::FilePath not_downloads("/not/downloads");
  shared_paths.AppendString(not_downloads.value());
  profile()->GetPrefs()->Set(prefs::kCrostiniSharedPaths, shared_paths);
  CrostiniSharePath::MigratePersistedPathsToMultiVM(profile()->GetPrefs());
  EXPECT_EQ(
      profile()->GetPrefs()->GetList(prefs::kCrostiniSharedPaths)->GetSize(),
      0U);
  const base::DictionaryValue* prefs =
      profile()->GetPrefs()->GetDictionary(prefs::kGuestOSPathsSharedToVms);
  EXPECT_EQ(prefs->size(), 2U);
  EXPECT_EQ(prefs->FindKey(downloads_file.value())->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey(downloads_file.value())->GetList()[0].GetString(),
            "termina");
  EXPECT_EQ(prefs->FindKey(not_downloads.value())->GetList().size(), 1U);
  EXPECT_EQ(prefs->FindKey(not_downloads.value())->GetList()[0].GetString(),
            "termina");
}

TEST_F(CrostiniSharePathTest, GetPersistedSharedPaths) {
  SetUpVolume();
  // path1:['vm1'], path2:['vm2'], path3:['vm3'], path12:['vm1','vm2']
  base::Value shared_paths(base::Value::Type::DICTIONARY);

  base::FilePath path1("path1");
  base::Value path1vms(base::Value::Type::LIST);
  path1vms.GetList().emplace_back(base::Value("vm1"));
  shared_paths.SetKey(path1.value(), std::move(path1vms));
  base::FilePath path2("path2");
  base::Value path2vms(base::Value::Type::LIST);
  path2vms.GetList().emplace_back(base::Value("vm2"));
  shared_paths.SetKey(path2.value(), std::move(path2vms));
  base::FilePath path3("path3");
  base::Value path3vms(base::Value::Type::LIST);
  path3vms.GetList().emplace_back(base::Value("vm3"));
  shared_paths.SetKey(path3.value(), std::move(path3vms));
  base::FilePath path12("path12");
  base::Value path12vms(base::Value::Type::LIST);
  path12vms.GetList().emplace_back(base::Value("vm1"));
  path12vms.GetList().emplace_back(base::Value("vm2"));
  shared_paths.SetKey(path12.value(), std::move(path12vms));
  profile()->GetPrefs()->Set(prefs::kGuestOSPathsSharedToVms, shared_paths);

  std::vector<base::FilePath> paths =
      crostini_share_path_->GetPersistedSharedPaths("vm1");
  std::sort(paths.begin(), paths.end());
  EXPECT_EQ(paths.size(), 2U);
  EXPECT_EQ(paths[0], path1);
  EXPECT_EQ(paths[1], path12);

  paths = crostini_share_path_->GetPersistedSharedPaths("vm2");
  std::sort(paths.begin(), paths.end());
  EXPECT_EQ(paths.size(), 2U);
  EXPECT_EQ(paths[0], path12);
  EXPECT_EQ(paths[1], path2);

  paths = crostini_share_path_->GetPersistedSharedPaths("vm3");
  EXPECT_EQ(paths.size(), 1U);
  EXPECT_EQ(paths[0], path3);

  paths = crostini_share_path_->GetPersistedSharedPaths("vm4");
  EXPECT_EQ(paths.size(), 0U);
}

TEST_F(CrostiniSharePathTest, ShareOnMountSuccessParentMount) {
  features_.InitWithFeatures({chromeos::features::kMyFilesVolume}, {});
  SetUpVolume();
  CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
      kCrostiniDefaultVmName);
  crostini_share_path_->set_mount_event_seneschal_callback_for_testing(
      base::BindRepeating(&CrostiniSharePathTest::MountEventSharePathCallback,
                          base::Unretained(this), "share-on-mount",
                          shared_path_, kCrostiniDefaultVmName, Persist::NO,
                          SeneschalClientCalled::YES,
                          &vm_tools::seneschal::SharePathRequest::MY_FILES,
                          "already-shared", Success::YES, ""));
  crostini_share_path_->OnVolumeMounted(chromeos::MountError::MOUNT_ERROR_NONE,
                                        *volume_downloads_);
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, ShareOnMountSuccessSelfMount) {
  features_.InitWithFeatures({chromeos::features::kMyFilesVolume}, {});
  SetUpVolume();
  CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
      kCrostiniDefaultVmName);
  auto volume_shared_path =
      file_manager::Volume::CreateForDownloads(shared_path_);
  crostini_share_path_->set_mount_event_seneschal_callback_for_testing(
      base::BindRepeating(&CrostiniSharePathTest::MountEventSharePathCallback,
                          base::Unretained(this), "share-on-mount",
                          shared_path_, kCrostiniDefaultVmName, Persist::NO,
                          SeneschalClientCalled::YES,
                          &vm_tools::seneschal::SharePathRequest::MY_FILES,
                          "already-shared", Success::YES, ""));
  crostini_share_path_->OnVolumeMounted(chromeos::MountError::MOUNT_ERROR_NONE,
                                        *volume_shared_path);
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, ShareOnMountVmNotRunning) {
  SetUpVolume();

  // Test mount.
  crostini_share_path_->OnVolumeMounted(chromeos::MountError::MOUNT_ERROR_NONE,
                                        *volume_downloads_);
  EXPECT_EQ(fake_seneschal_client_->share_path_called(), false);

  // Test unmount.
  crostini_share_path_->OnVolumeUnmounted(
      chromeos::MountError::MOUNT_ERROR_NONE, *volume_downloads_);
  EXPECT_EQ(fake_seneschal_client_->share_path_called(), false);
}

TEST_F(CrostiniSharePathTest, ShareOnMountVolumeUnrelated) {
  SetUpVolume();
  auto volume_unrelated_ = file_manager::Volume::CreateForDownloads(
      base::FilePath("/unrelated/path"));

  // Test mount.
  crostini_share_path_->OnVolumeMounted(chromeos::MountError::MOUNT_ERROR_NONE,
                                        *volume_unrelated_);
  EXPECT_EQ(fake_seneschal_client_->share_path_called(), false);

  // Test unmount.
  crostini_share_path_->OnVolumeUnmounted(
      chromeos::MountError::MOUNT_ERROR_NONE, *volume_unrelated_);
  EXPECT_EQ(fake_seneschal_client_->share_path_called(), false);
}

TEST_F(CrostiniSharePathTest, UnshareOnUnmountSuccessParentMount) {
  features_.InitWithFeatures({chromeos::features::kMyFilesVolume}, {});
  SetUpVolume();
  CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
      kCrostiniDefaultVmName);
  crostini_share_path_->set_mount_event_seneschal_callback_for_testing(
      base::BindRepeating(&CrostiniSharePathTest::MountEventUnsharePathCallback,
                          base::Unretained(this), "unshare-on-unmount",
                          shared_path_, Persist::YES,
                          SeneschalClientCalled::YES, "MyFiles/already-shared",
                          Success::YES, ""));
  crostini_share_path_->OnVolumeUnmounted(
      chromeos::MountError::MOUNT_ERROR_NONE, *volume_downloads_);
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, UnshareOnUnmountSuccessSelfMount) {
  features_.InitWithFeatures({chromeos::features::kMyFilesVolume}, {});
  SetUpVolume();
  CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
      kCrostiniDefaultVmName);
  auto volume_shared_path =
      file_manager::Volume::CreateForDownloads(shared_path_);
  crostini_share_path_->set_mount_event_seneschal_callback_for_testing(
      base::BindRepeating(&CrostiniSharePathTest::MountEventUnsharePathCallback,
                          base::Unretained(this), "unshare-on-unmount",
                          shared_path_, Persist::YES,
                          SeneschalClientCalled::YES, "MyFiles/already-shared",
                          Success::YES, ""));
  crostini_share_path_->OnVolumeUnmounted(
      chromeos::MountError::MOUNT_ERROR_NONE, *volume_shared_path);
  run_loop()->Run();
}

TEST_F(CrostiniSharePathTest, UnshareOnDelete) {
  features_.InitWithFeatures({chromeos::features::kMyFilesVolume}, {});
  SetUpVolume();
  CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
      kCrostiniDefaultVmName);
  auto volume_shared_path =
      file_manager::Volume::CreateForDownloads(shared_path_);
  crostini_share_path_->set_mount_event_seneschal_callback_for_testing(
      base::BindRepeating(&CrostiniSharePathTest::MountEventUnsharePathCallback,
                          base::Unretained(this), "unshare-on-delete",
                          shared_path_, Persist::NO, SeneschalClientCalled::YES,
                          "MyFiles/already-shared", Success::YES, ""));
  crostini_share_path_->PathDeleted(shared_path_);
  run_loop()->Run();
}

}  // namespace crostini
