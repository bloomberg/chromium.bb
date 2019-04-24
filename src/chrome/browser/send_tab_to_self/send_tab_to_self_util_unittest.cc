// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync/device_info/device_info.h"
#include "components/sync/device_info/device_info_sync_bridge.h"
#include "components/sync/device_info/device_info_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

// Mock DeviceInfoTracker class for setting active devices
class TestDeviceInfoTracker : public syncer::DeviceInfoTracker {
 public:
  TestDeviceInfoTracker() = default;
  ~TestDeviceInfoTracker() override = default;

  void SetActiveDevices(int devices) { active_devices_ = devices; }

  // DeviceInfoTracker implementation
  bool IsSyncing() const override { return false; }
  std::unique_ptr<syncer::DeviceInfo> GetDeviceInfo(
      const std::string& client_id) const override {
    return std::unique_ptr<syncer::DeviceInfo>();
  }
  std::vector<std::unique_ptr<syncer::DeviceInfo>> GetAllDeviceInfo()
      const override {
    return std::vector<std::unique_ptr<syncer::DeviceInfo>>();
  }
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  int CountActiveDevices() const override { return active_devices_; }

 protected:
  int active_devices_;
};

// Mock DeviceInfoSyncService to host mocked DeviceInfoTracker
class TestDeviceInfoSyncService : public syncer::DeviceInfoSyncService {
 public:
  TestDeviceInfoSyncService() = default;
  ~TestDeviceInfoSyncService() override = default;

  TestDeviceInfoTracker* GetMockDeviceInfoTracker() { return &tracker_; }
  void SetTrackerActiveDevices(int devices) {
    tracker_.SetActiveDevices(devices);
  }

  // DeviceInfoSyncService implementation
  syncer::LocalDeviceInfoProvider* GetLocalDeviceInfoProvider() override {
    return nullptr;
  }
  syncer::DeviceInfoTracker* GetDeviceInfoTracker() override {
    return &tracker_;
  }
  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetControllerDelegate()
      override {
    return nullptr;
  }
  void InitLocalCacheGuid(const std::string& cache_guid,
                          const std::string& session_name) override {}
  void ClearLocalCacheGuid() override {}

 protected:
  TestDeviceInfoTracker tracker_;
};

std::unique_ptr<KeyedService> BuildMockDeviceInfoSyncService(
    content::BrowserContext* context) {
  return std::make_unique<TestDeviceInfoSyncService>();
}

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

// SendTabToSelfModelMock
class SendTabToSelfModelMock : public SendTabToSelfModel {
 public:
  SendTabToSelfModelMock() = default;
  ~SendTabToSelfModelMock() override = default;

  MOCK_METHOD2(AddEntry,
               const SendTabToSelfEntry*(const GURL&, const std::string&));
  MOCK_METHOD1(DeleteEntry, void(const std::string&));
  MOCK_METHOD1(DismissEntry, void(const std::string&));

  MOCK_CONST_METHOD0(GetAllGuids, std::vector<std::string>());
  MOCK_METHOD0(DeleteAllEntries, void());
  MOCK_CONST_METHOD1(GetEntryByGUID, SendTabToSelfEntry*(const std::string&));

  void AddObserver(SendTabToSelfModelObserver* observer) {}
  void RemoveObserver(SendTabToSelfModelObserver* observer) {}
};

// Mock a SendTabToSelfSyncService to get SendTabToSelfModelMock
class SendTabToSelfSyncServiceMock : public SendTabToSelfSyncService {
 public:
  SendTabToSelfSyncServiceMock() = default;
  ~SendTabToSelfSyncServiceMock() override = default;

  SendTabToSelfModel* GetSendTabToSelfModel() override {
    return &send_tab_to_self_model_mock_;
  }

 protected:
  SendTabToSelfModelMock send_tab_to_self_model_mock_;
};

std::unique_ptr<KeyedService> BuildTestSendTabToSelfSyncService(
    content::BrowserContext* context) {
  return std::make_unique<SendTabToSelfSyncServiceMock>();
}

class SendTabToSelfUtilTest : public BrowserWithTestWindowTest {
 public:
  SendTabToSelfUtilTest() = default;
  ~SendTabToSelfUtilTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    test_sync_service_ = static_cast<syncer::TestSyncService*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildTestSyncService)));

    mock_device_sync_service_ = static_cast<TestDeviceInfoSyncService*>(
        DeviceInfoSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockDeviceInfoSyncService)));

    incognito_profile_ = profile()->GetOffTheRecordProfile();
    url_ = GURL("https://www.google.com");
  }

  // Set up all test conditions to let ShouldOfferFeature() return true
  void SetUpAllTrueEnv() {
    scoped_feature_list_.InitAndEnableFeature(switches::kSyncSendTabToSelf);
    syncer::ModelTypeSet enabled_modeltype(syncer::SEND_TAB_TO_SELF);
    test_sync_service_->SetPreferredDataTypes(enabled_modeltype);

    mock_device_sync_service_->SetTrackerActiveDevices(2);

    AddTab(browser(), url_);
    NavigateAndCommitActiveTab(url_);
  }

  // Set up a environment in which the feature flag is disabled
  void SetUpFeatureDisabledEnv() {
    scoped_feature_list_.InitAndDisableFeature(switches::kSyncSendTabToSelf);
    syncer::ModelTypeSet enabled_modeltype(syncer::SEND_TAB_TO_SELF);
    test_sync_service_->SetPreferredDataTypes(enabled_modeltype);

    mock_device_sync_service_->SetTrackerActiveDevices(2);

    AddTab(browser(), url_);
    NavigateAndCommitActiveTab(url_);
  }

 protected:
  syncer::TestSyncService* test_sync_service_;
  TestDeviceInfoSyncService* mock_device_sync_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
  Profile* incognito_profile_;
  GURL url_;
};

TEST_F(SendTabToSelfUtilTest, IsFlagEnabled_True) {
  scoped_feature_list_.InitAndEnableFeature(switches::kSyncSendTabToSelf);

  EXPECT_TRUE(IsFlagEnabled());
}

TEST_F(SendTabToSelfUtilTest, IsFlagEnabled_False) {
  scoped_feature_list_.InitAndDisableFeature(switches::kSyncSendTabToSelf);
  EXPECT_FALSE(IsFlagEnabled());
}

TEST_F(SendTabToSelfUtilTest, IsUserSyncTypeEnabled_True) {
  syncer::ModelTypeSet enabled_modeltype(syncer::SEND_TAB_TO_SELF);
  test_sync_service_->SetPreferredDataTypes(enabled_modeltype);

  EXPECT_TRUE(IsUserSyncTypeEnabled(profile()));

  test_sync_service_->SetPreferredDataTypes(syncer::ModelTypeSet::All());

  EXPECT_TRUE(IsUserSyncTypeEnabled(profile()));
}

TEST_F(SendTabToSelfUtilTest, IsUserSyncTypeEnabled_False) {
  test_sync_service_->SetPreferredDataTypes(syncer::ModelTypeSet());

  EXPECT_FALSE(IsUserSyncTypeEnabled(profile()));
}

TEST_F(SendTabToSelfUtilTest, IsSyncingOnMultipleDevices_True) {
  mock_device_sync_service_->SetTrackerActiveDevices(2);

  EXPECT_TRUE(IsSyncingOnMultipleDevices(profile()));
}

TEST_F(SendTabToSelfUtilTest, IsSyncingOnMultipleDevices_False) {
  mock_device_sync_service_->SetTrackerActiveDevices(0);

  EXPECT_FALSE(IsSyncingOnMultipleDevices(profile()));
}

TEST_F(SendTabToSelfUtilTest, ContentRequirementsMet) {
  EXPECT_TRUE(IsContentRequirementsMet(url_, profile()));
}

TEST_F(SendTabToSelfUtilTest, NotHTTPOrHTTPS) {
  url_ = GURL("192.168.0.0");
  EXPECT_FALSE(IsContentRequirementsMet(url_, profile()));
}

TEST_F(SendTabToSelfUtilTest, NativePage) {
  url_ = GURL("chrome://flags");
  EXPECT_FALSE(IsContentRequirementsMet(url_, profile()));
}

TEST_F(SendTabToSelfUtilTest, IncognitoMode) {
  EXPECT_FALSE(IsContentRequirementsMet(url_, incognito_profile_));
}

TEST_F(SendTabToSelfUtilTest, ShouldOfferFeature_True) {
  SetUpAllTrueEnv();

  EXPECT_TRUE(ShouldOfferFeature(browser()));
}

TEST_F(SendTabToSelfUtilTest, ShouldOfferFeature_IsFlagEnabled_False) {
  SetUpFeatureDisabledEnv();
  EXPECT_FALSE(ShouldOfferFeature(browser()));
}

TEST_F(SendTabToSelfUtilTest, ShouldOfferFeature_IsUserSyncTypeEnabled_False) {
  SetUpAllTrueEnv();
  test_sync_service_->SetPreferredDataTypes(syncer::ModelTypeSet());

  EXPECT_FALSE(ShouldOfferFeature(browser()));
}

TEST_F(SendTabToSelfUtilTest,
       ShouldOfferFeature_IsSyncingOnMultipleDevices_False) {
  SetUpAllTrueEnv();
  mock_device_sync_service_->SetTrackerActiveDevices(0);

  EXPECT_FALSE(ShouldOfferFeature(browser()));
}

TEST_F(SendTabToSelfUtilTest,
       ShouldOfferFeature_IsContentRequirementsMet_False) {
  SetUpAllTrueEnv();
  url_ = GURL("192.168.0.0");
  NavigateAndCommitActiveTab(url_);

  EXPECT_FALSE(ShouldOfferFeature(browser()));
}

TEST_F(SendTabToSelfUtilTest, CreateNewEntry) {
  SetUpAllTrueEnv();
  SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildTestSendTabToSelfSyncService));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url = tab->GetURL();
  std::string title = base::UTF16ToUTF8(tab->GetTitle());
  SendTabToSelfModelMock* model_mock = static_cast<SendTabToSelfModelMock*>(
      SendTabToSelfSyncServiceFactory::GetForProfile(profile())
          ->GetSendTabToSelfModel());

  EXPECT_CALL(*model_mock, AddEntry(url, title))
      .WillOnce(testing::Return(nullptr));

  CreateNewEntry(tab, profile());
}

}  // namespace

}  // namespace send_tab_to_self
