// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/bandwidth_management_collection_view_controller.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#import "ios/chrome/browser/ui/settings/dataplan_usage_collection_view_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/chrome/test/testing_application_context.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "net/log/test_net_log.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class BandwidthManagementCollectionViewControllerTest
    : public CollectionViewControllerTest {
 public:
  BandwidthManagementCollectionViewControllerTest() {}

 protected:
  void SetUp() override {
    CollectionViewControllerTest::SetUp();

    sync_preferences::PrefServiceMockFactory factory;
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs(
        factory.CreateSyncable(registry.get()));
    RegisterBrowserStatePrefs(registry.get());
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.SetPrefService(std::move(prefs));
    chrome_browser_state_ = test_cbs_builder.Build();

    CreateController();
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    CollectionViewControllerTest::TearDown();
  }

  CollectionViewController* InstantiateController() override {
    return [[BandwidthManagementCollectionViewController alloc]
        initWithBrowserState:chrome_browser_state_.get()];
  }

  web::TestWebThreadBundle thread_bundle_;
  net::TestNetLog net_log_;
  IOSChromeScopedTestingLocalState local_state_;

  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

TEST_F(BandwidthManagementCollectionViewControllerTest, TestModel) {
  CheckController();
  EXPECT_EQ(2, NumberOfSections());

  const NSInteger action_section = 0;
  EXPECT_EQ(1, NumberOfItemsInSection(action_section));
  // Preload webpages item.
  NSString* expected_title =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_PRELOAD_WEBPAGES);
  NSString* expected_subtitle = [DataplanUsageCollectionViewController
      currentLabelForPreference:chrome_browser_state_->GetPrefs()
                       basePref:prefs::kNetworkPredictionEnabled
                       wifiPref:prefs::kNetworkPredictionWifiOnly];
  CheckTextCellTitleAndSubtitle(expected_title, expected_subtitle,
                                action_section, 0);
}

}  // namespace
