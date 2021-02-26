// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_mediator.h"

#import <UIKit/UIKit.h>

#import "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#import "components/sync/driver/mock_sync_service.h"
#include "components/sync/driver/sync_service.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace {
std::unique_ptr<KeyedService> CreateMockSyncService(
    web::BrowserState* context) {
  return std::make_unique<NiceMock<syncer::MockSyncService>>();
}

std::unique_ptr<KeyedService> CreateMockSyncSetupService(
    web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<SyncSetupServiceMock>(
      ProfileSyncServiceFactory::GetForBrowserState(browser_state));
}

PrefService* SetPrefService() {
  TestingPrefServiceSimple* prefs = new TestingPrefServiceSimple();
  PrefRegistrySimple* registry = prefs->registry();
  registry->RegisterBooleanPref(autofill::prefs::kAutofillWalletImportEnabled,
                                true);
  return prefs;
}
}  // namespace

class ManageSyncSettingsMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(ProfileSyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(SyncSetupServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncSetupService));
    browser_state_ = builder.Build();

    consumer_ = [[ManageSyncSettingsTableViewController alloc]
        initWithStyle:UITableViewStyleGrouped];
    [consumer_ loadModel];

    pref_service_ = SetPrefService();
    sync_setup_service_mock_ = static_cast<SyncSetupServiceMock*>(
        SyncSetupServiceFactory::GetForBrowserState(browser_state_.get()));
    sync_service_mock_ = static_cast<syncer::MockSyncService*>(
        ProfileSyncServiceFactory::GetForBrowserState(browser_state_.get()));

    mediator_ = [[ManageSyncSettingsMediator alloc]
        initWithSyncService:sync_service_mock_
            userPrefService:pref_service_];
    mediator_.syncSetupService = sync_setup_service_mock_;
    mediator_.consumer = consumer_;
  }

  void SetupSyncServiceInitializedExpectations() {
    ON_CALL(*sync_setup_service_mock_, IsSyncEnabled())
        .WillByDefault(Return(true));
    ON_CALL(*sync_setup_service_mock_, IsSyncingAllDataTypes())
        .WillByDefault(Return(true));
    ON_CALL(*sync_setup_service_mock_, HasFinishedInitialSetup())
        .WillByDefault(Return(true));
    ON_CALL(*sync_service_mock_, GetTransportState())
        .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
  }

  void SetupSyncDisabledExpectations() {
    ON_CALL(*sync_setup_service_mock_, IsSyncEnabled())
        .WillByDefault(Return(false));
    ON_CALL(*sync_setup_service_mock_, IsSyncingAllDataTypes())
        .WillByDefault(Return(true));
    ON_CALL(*sync_setup_service_mock_, HasFinishedInitialSetup())
        .WillByDefault(Return(false));
    ON_CALL(*sync_service_mock_, GetTransportState())
        .WillByDefault(Return(syncer::SyncService::TransportState::DISABLED));
  }

 protected:
  // Needed for test browser state created by TestChromeBrowserState().
  web::WebTaskEnvironment task_environment_;

  syncer::MockSyncService* sync_service_mock_;
  SyncSetupServiceMock* sync_setup_service_mock_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;

  ManageSyncSettingsMediator* mediator_ = nullptr;
  ManageSyncSettingsTableViewController* consumer_ = nullptr;
  PrefService* pref_service_ = nullptr;
};

// Tests for Advanced Settings items.

// Tests that encryption is not accessible when Sync settings have not been
// confirmed.
TEST_F(ManageSyncSettingsMediatorTest, SyncServiceSetupNotCommitted) {
  SetupSyncDisabledExpectations();
  ON_CALL(*sync_setup_service_mock_, GetSyncServiceState())
      .WillByDefault(Return(SyncSetupService::kSyncSettingsNotConfirmed));

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  NSArray* advanced_settings_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       AdvancedSettingsSectionIdentifier];
  ASSERT_EQ(3UL, advanced_settings_items.count);

  TableViewImageItem* encryption_item = advanced_settings_items[0];
  ASSERT_EQ(encryption_item.type, SyncSettingsItemType::EncryptionItemType);
  ASSERT_FALSE(encryption_item.enabled);
}

// Tests that encryption is not accessible when Sync is disabled by the
// administrator.
TEST_F(ManageSyncSettingsMediatorTest, SyncServiceDisabledByAdministrator) {
  SetupSyncDisabledExpectations();
  ON_CALL(*sync_service_mock_, GetDisableReasons())
      .WillByDefault(
          Return(syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY));

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  NSArray* advanced_settings_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       AdvancedSettingsSectionIdentifier];
  ASSERT_EQ(3UL, advanced_settings_items.count);

  TableViewImageItem* encryption_item = advanced_settings_items[0];
  ASSERT_EQ(encryption_item.type, SyncSettingsItemType::EncryptionItemType);
  ASSERT_FALSE(encryption_item.enabled);
}

// Tests that encryption is accessible when there is a Sync error due to a
// missing passphrase, but Sync has otherwise been enabled.
TEST_F(ManageSyncSettingsMediatorTest, SyncServiceDisabledNeedsPassphrase) {
  SetupSyncServiceInitializedExpectations();
  ON_CALL(*sync_setup_service_mock_, GetSyncServiceState())
      .WillByDefault(Return(SyncSetupService::kSyncServiceNeedsPassphrase));

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  NSArray* advanced_settings_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       AdvancedSettingsSectionIdentifier];
  ASSERT_EQ(3UL, advanced_settings_items.count);

  TableViewImageItem* encryption_item = advanced_settings_items[0];
  ASSERT_EQ(encryption_item.type, SyncSettingsItemType::EncryptionItemType);
  ASSERT_TRUE(encryption_item.enabled);
}

// Tests that encryption is accessible when Sync is enabled.
TEST_F(ManageSyncSettingsMediatorTest, SyncServiceEnabled) {
  SetupSyncServiceInitializedExpectations();
  ON_CALL(*sync_setup_service_mock_, GetSyncServiceState())
      .WillByDefault(Return(SyncSetupService::kNoSyncServiceError));

  [mediator_ manageSyncSettingsTableViewControllerLoadModel:mediator_.consumer];

  NSArray* advanced_settings_items = [mediator_.consumer.tableViewModel
      itemsInSectionWithIdentifier:SyncSettingsSectionIdentifier::
                                       AdvancedSettingsSectionIdentifier];
  ASSERT_EQ(3UL, advanced_settings_items.count);

  TableViewImageItem* encryption_item = advanced_settings_items[0];
  ASSERT_EQ(encryption_item.type, SyncSettingsItemType::EncryptionItemType);
  ASSERT_TRUE(encryption_item.enabled);
}
