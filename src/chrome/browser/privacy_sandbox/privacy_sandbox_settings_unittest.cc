// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings.h"

#include "base/test/gtest_util.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/util/values/values_util.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/federated_learning/floc_id_provider.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/federated_learning/features/features.h"
#include "components/federated_learning/floc_id.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/federated_learning/floc.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

namespace {

class MockFlocIdProvider : public federated_learning::FlocIdProvider {
 public:
  blink::mojom::InterestCohortPtr GetInterestCohortForJsApi(
      const GURL& url,
      const absl::optional<url::Origin>& top_frame_origin) const override {
    return blink::mojom::InterestCohort::New();
  }
  MOCK_METHOD(void, MaybeRecordFlocToUkm, (ukm::SourceId), (override));
  MOCK_METHOD(base::Time, GetApproximateNextComputeTime, (), (const, override));
};

class MockPrivacySandboxObserver : public PrivacySandboxSettings::Observer {
 public:
  MOCK_METHOD(void, OnFlocDataAccessibleSinceUpdated, (bool), (override));
};

// Define an additional content setting value to simulate an unmanaged default
// content setting.
const ContentSetting kNoSetting = static_cast<ContentSetting>(-1);

struct CookieContentSettingException {
  std::string primary_pattern;
  std::string secondary_pattern;
  ContentSetting content_setting;
};

}  // namespace

class PrivacySandboxSettingsTest : public testing::Test {
 public:
  PrivacySandboxSettingsTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    InitializePrefsBeforeStart();

    // Disable the sandbox feature to prevent profile creation automatically
    // creating another copy of the service.
    // TODO(crbug.com/1152336): When the feature has solidified, inject the
    // service into the profile as required for tests.
    feature_list()->InitAndDisableFeature(features::kPrivacySandboxSettings);

    privacy_sandbox_settings_ = std::make_unique<PrivacySandboxSettings>(
        HostContentSettingsMapFactory::GetForProfile(profile()),
        CookieSettingsFactory::GetForProfile(profile()).get(),
        profile()->GetPrefs(), policy_service(), sync_service(),
        identity_test_env()->identity_manager());

    // Reset so tests can apply their desired feature state.
    feature_list()->Reset();
  }

  virtual void InitializePrefsBeforeStart() {}

  // Sets up preferences and content settings based on provided parameters.
  void SetupTestState(
      bool privacy_sandbox_available,
      bool privacy_sandbox_enabled,
      bool block_third_party_cookies,
      ContentSetting default_cookie_setting,
      std::vector<CookieContentSettingException> user_cookie_exceptions,
      ContentSetting managed_cookie_setting,
      std::vector<CookieContentSettingException> managed_cookie_exceptions) {
    // Setup block-third-party-cookies settings.
    profile()->GetTestingPrefService()->SetUserPref(
        prefs::kCookieControlsMode,
        std::make_unique<base::Value>(static_cast<int>(
            block_third_party_cookies
                ? content_settings::CookieControlsMode::kBlockThirdParty
                : content_settings::CookieControlsMode::kOff)));

    // Setup cookie content settings.
    auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
    auto user_provider = std::make_unique<content_settings::MockProvider>();
    auto managed_provider = std::make_unique<content_settings::MockProvider>();

    if (default_cookie_setting != kNoSetting) {
      user_provider->SetWebsiteSetting(
          ContentSettingsPattern::Wildcard(),
          ContentSettingsPattern::Wildcard(), ContentSettingsType::COOKIES,
          std::make_unique<base::Value>(default_cookie_setting));
    }

    for (const auto& exception : user_cookie_exceptions) {
      user_provider->SetWebsiteSetting(
          ContentSettingsPattern::FromString(exception.primary_pattern),
          ContentSettingsPattern::FromString(exception.secondary_pattern),
          ContentSettingsType::COOKIES,
          std::make_unique<base::Value>(exception.content_setting));
    }

    if (managed_cookie_setting != kNoSetting) {
      managed_provider->SetWebsiteSetting(
          ContentSettingsPattern::Wildcard(),
          ContentSettingsPattern::Wildcard(), ContentSettingsType::COOKIES,
          std::make_unique<base::Value>(managed_cookie_setting));
    }

    for (const auto& exception : managed_cookie_exceptions) {
      managed_provider->SetWebsiteSetting(
          ContentSettingsPattern::FromString(exception.primary_pattern),
          ContentSettingsPattern::FromString(exception.secondary_pattern),
          ContentSettingsType::COOKIES,
          std::make_unique<base::Value>(exception.content_setting));
    }

    content_settings::TestUtils::OverrideProvider(
        map, std::move(user_provider),
        HostContentSettingsMap::DEFAULT_PROVIDER);
    content_settings::TestUtils::OverrideProvider(
        map, std::move(managed_provider),
        HostContentSettingsMap::POLICY_PROVIDER);

    // Setup privacy sandbox feature & preference.
    feature_list()->Reset();
    if (privacy_sandbox_available) {
      feature_list()->InitWithFeatures(
          {features::kPrivacySandboxSettings, features::kConversionMeasurement,
           blink::features::kInterestCohortAPIOriginTrial},
          {});
    } else {
      feature_list()->InitAndDisableFeature(features::kPrivacySandboxSettings);
    }

    profile()->GetTestingPrefService()->SetUserPref(
        prefs::kPrivacySandboxApisEnabled,
        std::make_unique<base::Value>(privacy_sandbox_enabled));
  }

  TestingProfile* profile() { return &profile_; }
  PrivacySandboxSettings* privacy_sandbox_settings() {
    return privacy_sandbox_settings_.get();
  }
  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }
  syncer::TestSyncService* sync_service() { return &sync_service_; }
  policy::MockPolicyService* policy_service() { return &mock_policy_service_; }
  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  testing::NiceMock<policy::MockPolicyService> mock_policy_service_;

  TestingProfile profile_;
  base::test::ScopedFeatureList feature_list_;
  syncer::TestSyncService sync_service_;

  std::unique_ptr<PrivacySandboxSettings> privacy_sandbox_settings_;
};

TEST_F(PrivacySandboxSettingsTest, PrivacySandboxSettingsFunctional) {
  feature_list()->InitWithFeatures(
      {features::kConversionMeasurement,
       blink::features::kInterestCohortAPIOriginTrial},
      {features::kPrivacySandboxSettings});
  EXPECT_FALSE(privacy_sandbox_settings()->PrivacySandboxSettingsFunctional());
  feature_list()->Reset();

  feature_list()->InitWithFeatures(
      {features::kPrivacySandboxSettings},
      {features::kConversionMeasurement,
       blink::features::kInterestCohortAPIOriginTrial});
  EXPECT_TRUE(privacy_sandbox_settings()->PrivacySandboxSettingsFunctional());
}

TEST_F(PrivacySandboxSettingsTest, CookieSettingAppliesWhenUiDisabled) {
  // When the Privacy Sandbox UI is unavailable, the cookie setting should
  // apply directly.
  SetupTestState(
      /*privacy_sandbox_available=*/false,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  EXPECT_TRUE(privacy_sandbox_settings()->IsFlocAllowedForContext(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));

  EXPECT_TRUE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));
  EXPECT_EQ(std::vector<GURL>{GURL("https://embedded.com")},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com")}));

  SetupTestState(
      /*privacy_sandbox_available=*/false,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://another-embedded.com", "*",
        ContentSetting::CONTENT_SETTING_BLOCK},
       {"https://another-test.com", "*",
        ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  EXPECT_TRUE(privacy_sandbox_settings()->IsFlocAllowedForContext(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowedForContext(
      GURL("https://another-test.com"), absl::nullopt));

  EXPECT_TRUE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));
  EXPECT_EQ(std::vector<GURL>{GURL("https://embedded.com")},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com"),
                 GURL("https://another-embedded.com")}));

  SetupTestState(
      /*privacy_sandbox_available=*/false,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW}},
      /*managed_cookie_setting=*/kNoSetting,
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_BLOCK}});

  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowedForContext(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsFlocAllowedForContext(
      GURL("https://embedded.com"), absl::nullopt));

  EXPECT_FALSE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  // Should  block due to impression origin.
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  // Should  block due to conversion origin.
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));
  EXPECT_EQ(std::vector<GURL>{GURL("https://another-embedded.com")},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com"),
                 GURL("https://another-embedded.com")}));
}

TEST_F(PrivacySandboxSettingsTest, PreferenceOverridesDefaultContentSetting) {
  // When the Privacy Sandbox UI is available, the sandbox preference should
  // override the default cookie content setting.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  EXPECT_TRUE(privacy_sandbox_settings()->IsFlocAllowedForContext(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));

  EXPECT_TRUE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));
  EXPECT_EQ(std::vector<GURL>{},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com"),
                 GURL("https://another-embedded.com")}));

  // An allow exception should not override the preference value.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://another-embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://embedded.com", "https://another-test.com",
        ContentSetting::CONTENT_SETTING_ALLOW}},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowedForContext(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));
  EXPECT_EQ(std::vector<GURL>{},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com"),
                 GURL("https://another-embedded.com")}));
}

TEST_F(PrivacySandboxSettingsTest, CookieBlockExceptionsApply) {
  // When the Privacy Sandbox UI & preference are both enabled, targeted cookie
  // block exceptions should still apply.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_BLOCK},
       {"https://another-embedded.com", "*",
        ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowedForContext(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));
  EXPECT_EQ(std::vector<GURL>{},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com"),
                 GURL("https://another-embedded.com")}));

  // User created exceptions should not apply if a managed default coookie
  // setting exists. What the managed default setting actually is should *not*
  // affect whether APIs are enabled. The cookie managed state is reflected in
  // the privacy sandbox preferences directly.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_BLOCK},
       {"https://another-embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_BLOCK},
       {"https://embedded.com", "https://another-test.com",
        ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*managed_cookie_exceptions=*/{});

  EXPECT_TRUE(privacy_sandbox_settings()->IsFlocAllowedForContext(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));
  EXPECT_EQ(std::vector<GURL>{},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com"),
                 GURL("https://another-embedded.com")}));

  // Managed content setting exceptions should override both the privacy
  // sandbox pref and any user settings.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://another-embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://embedded.com", "https://another-test.com",
        ContentSetting::CONTENT_SETTING_ALLOW}},
      /*managed_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_BLOCK}});

  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowedForContext(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsFlocAllowedForContext(
      GURL("https://unrelated.com"),
      url::Origin::Create(GURL("https://unrelated.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://unrelated-a.com")),
      url::Origin::Create(GURL("https://unrelated-b.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://unrelated-c.com")),
      url::Origin::Create(GURL("https://unrelated-d.com")),
      url::Origin::Create(GURL("https://unrelated-e.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));
  EXPECT_EQ(std::vector<GURL>{GURL("https://another-embedded.com")},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com"),
                 GURL("https://another-embedded.com")}));

  // A less specific block exception should not override a more specific allow
  // exception. The effective content setting in this scenario is still allow,
  // even though a block exception exists.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://embedded.com", "https://another-test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://[*.]embedded.com", "https://[*.]test.com",
        ContentSetting::CONTENT_SETTING_BLOCK},
       {"https://[*.]embedded.com", "https://[*.]another-test.com",
        ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  EXPECT_TRUE(privacy_sandbox_settings()->IsFlocAllowedForContext(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));

  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));

  // Exceptions which specify a top frame origin should not match against other
  // top frame origins, or an empty origin.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_BLOCK}});

  EXPECT_TRUE(privacy_sandbox_settings()->IsFlocAllowedForContext(
      GURL("https://embedded.com"), absl::nullopt));

  EXPECT_TRUE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://yet-another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://another-test.com")),
      GURL("https://embedded.com")));
  EXPECT_EQ(std::vector<GURL>{},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com"),
                 GURL("https://another-embedded.com")}));

  // Exceptions which specify a wildcard top frame origin should match both
  // empty top frames and non empty top frames.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "*", ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowedForContext(
      GURL("https://embedded.com"), absl::nullopt));
  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowedForContext(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));
  EXPECT_EQ(std::vector<GURL>{GURL("https://another-embedded.com")},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com"),
                 GURL("https://another-embedded.com")}));
}

TEST_F(PrivacySandboxSettingsTest, ThirdPartyByDefault) {
  // Check that when the UI is not enabled, all requests are considered
  // as third party requests.
  SetupTestState(
      /*privacy_sandbox_available=*/false,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowedForContext(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowedForContext(
      GURL("https://embedded.com"), absl::nullopt));

  EXPECT_FALSE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://embedded.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://embedded.com")),
      url::Origin::Create(GURL("https://embedded.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://embedded.com")),
      GURL("https://embedded.com")));
  EXPECT_EQ(std::vector<GURL>{},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://embedded.com")),
                {GURL("https://embedded.com")}));
}

TEST_F(PrivacySandboxSettingsTest, IsFledgeAllowed) {
  // FLEDGE should be disabled if 3P cookies are blocked.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));
  EXPECT_EQ(std::vector<GURL>{},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com")}));

  // FLEDGE should be disabled if all cookies are blocked.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));
  EXPECT_EQ(std::vector<GURL>{},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com")}));

  // FLEDGE should be disabled if the privacy sandbox is available and disabled,
  // regardless of other cookie settings.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW}},
      /*managed_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*managed_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW}});

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));
  EXPECT_EQ(std::vector<GURL>{},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com")}));

  // The privacy sandbox preference value should only be consulted if the
  // feature is available.
  SetupTestState(
      /*privacy_sandbox_available=*/false,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));
  EXPECT_EQ(std::vector<GURL>{GURL("https://embedded.com")},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com")}));

  // The managed cookie content setting should override all user cookie content
  // settings.
  SetupTestState(
      /*privacy_sandbox_available=*/false,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*managed_cookie_exceptions=*/{});

  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));
  EXPECT_EQ(std::vector<GURL>{GURL("https://embedded.com")},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com")}));

  // The managed cookie content setting should not override an available and
  // disabled privacy sandbox setting.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*managed_cookie_exceptions=*/{});

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));
  EXPECT_EQ(std::vector<GURL>{},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com")}));
}

TEST_F(PrivacySandboxSettingsTest, IsPrivacySandboxAllowed) {
  SetupTestState(
      /*privacy_sandbox_available=*/false,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivacySandboxAllowed());

  SetupTestState(
      /*privacy_sandbox_available=*/false,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivacySandboxAllowed());

  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivacySandboxAllowed());

  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivacySandboxAllowed());

  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivacySandboxAllowed());
}

TEST_F(PrivacySandboxSettingsTest, IsFlocAllowed) {
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  EXPECT_TRUE(privacy_sandbox_settings()->IsFlocAllowed());

  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowed());

  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, false);
  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowed());

  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowed());

  SetupTestState(
      /*privacy_sandbox_available=*/false,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowed());
}

TEST_F(PrivacySandboxSettingsTest, FlocDataAccessibleSince) {
  ASSERT_NE(base::Time(), base::Time::Now());

  EXPECT_EQ(base::Time(),
            privacy_sandbox_settings()->FlocDataAccessibleSince());

  privacy_sandbox_settings()->OnCookiesCleared();

  EXPECT_EQ(base::Time::Now(),
            privacy_sandbox_settings()->FlocDataAccessibleSince());
}

TEST_F(PrivacySandboxSettingsTest, GetFlocDescriptionForDisplay) {
  // Check that the returned FLoC description correctly takes into account the
  // time between FLoC recomputes.
  std::map<std::string, std::u16string> param_to_expected_string = {
      {"1h", l10n_util::GetPluralStringFUTF16(
                 IDS_PRIVACY_SANDBOX_FLOC_DESCRIPTION, 0)},
      {"23h", l10n_util::GetPluralStringFUTF16(
                  IDS_PRIVACY_SANDBOX_FLOC_DESCRIPTION, 0)},
      {"24h", l10n_util::GetPluralStringFUTF16(
                  IDS_PRIVACY_SANDBOX_FLOC_DESCRIPTION, 1)},
      {"25h", l10n_util::GetPluralStringFUTF16(
                  IDS_PRIVACY_SANDBOX_FLOC_DESCRIPTION, 1)},
      {"60h", l10n_util::GetPluralStringFUTF16(
                  IDS_PRIVACY_SANDBOX_FLOC_DESCRIPTION, 3)},
      {"167h", l10n_util::GetPluralStringFUTF16(
                   IDS_PRIVACY_SANDBOX_FLOC_DESCRIPTION, 7)},
      {"168h", l10n_util::GetPluralStringFUTF16(
                   IDS_PRIVACY_SANDBOX_FLOC_DESCRIPTION, 7)}};

  for (const auto& param_expected : param_to_expected_string) {
    feature_list()->InitAndEnableFeatureWithParameters(
        federated_learning::kFederatedLearningOfCohorts,
        {{"update_interval", param_expected.first}});
    EXPECT_EQ(param_expected.second,
              privacy_sandbox_settings()->GetFlocDescriptionForDisplay());
    feature_list()->Reset();
  }
}

TEST_F(PrivacySandboxSettingsTest, GetFlocIdForDisplay) {
  // Check that the cohort identifier is correctly converted to a string when
  // available.
  feature_list()->InitWithFeatures(
      {blink::features::kInterestCohortAPIOriginTrial}, {});
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, true);
  federated_learning::FlocId floc_id(123456, base::Time(), base::Time::Now(),
                                     /*sorting_lsh_version=*/0);
  floc_id.SaveToPrefs(profile()->GetTestingPrefService());

  EXPECT_EQ(std::u16string(u"123456"),
            privacy_sandbox_settings()->GetFlocIdForDisplay());

  // If the FLoC preference, the Sandbox Preference, or the feature is disabled,
  // or the FLoC ID is invalid, the invalid string should be returned.
  feature_list()->Reset();
  feature_list()->InitWithFeatures(
      {}, {blink::features::kInterestCohortAPIOriginTrial});
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_INVALID),
            privacy_sandbox_settings()->GetFlocIdForDisplay());

  feature_list()->Reset();
  feature_list()->InitWithFeatures(
      {blink::features::kInterestCohortAPIOriginTrial}, {});
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, false);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_INVALID),
            privacy_sandbox_settings()->GetFlocIdForDisplay());

  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, true);
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, false);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_INVALID),
            privacy_sandbox_settings()->GetFlocIdForDisplay());

  floc_id.InvalidateIdAndSaveToPrefs(profile()->GetTestingPrefService());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_INVALID),
            privacy_sandbox_settings()->GetFlocIdForDisplay());
}

TEST_F(PrivacySandboxSettingsTest, GetFlocIdNextUpdateForDisplay) {
  // Check that date FLoC will be next updated is returned when available.
  MockFlocIdProvider mock_floc_id_provider;
  feature_list()->InitWithFeatures(
      {blink::features::kInterestCohortAPIOriginTrial}, {});
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, true);
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);

  std::map<base::TimeDelta, std::u16string> offsets_to_expected_string = {
      {base::TimeDelta::FromHours(23),
       l10n_util::GetPluralStringFUTF16(
           IDS_PRIVACY_SANDBOX_FLOC_TIME_TO_NEXT_COMPUTE, 0)},
      {base::TimeDelta::FromHours(25),
       l10n_util::GetPluralStringFUTF16(
           IDS_PRIVACY_SANDBOX_FLOC_TIME_TO_NEXT_COMPUTE, 1)},
      {base::TimeDelta::FromDays(2),
       l10n_util::GetPluralStringFUTF16(
           IDS_PRIVACY_SANDBOX_FLOC_TIME_TO_NEXT_COMPUTE, 2)},
      {base::TimeDelta::FromHours(60),
       l10n_util::GetPluralStringFUTF16(
           IDS_PRIVACY_SANDBOX_FLOC_TIME_TO_NEXT_COMPUTE, 3)},
      {base::TimeDelta::FromHours(167),  // 1 hour less than 7 days.
       l10n_util::GetPluralStringFUTF16(
           IDS_PRIVACY_SANDBOX_FLOC_TIME_TO_NEXT_COMPUTE, 7)}};

  for (const auto& offset_expected : offsets_to_expected_string) {
    EXPECT_CALL(mock_floc_id_provider, GetApproximateNextComputeTime)
        .WillOnce(testing::Return(base::Time::Now() + offset_expected.first));
    EXPECT_EQ(
        offset_expected.second,
        privacy_sandbox_settings()->GetFlocIdNextUpdateForDisplay(
            &mock_floc_id_provider, profile()->GetPrefs(), base::Time::Now()));
    testing::Mock::VerifyAndClearExpectations(&mock_floc_id_provider);
  }

  // Check that disabling FLoC is also reflected in the returned string.
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, false);
  EXPECT_CALL(mock_floc_id_provider, GetApproximateNextComputeTime).Times(0);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_PRIVACY_SANDBOX_FLOC_TIME_TO_NEXT_COMPUTE_INVALID),
      privacy_sandbox_settings()->GetFlocIdNextUpdateForDisplay(
          &mock_floc_id_provider, profile()->GetPrefs(), base::Time::Now()));
  testing::Mock::VerifyAndClearExpectations(&mock_floc_id_provider);

  // Disabling the FLoC feature should also invalidate the next compute time.
  feature_list()->Reset();
  feature_list()->InitWithFeatures(
      {}, {blink::features::kInterestCohortAPIOriginTrial});
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  testing::Mock::VerifyAndClearExpectations(&mock_floc_id_provider);
}

TEST_F(PrivacySandboxSettingsTest, GetFlocResetExplanationForDisplay) {
  // Check that the string description indicating what happens when the user
  // resets the FLoC ID updates appropriately based on the feature parameter.
  std::map<std::string, std::u16string> param_to_expected_string = {
      {"1h", l10n_util::GetPluralStringFUTF16(
                 IDS_PRIVACY_SANDBOX_FLOC_RESET_EXPLANATION, 0)},
      {"23h", l10n_util::GetPluralStringFUTF16(
                  IDS_PRIVACY_SANDBOX_FLOC_RESET_EXPLANATION, 0)},
      {"24h", l10n_util::GetPluralStringFUTF16(
                  IDS_PRIVACY_SANDBOX_FLOC_RESET_EXPLANATION, 1)},
      {"25h", l10n_util::GetPluralStringFUTF16(
                  IDS_PRIVACY_SANDBOX_FLOC_RESET_EXPLANATION, 1)},
      {"60h", l10n_util::GetPluralStringFUTF16(
                  IDS_PRIVACY_SANDBOX_FLOC_RESET_EXPLANATION, 3)},
      {"167h", l10n_util::GetPluralStringFUTF16(
                   IDS_PRIVACY_SANDBOX_FLOC_RESET_EXPLANATION, 7)},
      {"168h", l10n_util::GetPluralStringFUTF16(
                   IDS_PRIVACY_SANDBOX_FLOC_RESET_EXPLANATION, 7)}};

  for (const auto& param_expected : param_to_expected_string) {
    feature_list()->InitAndEnableFeatureWithParameters(
        federated_learning::kFederatedLearningOfCohorts,
        {{"update_interval", param_expected.first}});
    EXPECT_EQ(param_expected.second,
              privacy_sandbox_settings()->GetFlocResetExplanationForDisplay());
    feature_list()->Reset();
  }
}

TEST_F(PrivacySandboxSettingsTest, GetFlocStatusForDisplay) {
  // Check the status of the user's FLoC is correctly returned. This depends
  // on whether the FLoC origin trial feature is enabled, and whether the user
  // has FLoC enabled.
  feature_list()->InitWithFeatures(
      {blink::features::kInterestCohortAPIOriginTrial}, {});
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, true);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_STATUS_ACTIVE),
            privacy_sandbox_settings()->GetFlocStatusForDisplay());

  // The Privacy Sandbox APIs pref & FLoC pref should disable the trial when
  // either is disabled.
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, false);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_STATUS_NOT_ACTIVE),
      privacy_sandbox_settings()->GetFlocStatusForDisplay());

  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, true);
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, false);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_STATUS_NOT_ACTIVE),
      privacy_sandbox_settings()->GetFlocStatusForDisplay());

  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  feature_list()->Reset();
  feature_list()->InitWithFeatures(
      {}, {blink::features::kInterestCohortAPIOriginTrial});
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PRIVACY_SANDBOX_FLOC_STATUS_ELIGIBLE_NOT_ACTIVE),
            privacy_sandbox_settings()->GetFlocStatusForDisplay());
}

TEST_F(PrivacySandboxSettingsTest, IsFlocIdResettable) {
  // Check that if FLoC is functional the FLoC ID is resettable, regardless of
  // whether the FLoC ID is currently valid.
  feature_list()->InitWithFeatures(
      {blink::features::kInterestCohortAPIOriginTrial}, {});
  federated_learning::FlocId floc_id(123456, base::Time(), base::Time::Now(),
                                     /*sorting_lsh_version=*/0);
  floc_id.SaveToPrefs(profile()->GetTestingPrefService());
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, true);
  EXPECT_TRUE(privacy_sandbox_settings()->IsFlocIdResettable());

  feature_list()->Reset();
  feature_list()->InitWithFeatures(
      {}, {blink::features::kInterestCohortAPIOriginTrial});
  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocIdResettable());

  feature_list()->Reset();
  feature_list()->InitWithFeatures(
      {blink::features::kInterestCohortAPIOriginTrial}, {});
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, false);
  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocIdResettable());

  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, false);
  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocIdResettable());

  floc_id.InvalidateIdAndSaveToPrefs(profile()->GetTestingPrefService());
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  EXPECT_TRUE(privacy_sandbox_settings()->IsFlocIdResettable());
}

TEST_F(PrivacySandboxSettingsTest, IsFlocPrefEnabled) {
  // IsFlocPrefEnabled should directly reflect the state of the FLoC pref.
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  EXPECT_TRUE(privacy_sandbox_settings()->IsFlocPrefEnabled());

  // The Privacy Sandbox APIs pref should not impact the return value.
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, false);
  EXPECT_TRUE(privacy_sandbox_settings()->IsFlocPrefEnabled());

  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, false);
  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocPrefEnabled());
}

TEST_F(PrivacySandboxSettingsTest, SetFlocPrefEnabled) {
  // The FLoc pref should always be updated by this function, regardless of
  // other Sandbox State.
  base::UserActionTester user_action_tester;
  ASSERT_EQ(0, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.FlocEnabled"));
  ASSERT_EQ(0, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.FlocDisabled"));

  privacy_sandbox_settings()->SetFlocPrefEnabled(false);
  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxFlocEnabled));
  ASSERT_EQ(0, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.FlocEnabled"));
  ASSERT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.FlocDisabled"));

  // Disabling the sandbox shouldn't prevent the pref from being updated. This
  // state is not directly allowable by the UI, but the state itself is valid
  // as far as the PrivacySandboxSettings service is concerned.
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, false);
  privacy_sandbox_settings()->SetFlocPrefEnabled(true);
  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxFlocEnabled));
  ASSERT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.FlocEnabled"));
  ASSERT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.FlocDisabled"));
}

TEST_F(PrivacySandboxSettingsTest, OnPrivacySandboxPrefChanged) {
  // When either the main Privacy Sandbox pref, or the FLoC pref, are changed
  // the FLoC ID should be reset.
  MockPrivacySandboxObserver mock_privacy_sandbox_observer;
  privacy_sandbox_settings()->AddObserver(&mock_privacy_sandbox_observer);
  EXPECT_CALL(mock_privacy_sandbox_observer,
              OnFlocDataAccessibleSinceUpdated(/*reset_compute_timer=*/true));

  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, false);
  testing::Mock::VerifyAndClearExpectations(&mock_privacy_sandbox_observer);

  EXPECT_CALL(mock_privacy_sandbox_observer,
              OnFlocDataAccessibleSinceUpdated(/*reset_compute_timer=*/true));
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, false);
  testing::Mock::VerifyAndClearExpectations(&mock_privacy_sandbox_observer);

  EXPECT_CALL(mock_privacy_sandbox_observer,
              OnFlocDataAccessibleSinceUpdated(/*reset_compute_timer=*/true));
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  testing::Mock::VerifyAndClearExpectations(&mock_privacy_sandbox_observer);

  EXPECT_CALL(mock_privacy_sandbox_observer,
              OnFlocDataAccessibleSinceUpdated(/*reset_compute_timer=*/true));
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, true);
  testing::Mock::VerifyAndClearExpectations(&mock_privacy_sandbox_observer);
}

TEST_F(PrivacySandboxSettingsTest, ReconciliationOutcome) {
  // Check that reconciling preferences has the appropriate outcome based on
  // the current user cookie settings.

  // Blocking 3P cookies should disable.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_settings()->ReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxApisEnabled));

  // Blocking all cookies should disable.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_settings()->ReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxApisEnabled));

  // Blocking cookies via content setting exceptions, now matter how broad,
  // should not disable.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      // /*user_cookie_exceptions=*/
      // {{"[*.]com", "*", ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_settings()->ReconcilePrivacySandboxPref();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxApisEnabled));

  // If the user has already expressed control over the privacy sandbox, it
  // should not be disabled.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kPrivacySandboxManuallyControlled,
      std::make_unique<base::Value>(true));

  privacy_sandbox_settings()->ReconcilePrivacySandboxPref();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxApisEnabled));

  // Allowing cookies should leave the sandbox enabled.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kPrivacySandboxManuallyControlled,
      std::make_unique<base::Value>(true));

  privacy_sandbox_settings()->ReconcilePrivacySandboxPref();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxApisEnabled));

  // Reconciliation should not enable the privacy sandbox.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kPrivacySandboxManuallyControlled,
      std::make_unique<base::Value>(false));

  privacy_sandbox_settings()->ReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxApisEnabled));
}

TEST_F(PrivacySandboxSettingsTest, ImmediateReconciliationNoSync) {
  // Check that if the user is not syncing preferences, reconciliation occurs
  // immediately.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  auto registered_types =
      sync_service()->GetUserSettings()->GetRegisteredSelectableTypes();
  registered_types.Remove(syncer::UserSelectableType::kPreferences);
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, registered_types);

  privacy_sandbox_settings()->MaybeReconcilePrivacySandboxPref();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxSettingsTest, ImmediateReconciliationSyncComplete) {
  // Check that if sync has completed a cycle that reconciliation occurs
  // immediately.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  sync_service()->SetNonEmptyLastCycleSnapshot();

  privacy_sandbox_settings()->MaybeReconcilePrivacySandboxPref();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxSettingsTest, ImmediateReconciliationPersistentSyncError) {
  // Check that if sync has a persistent error that reconciliation occurs
  // immediately.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  sync_service()->SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);

  privacy_sandbox_settings()->MaybeReconcilePrivacySandboxPref();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxSettingsTest, ImmediateReconciliationNoDisable) {
  // Check that if the local settings would not disable the privacy sandbox
  // that reconciliation runs.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_settings()->MaybeReconcilePrivacySandboxPref();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxSettingsTest, DelayedReconciliationSyncSuccess) {
  // Check that a sync service which has not yet started delays reconciliation
  // until it has completed a sync cycle.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  sync_service()->SetEmptyLastCycleSnapshot();

  privacy_sandbox_settings()->MaybeReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  sync_service()->SetNonEmptyLastCycleSnapshot();
  sync_service()->FireSyncCycleCompleted();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxSettingsTest, DelayedReconciliationSyncFailure) {
  // Check that a sync service which has not yet started delays reconciliation
  // until a persistent error has occurred.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  sync_service()->SetEmptyLastCycleSnapshot();

  privacy_sandbox_settings()->MaybeReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // A transient sync startup state should not result in reconciliation.
  sync_service()->SetTransportState(
      syncer::SyncService::TransportState::START_DEFERRED);
  sync_service()->FireStateChanged();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // A state update after an unrecoverable error should result in
  // reconciliation.
  sync_service()->SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);
  sync_service()->FireStateChanged();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxSettingsTest, DelayedReconciliationIdentityFailure) {
  // Check that a sync service which has not yet started delays reconciliation
  // until a persistent identity error has occurred.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  sync_service()->SetEmptyLastCycleSnapshot();

  privacy_sandbox_settings()->MaybeReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // An account becoming available should not result in reconciliation.
  identity_test_env()->MakePrimaryAccountAvailable("test@test.com");

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // A successful update to refresh tokens should not result in reconciliation.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // A persistent authentication error for a non-primary account should not
  // result in reconciliation.
  auto non_primary_account =
      identity_test_env()->MakeAccountAvailable("unrelated@unrelated.com");
  identity_test_env()->SetRefreshTokenForAccount(
      non_primary_account.account_id);
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      non_primary_account.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // A perisistent authentication error for the primary account should result
  // in reconciliation.
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_test_env()->identity_manager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSync),
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxSettingsTest, DelayedReconciliationSyncIssueThenManaged) {
  // Check that if before an initial sync issue is resolved, the cookie settings
  // are disabled by policy, that reconciliation does not run until the policy
  // is removed.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  sync_service()->SetEmptyLastCycleSnapshot();

  privacy_sandbox_settings()->MaybeReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // Apply a management state that is disabling cookies. This should result
  // in the policy service being observed when the sync issue is resolved.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*managed_cookie_exceptions=*/{});

  EXPECT_CALL(*policy_service(), AddObserver(policy::POLICY_DOMAIN_CHROME,
                                             privacy_sandbox_settings()))
      .Times(1);

  sync_service()->SetNonEmptyLastCycleSnapshot();
  sync_service()->FireSyncCycleCompleted();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // Removing the management state and firing the policy update listener should
  // result in reconciliation running.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  // The HostContentSettingsMap & PrefService are inspected directly, and not
  // the PolicyMap provided here. The associated browser tests confirm that this
  // is a valid approach.
  privacy_sandbox_settings()->OnPolicyUpdated(
      policy::PolicyNamespace(), policy::PolicyMap(), policy::PolicyMap());

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxSettingsTest, NoReconciliationAlreadyRun) {
  // Reconciliation should not run if it is recorded as already occurring.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kPrivacySandboxPreferencesReconciled,
      std::make_unique<base::Value>(true));

  privacy_sandbox_settings()->MaybeReconcilePrivacySandboxPref();

  // If run, reconciliation would have disabled the sandbox.
  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxApisEnabled));
}

TEST_F(PrivacySandboxSettingsTest, NoReconciliationSandboxSettingsDisabled) {
  // Reconciliation should not run if the privacy sandbox settings are not
  // enabled.
  SetupTestState(
      /*privacy_sandbox_available=*/false,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_settings()->MaybeReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxSettingsTest, MetricsLoggingOccursCorrectly) {
  base::HistogramTester histograms;
  const std::string histogram_name = "Settings.PrivacySandbox.Enabled";

  // The histogram should start off empty.
  histograms.ExpectTotalCount(histogram_name, 0);

  // For buckets that do not explicitly mention FLoC, it is assumed to be on,
  // or its state is irrelevant, i.e. overriden by the Privacy Sandbox pref.
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);

  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_settings()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 1);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
                           kPSEnabledAllowAll),
      1);

  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_settings()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 2);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
                           kPSEnabledBlock3P),
      1);

  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_settings()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 3);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
                           kPSEnabledBlockAll),
      1);

  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kPrivacySandboxPreferencesReconciled,
      std::make_unique<base::Value>(false));
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_settings()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 4);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
                           kPSDisabledAllowAll),
      1);

  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kPrivacySandboxPreferencesReconciled,
      std::make_unique<base::Value>(false));
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_settings()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 5);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
                           kPSDisabledBlock3P),
      1);

  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kPrivacySandboxPreferencesReconciled,
      std::make_unique<base::Value>(false));
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_settings()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 6);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxSettings::PrivacySandboxSettings::
                           SettingsPrivacySandboxEnabled::kPSDisabledBlockAll),
      1);

  // Verify that delayed reconciliation still logs properly.
  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kPrivacySandboxPreferencesReconciled,
      std::make_unique<base::Value>(false));
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  sync_service()->SetEmptyLastCycleSnapshot();

  privacy_sandbox_settings()->MaybeReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  histograms.ExpectTotalCount(histogram_name, 6);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
                           kPSDisabledBlockAll),
      1);

  sync_service()->SetNonEmptyLastCycleSnapshot();
  sync_service()->FireSyncCycleCompleted();

  histograms.ExpectTotalCount(histogram_name, 7);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
                           kPSDisabledBlockAll),
      2);

  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kPrivacySandboxPreferencesReconciled,
      std::make_unique<base::Value>(false));
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_settings()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 8);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
                           kPSDisabledPolicyBlockAll),
      1);

  // Disable FLoC and test the buckets that reflect a disabled FLoC state.
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, false);

  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_settings()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 9);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
                           kPSEnabledFlocDisabledAllowAll),
      1);

  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_settings()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 10);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
                           kPSEnabledFlocDisabledBlock3P),
      1);

  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_settings()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 11);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxSettings::SettingsPrivacySandboxEnabled::
                           kPSEnabledFlocDisabledBlockAll),
      1);
}

class PrivacySandboxSettingsTestCookiesClearOnExitTurnedOff
    : public PrivacySandboxSettingsTest {
 public:
  void InitializePrefsBeforeStart() override {
    profile()->GetTestingPrefService()->SetUserPref(
        prefs::kPrivacySandboxFlocDataAccessibleSince,
        std::make_unique<base::Value>(
            ::util::TimeToValue(base::Time::FromTimeT(12345))));
  }
};

TEST_F(PrivacySandboxSettingsTestCookiesClearOnExitTurnedOff,
       UseLastFlocDataAccessibleSince) {
  EXPECT_EQ(base::Time::FromTimeT(12345),
            privacy_sandbox_settings()->FlocDataAccessibleSince());
}

class PrivacySandboxSettingsTestCookiesClearOnExitTurnedOn
    : public PrivacySandboxSettingsTest {
 public:
  void InitializePrefsBeforeStart() override {
    auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
    map->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                  ContentSetting::CONTENT_SETTING_SESSION_ONLY);

    profile()->GetTestingPrefService()->SetUserPref(
        prefs::kPrivacySandboxFlocDataAccessibleSince,
        std::make_unique<base::Value>(
            ::util::TimeToValue(base::Time::FromTimeT(12345))));
  }
};

TEST_F(PrivacySandboxSettingsTestCookiesClearOnExitTurnedOn,
       UpdateFlocDataAccessibleSince) {
  EXPECT_EQ(base::Time::Now(),
            privacy_sandbox_settings()->FlocDataAccessibleSince());
}
