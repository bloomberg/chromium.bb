// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_table_view_controller.h"

#import <LocalAuthentication/LAContext.h>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#include "components/content_settings/core/common/features.h"
#include "components/handoff/pref_names_ios.h"
#include "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/strings/grit/components_strings.h"
#import "components/sync/driver/mock_sync_service.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/policy/policy_util.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#include "ios/chrome/browser/sync/sync_service_factory.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ::testing::Return;

namespace {

NSString* const kSpdyProxyEnabled = @"SpdyProxyEnabled";

std::unique_ptr<KeyedService> BuildMockSyncService(web::BrowserState* context) {
  return std::make_unique<syncer::MockSyncService>();
}

// Checks if the device has Passcode, Face ID, or Touch ID set up.
BOOL DeviceSupportsAuthentication() {
  LAContext* context = [[LAContext alloc] init];
  return [context canEvaluatePolicy:LAPolicyDeviceOwnerAuthentication
                              error:nil];
}

// Bitset
typedef NS_ENUM(NSUInteger, PrivacyTableViewControllerTestConfig) {
  // Tests should run with Enhanced Protection flag enabled.
  PrivacyTableViewControllerTestConfigEnhancedProtectionEnabled = 1 << 0,
  // Tests should run with Third-party intents in Incognito flag enabled.
  PrivacyTableViewControllerTestConfig3PIntentsInIncognitoEnabled = 1 << 1,
};

// `ScopedFeatureList` wrapper so `PrivacyTableViewControllerTest` can ensure
// proper initialization of the feature list before all of its own attributes.
class WithScopedFeatureList {
 protected:
  WithScopedFeatureList(
      std::pair<std::vector<base::Feature>, std::vector<base::Feature>> const&
          enabled_disabled_features) {
    feature_list_.InitWithFeatures(enabled_disabled_features.first,
                                   enabled_disabled_features.second);
  }

  base::test::ScopedFeatureList feature_list_;
};

class PrivacyTableViewControllerTest
    : public ChromeTableViewControllerTest,
      public testing::WithParamInterface<PrivacyTableViewControllerTestConfig>,
      public WithScopedFeatureList {
 protected:
  PrivacyTableViewControllerTest()
      : WithScopedFeatureList(EnabledDisabledFeatures()) {}

  std::pair<std::vector<base::Feature>, std::vector<base::Feature>>
  EnabledDisabledFeatures() const {
    std::pair<std::vector<base::Feature>, std::vector<base::Feature>>
        enabledDisabledFeatures;

    // Explicitly enable/disable Enhanced Protection flag.
    if (GetParam() &
        PrivacyTableViewControllerTestConfigEnhancedProtectionEnabled) {
      enabledDisabledFeatures.first.push_back(
          safe_browsing::kEnhancedProtection);
    } else {
      enabledDisabledFeatures.second.push_back(
          safe_browsing::kEnhancedProtection);
    }

    // Explicitly enable/disable Third-party intents in Incognito flag.
    if (GetParam() &
        PrivacyTableViewControllerTestConfigEnhancedProtectionEnabled) {
      enabledDisabledFeatures.first.push_back(kIOS3PIntentsInIncognito);
    } else {
      enabledDisabledFeatures.second.push_back(kIOS3PIntentsInIncognito);
    }

    return enabledDisabledFeatures;
  }

  void SetUp() override {
    ChromeTableViewControllerTest::SetUp();

    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.SetPrefService(CreatePrefService());
    test_cbs_builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockSyncService));
    chrome_browser_state_ = test_cbs_builder.Build();

    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());

    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    initialValueForSpdyProxyEnabled_ =
        [[defaults valueForKey:kSpdyProxyEnabled] copy];
    [defaults setValue:@"Disabled" forKey:kSpdyProxyEnabled];
  }

  void TearDown() override {
    if (initialValueForSpdyProxyEnabled_) {
      [[NSUserDefaults standardUserDefaults]
          setObject:initialValueForSpdyProxyEnabled_
             forKey:kSpdyProxyEnabled];
    } else {
      [[NSUserDefaults standardUserDefaults]
          removeObjectForKey:kSpdyProxyEnabled];
    }
    ChromeTableViewControllerTest::TearDown();
  }

  // Makes a PrefService to be used by the test.
  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    RegisterBrowserStatePrefs(registry.get());
    sync_preferences::PrefServiceMockFactory factory;
    return factory.CreateSyncable(registry.get());
  }

  ChromeTableViewController* InstantiateController() override {
    return [[PrivacyTableViewController alloc] initWithBrowser:browser_.get()
                                        reauthenticationModule:nil];
  }

  syncer::MockSyncService* mock_sync_service() {
    return static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForBrowserState(chrome_browser_state_.get()));
  }

  // Returns the proper detail text for the safe browsing item depending on the
  // safe browsing and enhanced protection preference values.
  NSString* SafeBrowsingDetailText() {
    PrefService* prefService = chrome_browser_state_->GetPrefs();
    if (safe_browsing::IsEnhancedProtectionEnabled(*prefService)) {
      return l10n_util::GetNSString(
          IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_TITLE);
    } else if (safe_browsing::IsSafeBrowsingEnabled(*prefService)) {
      return l10n_util::GetNSString(
          IDS_IOS_PRIVACY_SAFE_BROWSING_STANDARD_PROTECTION_TITLE);
    }
    return l10n_util::GetNSString(
        IDS_IOS_PRIVACY_SAFE_BROWSING_NO_PROTECTION_DETAIL_TITLE);
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
  NSString* initialValueForSpdyProxyEnabled_;
};

// Tests PrivacyTableViewController is set up with all appropriate items
// and sections.
TEST_P(PrivacyTableViewControllerTest, TestModel) {
  PrefService* prefService = chrome_browser_state_->GetPrefs();
  CreateController();
  CheckController();

  int expectedNumberOfSections = 3;
  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedProtection)) {
    expectedNumberOfSections++;
  }
  if (base::FeatureList::IsEnabled(kIOS3PIntentsInIncognito)) {
    expectedNumberOfSections++;
  }
  EXPECT_EQ(expectedNumberOfSections, NumberOfSections());

  int currentSection = 0;
  // PrivacyContent section.
  EXPECT_EQ(1, NumberOfItemsInSection(currentSection));
  CheckTextCellTextAndDetailText(
      l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_TITLE), nil,
      currentSection, 0);

  // SafeBrowsing section.
  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedProtection)) {
    currentSection++;
    EXPECT_EQ(currentSection, NumberOfItemsInSection(1));
    CheckTextCellTextAndDetailText(
        l10n_util::GetNSString(IDS_IOS_PRIVACY_SAFE_BROWSING_TITLE),
        SafeBrowsingDetailText(), 1, 0);
  }

  // WebServices section.
  currentSection++;
  EXPECT_EQ(1, NumberOfItemsInSection(currentSection));
  NSString* handoffSubtitle = chrome_browser_state_->GetPrefs()->GetBoolean(
                                  prefs::kIosHandoffToOtherDevices)
                                  ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                                  : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  CheckTextCellTextAndDetailText(
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ENABLE_HANDOFF_TO_OTHER_DEVICES),
      handoffSubtitle, currentSection, 0);

  // IncognitoAuth section.
  currentSection++;
  EXPECT_EQ(1, NumberOfItemsInSection(currentSection));
  if ((IsIncognitoModeDisabled(prefService) ||
       !DeviceSupportsAuthentication())) {
    // Disabled version of Incognito auth item is expected in this case.
    CheckInfoButtonCellStatusWithIdAndTextWithId(
        IDS_IOS_SETTING_OFF, IDS_IOS_INCOGNITO_REAUTH_SETTING_NAME,
        currentSection, 0);
  } else {
    CheckSwitchCellStateAndTextWithId(NO, IDS_IOS_INCOGNITO_REAUTH_SETTING_NAME,
                                      currentSection, 0);
  }

  // IncognitoInterstitial section.
  if (base::FeatureList::IsEnabled(kIOS3PIntentsInIncognito)) {
    currentSection++;
    EXPECT_EQ(1, NumberOfItemsInSection(currentSection));
    if ((IsIncognitoModeDisabled(prefService) ||
         IsIncognitoModeForced(prefService))) {
      // Disabled version of Incognito interstitial item is expected in this
      // case.
      CheckInfoButtonCellStatusWithIdAndTextWithId(
          IDS_IOS_SETTING_OFF, IDS_IOS_OPTIONS_ENABLE_INCOGNITO_INTERSTITIAL,
          currentSection, 0);
    } else {
      CheckSwitchCellStateAndTextWithId(
          NO, IDS_IOS_OPTIONS_ENABLE_INCOGNITO_INTERSTITIAL, currentSection, 0);
    }
  }

  // Testing section index and text of the privacy footer.
  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedProtection)) {
    CheckSectionFooter(
        l10n_util::GetNSString(IDS_IOS_PRIVACY_GOOGLE_SERVICES_FOOTER),
        /* section= */ expectedNumberOfSections - 1);
  } else {
    CheckSectionFooter(
        l10n_util::GetNSString(IDS_IOS_PRIVACY_GOOGLE_SERVICES_FOOTER),
        /* section= */ 0);
  }
}

// Tests PrivacyTableViewController sets the correct privacy footer for a
// non-syncing user.
TEST_P(PrivacyTableViewControllerTest, TestModelFooterWithSyncDisabled) {
  ON_CALL(*mock_sync_service()->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(Return(false));

  CreateController();
  CheckController();

  int expectedNumberOfSections = 3;
  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedProtection)) {
    expectedNumberOfSections++;
  }
  if (base::FeatureList::IsEnabled(kIOS3PIntentsInIncognito)) {
    expectedNumberOfSections++;
  }
  EXPECT_EQ(expectedNumberOfSections, NumberOfSections());

  // Testing section index and text of the privacy footer.
  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedProtection)) {
    CheckSectionFooter(
        l10n_util::GetNSString(IDS_IOS_PRIVACY_GOOGLE_SERVICES_FOOTER),
        /* section= */ expectedNumberOfSections - 1);
  } else {
    CheckSectionFooter(
        l10n_util::GetNSString(IDS_IOS_PRIVACY_GOOGLE_SERVICES_FOOTER),
        /* section= */ 0);
  }
}

// Tests PrivacyTableViewController sets the correct privacy footer for a
// syncing user.
TEST_P(PrivacyTableViewControllerTest, TestModelFooterWithSyncEnabled) {
  ON_CALL(*mock_sync_service()->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(Return(true));
  ON_CALL(*mock_sync_service(), HasSyncConsent()).WillByDefault(Return(true));

  CreateController();
  CheckController();

  int expectedNumberOfSections = 3;
  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedProtection)) {
    expectedNumberOfSections++;
  }
  if (base::FeatureList::IsEnabled(kIOS3PIntentsInIncognito)) {
    expectedNumberOfSections++;
  }
  EXPECT_EQ(expectedNumberOfSections, NumberOfSections());

  // Testing section index and text of the privacy footer.
  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedProtection)) {
    CheckSectionFooter(
        l10n_util::GetNSString(IDS_IOS_PRIVACY_SYNC_AND_GOOGLE_SERVICES_FOOTER),
        /* section= */ expectedNumberOfSections - 1);
  } else {
    CheckSectionFooter(
        l10n_util::GetNSString(IDS_IOS_PRIVACY_SYNC_AND_GOOGLE_SERVICES_FOOTER),
        /* section= */ 0);
  }
}

INSTANTIATE_TEST_SUITE_P(
    PrivacyTableViewControllerTestAllConfigs,
    PrivacyTableViewControllerTest,
    testing::Values(
        0,
        PrivacyTableViewControllerTestConfigEnhancedProtectionEnabled,
        PrivacyTableViewControllerTestConfig3PIntentsInIncognitoEnabled,
        PrivacyTableViewControllerTestConfigEnhancedProtectionEnabled |
            PrivacyTableViewControllerTestConfig3PIntentsInIncognitoEnabled));

}  // namespace
