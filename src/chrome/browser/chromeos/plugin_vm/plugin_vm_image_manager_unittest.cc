// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_download_client.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_manager_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/background_service/test/test_download_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip.h"

namespace plugin_vm {

namespace {

const char kProfileName[] = "p1";
const char kUrl[] = "http://example.com";
const char kHash[] =
    "842841a4c75a55ad050d686f4ea5f77e83ae059877fe9b6946aa63d3d057ed32";
const char kHashUppercase[] =
    "842841A4C75A55AD050D686F4EA5F77E83AE059877FE9B6946AA63D3D057ED32";
const char kHash2[] =
    "02f06421ae27144aacdc598aebcd345a5e2e634405e8578300173628fe1574bd";
const char kPluginVmImageUnzipped[] = "plugin_vm_image_unzipped";
const char kPluginVmImageFile1[] = "plugin_vm_image_file_1";
const char kContent1[] = "This is content #1.";
const int kContent1Size = strlen(kContent1);
const char kPluginVmImageFile2[] = "plugin_vm_image_file_2";
const char kContent2[] = "This is content #2.";
const int kContent2Size = strlen(kContent2);
const int kContentSize = kContent1Size + kContent2Size;

}  // namespace

class MockObserver : public PluginVmImageManager::Observer {
 public:
  MOCK_METHOD0(OnDownloadStarted, void());
  MOCK_METHOD3(OnDownloadProgressUpdated,
               void(uint64_t bytes_downloaded,
                    int64_t content_length,
                    int64_t download_bytes_per_sec));
  MOCK_METHOD0(OnDownloadCompleted, void());
  MOCK_METHOD0(OnDownloadCancelled, void());
  MOCK_METHOD0(OnDownloadFailed, void());
  MOCK_METHOD3(OnUnzippingProgressUpdated,
               void(int64_t bytes_unzipped,
                    int64_t plugin_vm_image_size,
                    int64_t unzipping_bytes_per_sec));
  MOCK_METHOD0(OnUnzipped, void());
  MOCK_METHOD0(OnUnzippingFailed, void());
  MOCK_METHOD0(OnRegistered, void());
  MOCK_METHOD0(OnRegistrationFailed, void());
};

class PluginVmImageManagerTest : public testing::Test {
 public:
  PluginVmImageManagerTest()
      : download_service_(new download::test::TestDownloadService()) {}

 protected:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  PluginVmImageManager* manager_;
  download::test::TestDownloadService* download_service_;
  std::unique_ptr<MockObserver> observer_;
  base::FilePath fake_downloaded_plugin_vm_image_archive_;

  void SetUp() override {
    ASSERT_TRUE(profiles_dir_.CreateUniqueTempDir());
    CreateProfile();
    SetPluginVmImagePref(kUrl, kHash);

    manager_ = PluginVmImageManagerFactory::GetForProfile(profile_.get());
    download_service_->SetIsReady(true);
    download_service_->set_client(
        new PluginVmImageDownloadClient(profile_.get()));
    manager_->SetDownloadServiceForTesting(download_service_);
    observer_ = std::make_unique<MockObserver>();
    manager_->SetObserver(observer_.get());

    fake_downloaded_plugin_vm_image_archive_ = CreateZipFile();
  }

  void TearDown() override {
    profile_.reset();
    observer_.reset();
  }

  void SetPluginVmImagePref(std::string url, std::string hash) {
    DictionaryPrefUpdate update(profile_->GetPrefs(),
                                plugin_vm::prefs::kPluginVmImage);
    base::DictionaryValue* plugin_vm_image = update.Get();
    plugin_vm_image->SetKey("url", base::Value(url));
    plugin_vm_image->SetKey("hash", base::Value(hash));
  }

  void ProcessImageUntilUnzipping() {
    manager_->StartDownload();
    test_browser_thread_bundle_.RunUntilIdle();
  }

  void ProcessImageUntilRegistration() {
    ProcessImageUntilUnzipping();

    // Faking downloaded file for testing.
    manager_->SetDownloadedPluginVmImageArchiveForTesting(
        fake_downloaded_plugin_vm_image_archive_);
    manager_->StartUnzipping();
    test_browser_thread_bundle_.RunUntilIdle();
  }

  void ProcessImageUntilConfigured() {
    ProcessImageUntilRegistration();

    manager_->StartRegistration();
    test_browser_thread_bundle_.RunUntilIdle();
  }

  void EnsurePluginVmImageIsRemoved() {
    base::FilePath plugin_vm_image_unzipped =
        profile_->GetPath()
            .AppendASCII(kCrosvmDir)
            .AppendASCII(kPvmDir)
            .AppendASCII(kPluginVmImageDir);
    EXPECT_FALSE(base::DirectoryExists(plugin_vm_image_unzipped));
  }

  base::FilePath CreateZipFile() {
    base::FilePath src_dir = profile_->GetPath().AppendASCII("src");
    base::CreateDirectory(src_dir);
    base::FilePath dest_dir = profile_->GetPath().AppendASCII("dest");
    base::CreateDirectory(dest_dir);
    base::FilePath zip_file = dest_dir.Append("out");

    base::FilePath plugin_vm_image_unzipped =
        src_dir.Append(kPluginVmImageUnzipped);
    base::CreateDirectory(plugin_vm_image_unzipped);
    base::FilePath plugin_vm_image_file_1 =
        plugin_vm_image_unzipped.Append(kPluginVmImageFile1);
    base::FilePath plugin_vm_image_file_2 =
        plugin_vm_image_unzipped.Append(kPluginVmImageFile2);
    base::WriteFile(plugin_vm_image_file_1, kContent1, strlen(kContent1));
    base::WriteFile(plugin_vm_image_file_2, kContent2, strlen(kContent2));

    zip::Zip(src_dir, zip_file, true /* include_hidden_files */);
    return zip_file;
  }

 private:
  base::ScopedTempDir profiles_dir_;

  void CreateProfile() {
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(kProfileName);
    profile_builder.SetPath(profiles_dir_.GetPath().AppendASCII(kProfileName));
    std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(pref_service->registry());
    profile_builder.SetPrefService(std::move(pref_service));
    profile_ = profile_builder.Build();
  }

  DISALLOW_COPY_AND_ASSIGN(PluginVmImageManagerTest);
};

TEST_F(PluginVmImageManagerTest, DownloadPluginVmImageParamsTest) {
  EXPECT_CALL(*observer_, OnDownloadCompleted());
  EXPECT_CALL(*observer_, OnUnzippingProgressUpdated(kContent1Size,
                                                     kContentSize, testing::_));
  EXPECT_CALL(*observer_, OnUnzippingProgressUpdated(kContentSize, kContentSize,
                                                     testing::_));
  EXPECT_CALL(*observer_, OnUnzipped());
  EXPECT_CALL(*observer_, OnRegistered());

  manager_->StartDownload();

  std::string guid = manager_->GetCurrentDownloadGuidForTesting();
  base::Optional<download::DownloadParams> params =
      download_service_->GetDownload(guid);
  ASSERT_TRUE(params.has_value());
  EXPECT_EQ(guid, params->guid);
  EXPECT_EQ(download::DownloadClient::PLUGIN_VM_IMAGE, params->client);
  EXPECT_EQ(GURL(kUrl), params->request_params.url);

  // Finishing image processing.
  test_browser_thread_bundle_.RunUntilIdle();
  // Faking downloaded file for testing.
  manager_->SetDownloadedPluginVmImageArchiveForTesting(
      fake_downloaded_plugin_vm_image_archive_);
  manager_->StartUnzipping();
  test_browser_thread_bundle_.RunUntilIdle();
  manager_->StartRegistration();
  test_browser_thread_bundle_.RunUntilIdle();
}

TEST_F(PluginVmImageManagerTest, OnlyOneImageIsProcessedTest) {
  EXPECT_CALL(*observer_, OnDownloadCompleted());
  EXPECT_CALL(*observer_, OnUnzippingProgressUpdated(kContent1Size,
                                                     kContentSize, testing::_));
  EXPECT_CALL(*observer_, OnUnzippingProgressUpdated(kContentSize, kContentSize,
                                                     testing::_));
  EXPECT_CALL(*observer_, OnUnzipped());
  EXPECT_CALL(*observer_, OnRegistered());

  manager_->StartDownload();

  EXPECT_TRUE(manager_->IsProcessingImage());

  test_browser_thread_bundle_.RunUntilIdle();
  // Faking downloaded file for testing.
  manager_->SetDownloadedPluginVmImageArchiveForTesting(
      fake_downloaded_plugin_vm_image_archive_);

  EXPECT_TRUE(manager_->IsProcessingImage());

  manager_->StartUnzipping();

  EXPECT_TRUE(manager_->IsProcessingImage());

  test_browser_thread_bundle_.RunUntilIdle();

  EXPECT_TRUE(manager_->IsProcessingImage());

  manager_->StartRegistration();

  EXPECT_TRUE(manager_->IsProcessingImage());

  test_browser_thread_bundle_.RunUntilIdle();

  EXPECT_FALSE(manager_->IsProcessingImage());
}

TEST_F(PluginVmImageManagerTest, CanProceedWithANewImageWhenSucceededTest) {
  EXPECT_CALL(*observer_, OnDownloadCompleted()).Times(2);
  EXPECT_CALL(*observer_, OnUnzippingProgressUpdated(kContent1Size,
                                                     kContentSize, testing::_))
      .Times(2);
  EXPECT_CALL(*observer_, OnUnzippingProgressUpdated(kContentSize, kContentSize,
                                                     testing::_))
      .Times(2);
  EXPECT_CALL(*observer_, OnUnzipped()).Times(2);
  EXPECT_CALL(*observer_, OnRegistered()).Times(2);

  ProcessImageUntilConfigured();

  EXPECT_FALSE(manager_->IsProcessingImage());

  // As it is deleted after successful unzipping.
  fake_downloaded_plugin_vm_image_archive_ = CreateZipFile();
  ProcessImageUntilConfigured();
}

TEST_F(PluginVmImageManagerTest, CanProceedWithANewImageWhenFailedTest) {
  EXPECT_CALL(*observer_, OnDownloadFailed());
  EXPECT_CALL(*observer_, OnDownloadCompleted());
  EXPECT_CALL(*observer_, OnUnzippingProgressUpdated(kContent1Size,
                                                     kContentSize, testing::_));
  EXPECT_CALL(*observer_, OnUnzippingProgressUpdated(kContentSize, kContentSize,
                                                     testing::_));
  EXPECT_CALL(*observer_, OnUnzipped());
  EXPECT_CALL(*observer_, OnRegistered());

  manager_->StartDownload();
  std::string guid = manager_->GetCurrentDownloadGuidForTesting();
  download_service_->SetFailedDownload(guid, false);
  test_browser_thread_bundle_.RunUntilIdle();

  EXPECT_FALSE(manager_->IsProcessingImage());

  ProcessImageUntilConfigured();
}

TEST_F(PluginVmImageManagerTest, CancelledDownloadTest) {
  EXPECT_CALL(*observer_, OnDownloadCompleted()).Times(0);
  EXPECT_CALL(*observer_, OnDownloadCancelled());

  manager_->StartDownload();
  manager_->CancelDownload();
  test_browser_thread_bundle_.RunUntilIdle();
  // Finishing image processing as it should really happen.
  manager_->OnDownloadCancelled();
}

TEST_F(PluginVmImageManagerTest, UnzipDownloadedImageTest) {
  EXPECT_CALL(*observer_, OnDownloadCompleted());
  EXPECT_CALL(*observer_, OnUnzippingProgressUpdated(kContent1Size,
                                                     kContentSize, testing::_));
  EXPECT_CALL(*observer_, OnUnzippingProgressUpdated(kContentSize, kContentSize,
                                                     testing::_));
  EXPECT_CALL(*observer_, OnUnzipped());
  EXPECT_CALL(*observer_, OnRegistered());

  ProcessImageUntilConfigured();

  // Checking that all files are in place.
  base::FilePath plugin_vm_image_unzipped =
      profile_->GetPath()
          .AppendASCII(kCrosvmDir)
          .AppendASCII(kPvmDir)
          .AppendASCII(kPluginVmImageDir)
          .AppendASCII(kPluginVmImageUnzipped);
  EXPECT_TRUE(base::DirectoryExists(plugin_vm_image_unzipped));
  base::FilePath plugin_vm_image_file_1 =
      plugin_vm_image_unzipped.AppendASCII(kPluginVmImageFile1);
  EXPECT_TRUE(base::PathExists(plugin_vm_image_file_1));
  EXPECT_FALSE(base::DirectoryExists(plugin_vm_image_file_1));
  std::string plugin_vm_image_file_1_content;
  EXPECT_TRUE(base::ReadFileToString(plugin_vm_image_file_1,
                                     &plugin_vm_image_file_1_content));
  EXPECT_EQ(kContent1, plugin_vm_image_file_1_content);
  base::FilePath plugin_vm_image_file_2 =
      plugin_vm_image_unzipped.AppendASCII(kPluginVmImageFile2);
  EXPECT_TRUE(base::PathExists(plugin_vm_image_file_2));
  EXPECT_FALSE(base::DirectoryExists(plugin_vm_image_file_2));
  std::string plugin_vm_image_file_2_content;
  EXPECT_TRUE(base::ReadFileToString(plugin_vm_image_file_2,
                                     &plugin_vm_image_file_2_content));
  EXPECT_EQ(kContent2, plugin_vm_image_file_2_content);
}

TEST_F(PluginVmImageManagerTest, UnzipNonExistingImageTest) {
  EXPECT_CALL(*observer_, OnDownloadCompleted());
  EXPECT_CALL(*observer_, OnUnzippingFailed());

  ProcessImageUntilUnzipping();
  // Should fail as fake downloaded file isn't set.
  manager_->StartUnzipping();
  test_browser_thread_bundle_.RunUntilIdle();

  EnsurePluginVmImageIsRemoved();
}

TEST_F(PluginVmImageManagerTest, CancelUnzippingTest) {
  EXPECT_CALL(*observer_, OnDownloadCompleted());
  EXPECT_CALL(*observer_, OnUnzippingProgressUpdated(kContent1Size,
                                                     kContentSize, testing::_));
  EXPECT_CALL(*observer_, OnUnzippingFailed());

  ProcessImageUntilUnzipping();

  // Faking downloaded file for testing.
  manager_->SetDownloadedPluginVmImageArchiveForTesting(
      fake_downloaded_plugin_vm_image_archive_);
  manager_->StartUnzipping();
  manager_->CancelUnzipping();
  test_browser_thread_bundle_.RunUntilIdle();

  EnsurePluginVmImageIsRemoved();
}

TEST_F(PluginVmImageManagerTest, CancelRegistrationTest) {
  EXPECT_CALL(*observer_, OnDownloadCompleted());
  EXPECT_CALL(*observer_, OnUnzippingProgressUpdated(kContent1Size,
                                                     kContentSize, testing::_));
  EXPECT_CALL(*observer_, OnUnzippingProgressUpdated(kContentSize, kContentSize,
                                                     testing::_));
  EXPECT_CALL(*observer_, OnUnzipped());
  EXPECT_CALL(*observer_, OnRegistered()).Times(0);
  EXPECT_CALL(*observer_, OnRegistrationFailed()).Times(0);

  ProcessImageUntilRegistration();

  manager_->StartRegistration();
  manager_->CancelRegistration();
  test_browser_thread_bundle_.RunUntilIdle();

  EnsurePluginVmImageIsRemoved();
}

TEST_F(PluginVmImageManagerTest, EmptyPluginVmImageUrlTest) {
  SetPluginVmImagePref("", kHash);

  EXPECT_CALL(*observer_, OnDownloadFailed());

  ProcessImageUntilUnzipping();
}

TEST_F(PluginVmImageManagerTest, VerifyDownloadTest) {
  EXPECT_FALSE(manager_->VerifyDownload(kHash2));
  EXPECT_TRUE(manager_->VerifyDownload(kHashUppercase));
  EXPECT_TRUE(manager_->VerifyDownload(kHash));
}

}  // namespace plugin_vm
