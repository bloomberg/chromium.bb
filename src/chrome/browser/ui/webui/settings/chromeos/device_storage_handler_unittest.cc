// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/test/test_arc_session_manager.h"
#include "chrome/browser/chromeos/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/scoped_set_running_on_chromeos_for_testing.h"
#include "chrome/browser/ui/webui/settings/chromeos/calculator/size_calculator_test_api.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_storage_handler.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/test/fake_arc_session.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/text/bytes_formatting.h"

namespace chromeos {
namespace settings {

namespace {

const char kLsbRelease[] =
    "CHROMEOS_RELEASE_NAME=Chrome OS\n"
    "CHROMEOS_RELEASE_VERSION=1.2.3.4\n";

class TestStorageHandler : public StorageHandler {
 public:
  explicit TestStorageHandler(Profile* profile,
                              content::WebUIDataSource* html_source)
      : StorageHandler(profile, html_source) {}

  // Pull WebUIMessageHandler::set_web_ui() into public so tests can call it.
  using StorageHandler::RoundByteSize;
  using StorageHandler::set_web_ui;
};

class StorageHandlerTest : public testing::Test {
 public:
  StorageHandlerTest() = default;
  ~StorageHandlerTest() override = default;

  void SetUp() override {
    // The storage handler requires an instance of DiskMountManager,
    // ArcServiceManager and ArcSessionManager.
    chromeos::disks::DiskMountManager::InitializeForTesting(
        new file_manager::FakeDiskMountManager);
    arc_service_manager_ = std::make_unique<arc::ArcServiceManager>();
    arc_session_manager_ = arc::CreateTestArcSessionManager(
        std::make_unique<arc::ArcSessionRunner>(
            base::BindRepeating(arc::FakeArcSession::Create)));

    // Initialize profile.
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("p1");

    // Initialize storage handler.
    content::WebUIDataSource* html_source =
        content::WebUIDataSource::Create(chrome::kChromeUIOSSettingsHost);
    handler_ = std::make_unique<TestStorageHandler>(profile_, html_source);
    handler_->set_web_ui(&web_ui_);
    handler_->AllowJavascriptForTesting();

    // Initialize tests APIs.
    size_stat_test_api_ = std::make_unique<calculator::SizeStatTestAPI>(
        handler_.get(), new calculator::SizeStatCalculator(profile_));
    my_files_size_test_api_ = std::make_unique<calculator::MyFilesSizeTestAPI>(
        handler_.get(), new calculator::MyFilesSizeCalculator(profile_));
    browsing_data_size_test_api_ =
        std::make_unique<calculator::BrowsingDataSizeTestAPI>(
            handler_.get(),
            new calculator::BrowsingDataSizeCalculator(profile_));
    apps_size_test_api_ = std::make_unique<calculator::AppsSizeTestAPI>(
        handler_.get(), new calculator::AppsSizeCalculator(profile_));
    crostini_size_test_api_ = std::make_unique<calculator::CrostiniSizeTestAPI>(
        handler_.get(), new calculator::CrostiniSizeCalculator(profile_));
    other_users_size_test_api_ =
        std::make_unique<calculator::OtherUsersSizeTestAPI>(
            handler_.get(), new calculator::OtherUsersSizeCalculator());

    // Create and register My files directory.
    // By emulating chromeos running, GetMyFilesFolderForProfile will return the
    // profile's temporary location instead of $HOME/Downloads.
    chromeos::ScopedSetRunningOnChromeOSForTesting fake_release(kLsbRelease,
                                                                base::Time());
    const base::FilePath my_files_path =
        file_manager::util::GetMyFilesFolderForProfile(profile_);
    CHECK(base::CreateDirectory(my_files_path));
    CHECK(storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        file_manager::util::GetDownloadsMountPointName(profile_),
        storage::kFileSystemTypeNativeLocal, storage::FileSystemMountOption(),
        my_files_path));
  }

  void TearDown() override {
    handler_.reset();
    size_stat_test_api_.reset();
    my_files_size_test_api_.reset();
    browsing_data_size_test_api_.reset();
    apps_size_test_api_.reset();
    crostini_size_test_api_.reset();
    other_users_size_test_api_.reset();
    chromeos::disks::DiskMountManager::Shutdown();
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
  }

 protected:
  // From a given amount of total size and available size as input, returns the
  // space state determined by the OnGetSizeState function.
  int GetSpaceState(int64_t* total_size, int64_t* available_size) {
    size_stat_test_api_->SimulateOnGetSizeStat(total_size, available_size);
    task_environment_.RunUntilIdle();
    const base::Value* dictionary =
        GetWebUICallbackMessage("storage-size-stat-changed");
    EXPECT_TRUE(dictionary) << "No 'storage-size-stat-changed' callback";
    int space_state = dictionary->FindKey("spaceState")->GetInt();
    return space_state;
  }

  // Expects a callback message with a given |event_name|. A non null
  // base::Value is returned if the callback message is found and has associated
  // data.
  const base::Value* GetWebUICallbackMessage(const std::string& event_name) {
    for (auto it = web_ui_.call_data().rbegin();
         it != web_ui_.call_data().rend(); ++it) {
      const content::TestWebUI::CallData* data = it->get();
      std::string name;
      if (data->function_name() != "cr.webUIListenerCallback" ||
          !data->arg1()->GetAsString(&name)) {
        continue;
      }
      if (name == event_name)
        return data->arg2();
    }
    return nullptr;
  }

  // Get the path to file manager's test data directory.
  base::FilePath GetTestDataFilePath(const std::string& file_name) {
    // Get the path to file manager's test data directory.
    base::FilePath source_dir;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_dir));
    base::FilePath test_data_dir = source_dir.AppendASCII("chrome")
                                       .AppendASCII("test")
                                       .AppendASCII("data")
                                       .AppendASCII("chromeos")
                                       .AppendASCII("file_manager");

    // Return full test data path to the given |file_name|.
    return test_data_dir.Append(base::FilePath::FromUTF8Unsafe(file_name));
  }

  // Copy a file from the file manager's test data directory to the specified
  // target_path.
  void AddFile(const std::string& file_name,
               int64_t expected_size,
               base::FilePath target_path) {
    const base::FilePath entry_path = GetTestDataFilePath(file_name);
    target_path = target_path.AppendASCII(file_name);
    ASSERT_TRUE(base::CopyFile(entry_path, target_path))
        << "Copy from " << entry_path.value() << " to " << target_path.value()
        << " failed.";
    // Verify file size.
    base::stat_wrapper_t stat;
    const int res = base::File::Lstat(target_path.value().c_str(), &stat);
    ASSERT_FALSE(res < 0) << "Couldn't stat" << target_path.value();
    ASSERT_EQ(expected_size, stat.st_size);
  }

  std::unique_ptr<TestStorageHandler> handler_;
  content::TestWebUI web_ui_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  Profile* profile_;
  std::unique_ptr<calculator::SizeStatTestAPI> size_stat_test_api_;
  std::unique_ptr<calculator::MyFilesSizeTestAPI> my_files_size_test_api_;
  std::unique_ptr<calculator::BrowsingDataSizeTestAPI>
      browsing_data_size_test_api_;
  std::unique_ptr<calculator::AppsSizeTestAPI> apps_size_test_api_;
  std::unique_ptr<calculator::CrostiniSizeTestAPI> crostini_size_test_api_;
  std::unique_ptr<calculator::OtherUsersSizeTestAPI> other_users_size_test_api_;

 private:
  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;
  DISALLOW_COPY_AND_ASSIGN(StorageHandlerTest);
};

TEST_F(StorageHandlerTest, RoundByteSize) {
  static const struct {
    int64_t bytes;
    const char* expected;
  } cases[] = {
      {0, "0 B"},
      {3, "4 B"},
      {4, "4 B"},
      {5, "8 B"},
      {8 * 1024 - 1, "8.0 KB"},
      {8 * 1024, "8.0 KB"},
      {8 * 1024 + 1, "16.0 KB"},
      {31 * 1024 * 1024, "32.0 MB"},
      {32 * 1024 * 1024, "32.0 MB"},
      {50 * 1024 * 1024, "64.0 MB"},
      {65LL * 1024 * 1024 * 1024, "128 GB"},
      {130LL * 1024 * 1024 * 1024, "256 GB"},
      {130LL * 1024 * 1024 * 1024, "256 GB"},
      {1LL * 1024 * 1024 * 1024 * 1024, "1.0 TB"},
      {1LL * 1024 * 1024 * 1024 * 1024 + 1, "2.0 TB"},
      {(1LL << 61) + 1, "4,096 PB"},
  };

  for (auto& c : cases) {
    int64_t rounded_bytes = handler_->RoundByteSize(c.bytes);
    EXPECT_EQ(base::ASCIIToUTF16(c.expected), ui::FormatBytes(rounded_bytes));
  }
}

TEST_F(StorageHandlerTest, GlobalSizeStat) {
  // Get local filesystem storage statistics.
  const base::FilePath mount_path =
      file_manager::util::GetMyFilesFolderForProfile(profile_);
  int64_t total_size = base::SysInfo::AmountOfTotalDiskSpace(mount_path);
  int64_t available_size = base::SysInfo::AmountOfFreeDiskSpace(mount_path);

  // Round the total size.
  int64_t rounded_total_size = handler_->RoundByteSize(total_size);
  int64_t used_size = rounded_total_size - available_size;
  double used_ratio = static_cast<double>(used_size) / rounded_total_size;

  // Get statistics from storage handler's UpdateSizeStat.
  size_stat_test_api_->StartCalculation();
  task_environment_.RunUntilIdle();

  const base::Value* dictionary =
      GetWebUICallbackMessage("storage-size-stat-changed");
  ASSERT_TRUE(dictionary) << "No 'storage-size-stat-changed' callback";

  const std::string& storage_handler_available_size =
      dictionary->FindKey("availableSize")->GetString();
  const std::string& storage_handler_used_size =
      dictionary->FindKey("usedSize")->GetString();
  double storage_handler_used_ratio =
      dictionary->FindKey("usedRatio")->GetDouble();

  EXPECT_EQ(ui::FormatBytes(available_size),
            base::ASCIIToUTF16(storage_handler_available_size));
  EXPECT_EQ(ui::FormatBytes(used_size),
            base::ASCIIToUTF16(storage_handler_used_size));
  double diff = used_ratio > storage_handler_used_ratio
                    ? used_ratio - storage_handler_used_ratio
                    : storage_handler_used_ratio - used_ratio;
  // Running the test while writing data on disk (~400MB/s), the difference
  // between the values returned by the two AmountOfFreeDiskSpace calls is never
  // more than 100KB. By expecting diff to be less than 100KB /
  // rounded_total_size, the test is very unlikely to be flaky.
  EXPECT_LE(diff, static_cast<double>(100 * 1024) / rounded_total_size);
}

TEST_F(StorageHandlerTest, StorageSpaceState) {
  // Less than 512 MB available, space state is critically low.
  int64_t total_size = 1024 * 1024 * 1024;
  int64_t available_size = 512 * 1024 * 1024 - 1;
  int space_state = GetSpaceState(&total_size, &available_size);
  EXPECT_EQ(static_cast<int>(StorageSpaceState::kStorageSpaceCriticallyLow),
            space_state);

  // Less than 1GB available, space state is low.
  available_size = 512 * 1024 * 1024;
  space_state = GetSpaceState(&total_size, &available_size);
  EXPECT_EQ(static_cast<int>(StorageSpaceState::kStorageSpaceLow), space_state);
  available_size = 1024 * 1024 * 1024 - 1;
  space_state = GetSpaceState(&total_size, &available_size);
  EXPECT_EQ(static_cast<int>(StorageSpaceState::kStorageSpaceLow), space_state);

  // From 1GB, normal space state.
  available_size = 1024 * 1024 * 1024;
  space_state = GetSpaceState(&total_size, &available_size);
  EXPECT_EQ(static_cast<int>(StorageSpaceState::kStorageSpaceNormal),
            space_state);
}

TEST_F(StorageHandlerTest, MyFilesSize) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  const base::FilePath my_files_path =
      file_manager::util::GetMyFilesFolderForProfile(profile_);
  const base::FilePath downloads_path =
      file_manager::util::GetDownloadsFolderForProfile(profile_);
  const base::FilePath android_files_path =
      profile_->GetPath().Append("AndroidFiles");
  const base::FilePath android_files_download_path =
      android_files_path.Append("Download");

  // Create directories.
  CHECK(base::CreateDirectory(downloads_path));
  CHECK(base::CreateDirectory(android_files_path));
  CHECK(base::CreateDirectory(android_files_download_path));

  // Register android files mount point.
  CHECK(storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      file_manager::util::GetAndroidFilesMountPointName(),
      storage::kFileSystemTypeNativeLocal, storage::FileSystemMountOption(),
      android_files_path));

  // Add files in My files and android files.
  AddFile("random.bin", 8092, my_files_path);      // ~7.9 KB
  AddFile("tall.pdf", 15271, android_files_path);  // ~14.9 KB
  // Add file in Downloads and simulate bind mount with
  // [android files]/Download.
  AddFile("video.ogv", 59943, downloads_path);  // ~58.6 KB
  AddFile("video.ogv", 59943, android_files_download_path);

  // Calculate My files size.
  my_files_size_test_api_->StartCalculation();
  task_environment_.RunUntilIdle();

  const base::Value* callback =
      GetWebUICallbackMessage("storage-my-files-size-changed");
  ASSERT_TRUE(callback) << "No 'storage-my-files-size-changed' callback";

  // Check return value.
  EXPECT_EQ("81.4 KB", callback->GetString());
}

TEST_F(StorageHandlerTest, AppsExtensionsSize) {
  // The data for apps and extensions apps_size_test_api_installed from the
  // webstore is stored in the Extensions folder. Add data at a random location
  // in the Extensions folder and check UI callback message.
  const base::FilePath extensions_data_path =
      profile_->GetPath()
          .AppendASCII("Extensions")
          .AppendASCII("fake_extension_id");
  CHECK(base::CreateDirectory(extensions_data_path));
  AddFile("id3Audio.mp3", 180999, extensions_data_path);  // ~177 KB

  // Calculate web store apps and extensions size.
  apps_size_test_api_->StartCalculation();
  task_environment_.RunUntilIdle();

  const base::Value* callback =
      GetWebUICallbackMessage("storage-apps-size-changed");
  ASSERT_TRUE(callback) << "No 'storage-apps-size-changed' callback";

  // Check return value.
  EXPECT_EQ("177 KB", callback->GetString());

  // Simulate android apps size callback.
  // 592840 + 25284 + 9987 = 628111  ~613 KB.
  apps_size_test_api_->SimulateOnGetAndroidAppsSize(true /* succeeded */,
                                                    592840, 25284, 9987);
  task_environment_.RunUntilIdle();

  callback = GetWebUICallbackMessage("storage-apps-size-changed");
  ASSERT_TRUE(callback) << "No 'storage-apps-size-changed' callback";

  // Check return value.
  EXPECT_EQ("790 KB", callback->GetString());

  // Add more data in the Extensions folder. Android is not running and the size
  // of android apps is back down to 0 B.
  AddFile("video_long.ogv", 230096, extensions_data_path);  // ~225 KB

  // Calculate web store apps and extensions size.
  apps_size_test_api_->StartCalculation();
  task_environment_.RunUntilIdle();

  callback = GetWebUICallbackMessage("storage-apps-size-changed");
  ASSERT_TRUE(callback) << "No 'storage-apps-size-changed' callback";

  // Check return value.
  EXPECT_EQ("401 KB", callback->GetString());
}

TEST_F(StorageHandlerTest, SystemSize) {
  // The "System" row on the storage page displays the difference between the
  // total amount of used space and the sum of the sizes of the different
  // storage items of the storage page (My files, Browsing data, apps etc...)
  // This test simulates callbacks from each one of these storage items; the
  // calculation of the "System" size should only happen when all of the other
  // storage items have been calculated.
  const int64_t KB = 1024;
  const int64_t MB = 1024 * KB;
  const int64_t GB = 1024 * MB;
  const int64_t TB = 1024 * GB;

  // Simulate size stat callback.
  int64_t total_size = TB;
  int64_t available_size = 100 * GB;
  size_stat_test_api_->SimulateOnGetSizeStat(&total_size, &available_size);
  const base::Value* callback =
      GetWebUICallbackMessage("storage-size-stat-changed");
  ASSERT_TRUE(callback) << "No 'storage-size-stat-changed' callback";
  EXPECT_EQ("100 GB", callback->FindKey("availableSize")->GetString());
  EXPECT_EQ("924 GB", callback->FindKey("usedSize")->GetString());
  // Expect no system size callback until every other item has been updated.
  ASSERT_FALSE(GetWebUICallbackMessage("storage-system-size-changed"));

  // Simulate my files size callback.
  my_files_size_test_api_->SimulateOnGetTotalBytes(400 * GB);
  callback = GetWebUICallbackMessage("storage-my-files-size-changed");
  ASSERT_TRUE(callback) << "No 'storage-my-files-size-changed' callback";
  EXPECT_EQ("400 GB", callback->GetString());
  ASSERT_FALSE(GetWebUICallbackMessage("storage-system-size-changed"));

  // Simulate browsing data callbacks. Has to be called with
  // both |is_data_site| = true and false.
  browsing_data_size_test_api_->SimulateOnGetBrowsingDataSize(
      true /* is_site_data */, 10 * GB);
  ASSERT_FALSE(GetWebUICallbackMessage("storage-browsing-data-size-changed"));
  ASSERT_FALSE(GetWebUICallbackMessage("storage-system-size-changed"));
  browsing_data_size_test_api_->SimulateOnGetBrowsingDataSize(
      false /* is_site_data */, 14 * GB);
  callback = GetWebUICallbackMessage("storage-browsing-data-size-changed");
  ASSERT_TRUE(callback) << "No 'storage-browsing-data-size-changed' callback";
  EXPECT_EQ("24.0 GB", callback->GetString());
  ASSERT_FALSE(GetWebUICallbackMessage("storage-system-size-changed"));

  // Simulate apps and extensions size callbacks.
  apps_size_test_api_->SimulateOnGetAppsSize(29 * GB);
  apps_size_test_api_->SimulateOnGetAndroidAppsSize(false, 0, 0, 0);
  callback = GetWebUICallbackMessage("storage-apps-size-changed");
  ASSERT_TRUE(callback) << "No 'storage-apps-size-changed' callback";
  EXPECT_EQ("29.0 GB", callback->GetString());
  ASSERT_FALSE(GetWebUICallbackMessage("storage-system-size-changed"));
  apps_size_test_api_->SimulateOnGetAndroidAppsSize(
      true /* succeeded */, 724 * MB, 100 * MB, 200 * MB);
  callback = GetWebUICallbackMessage("storage-apps-size-changed");
  ASSERT_TRUE(callback) << "No 'storage-apps-size-changed' callback";
  EXPECT_EQ("30.0 GB", callback->GetString());
  ASSERT_FALSE(GetWebUICallbackMessage("storage-system-size-changed"));

  // Simulate crostini size callback.
  crostini_size_test_api_->SimulateOnGetCrostiniSize(70 * GB);
  callback = GetWebUICallbackMessage("storage-crostini-size-changed");
  ASSERT_TRUE(callback) << "No 'storage-crostini-size-changed' callback";
  EXPECT_EQ("70.0 GB", callback->GetString());
  ASSERT_FALSE(GetWebUICallbackMessage("storage-system-size-changed"));

  // Simulate other users size callback. No callback message until the sizes of
  // every users is calculated.
  std::vector<int64_t> other_user_sizes =
      std::vector<int64_t>{200 * GB, 50 * GB, 50 * GB};
  other_users_size_test_api_->InitializeOtherUserSize(other_user_sizes.size());
  for (std::size_t i = 0; i < other_user_sizes.size(); i++) {
    cryptohome::BaseReply result;
    result.set_error(cryptohome::CRYPTOHOME_ERROR_NOT_SET);
    cryptohome::GetAccountDiskUsageReply* usage_reply =
        result.MutableExtension(cryptohome::GetAccountDiskUsageReply::reply);
    usage_reply->set_size(other_user_sizes[i]);
    base::Optional<cryptohome::BaseReply> reply = std::move(result);
    other_users_size_test_api_->SimulateOnGetOtherUserSize(reply);
    if (i < other_user_sizes.size() - 1) {
      ASSERT_FALSE(GetWebUICallbackMessage("storage-other-users-size-changed"));
      ASSERT_FALSE(GetWebUICallbackMessage("storage-system-size-changed"));
    } else {
      // When the size of the last user's cryptohome is calculated, we expect a
      // callback with the "Other users" size.
      callback = GetWebUICallbackMessage("storage-other-users-size-changed");
      ASSERT_TRUE(callback) << "No 'storage-other-users-size-changed' callback";
      EXPECT_EQ("300 GB", callback->GetString());
      // Every item size has been calculated, system size should also be
      // updated.
      callback = GetWebUICallbackMessage("storage-system-size-changed");
      ASSERT_TRUE(callback) << "No 'storage-system-size-changed' callback";
      EXPECT_EQ("100 GB", callback->GetString());
    }
  }

  // If there's an error while calculating the size of browsing data, the size
  // of browsing data and system should be displayed as "Unknown".
  browsing_data_size_test_api_->SimulateOnGetBrowsingDataSize(
      true /* is_site_data */, -1);
  callback = GetWebUICallbackMessage("storage-browsing-data-size-changed");
  ASSERT_TRUE(callback) << "No 'storage-browsing-data-size-changed' callback";
  EXPECT_EQ("Unknown", callback->GetString());
  // The missing 24.0 GB of browsing data should be reflected in the system
  // section instead. We expect the displayed size to be 100 + 24 GB.
  callback = GetWebUICallbackMessage("storage-system-size-changed");
  ASSERT_TRUE(callback) << "No 'storage-system-size-changed' callback";
  EXPECT_EQ("124 GB", callback->GetString());

  // No error while recalculating browsing data size, the UI should be updated
  // with the right sizes.
  browsing_data_size_test_api_->SimulateOnGetBrowsingDataSize(
      true /* is_site_data */, 10 * GB);
  callback = GetWebUICallbackMessage("storage-browsing-data-size-changed");
  ASSERT_TRUE(callback) << "No 'storage-browsing-data-size-changed' callback";
  EXPECT_EQ("24.0 GB", callback->GetString());
  callback = GetWebUICallbackMessage("storage-system-size-changed");
  ASSERT_TRUE(callback) << "No 'storage-system-size-changed' callback";
  EXPECT_EQ("100 GB", callback->GetString());
}

}  // namespace

}  // namespace settings
}  // namespace chromeos
