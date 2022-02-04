// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;

class TrustSafetySentimentServiceTest : public testing::Test {
 public:
  TrustSafetySentimentServiceTest() {
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockHatsService)));
    EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey(_))
        .WillRepeatedly(testing::Return(true));
  }

 protected:
  struct FeatureParams {
    std::string privacy_settings_time = "20s";
    std::string min_time_to_prompt = "2m";
    std::string max_time_to_prompt = "60m";
    std::string ntp_visits_min_range = "2";
    std::string ntp_visits_max_range = "4";
    std::string privacy_settings_probability = "0.6";
    std::string trusted_surface_probability = "0.4";
    std::string transactions_probability = "0.05";
    std::string privacy_settings_trigger_id = "privacy-settings-test";
    std::string trusted_surface_trigger_id = "trusted-surface-test";
    std::string transactions_trigger_id = "transactions-test";
    std::string transactions_password_manager_time = "20s";
  };

  void SetupFeatureParameters(FeatureParams params) {
    feature_list()->InitAndEnableFeatureWithParameters(
        features::kTrustSafetySentimentSurvey,
        {
            {"privacy-settings-time", params.privacy_settings_time},
            {"min-time-to-prompt", params.min_time_to_prompt},
            {"max-time-to-prompt", params.max_time_to_prompt},
            {"ntp-visits-min-range", params.ntp_visits_min_range},
            {"ntp-visits-max-range", params.ntp_visits_max_range},
            {"privacy-settings-probability",
             params.privacy_settings_probability},
            {"trusted-surface-probability", params.trusted_surface_probability},
            {"transactions-probability", params.transactions_probability},
            {"privacy-settings-trigger-id", params.privacy_settings_trigger_id},
            {"trusted-surface-trigger-id", params.trusted_surface_trigger_id},
            {"transactions-trigger-id", params.transactions_trigger_id},
            {"transactions-password-manager-time",
             params.transactions_password_manager_time},
        });
  }

  void CheckHistograms(
      const std::set<TrustSafetySentimentService::FeatureArea>& triggered_areas,
      const std::set<TrustSafetySentimentService::FeatureArea>&
          surveyed_areas) {
    std::map<std::string, std::set<TrustSafetySentimentService::FeatureArea>>
        histogram_to_expected = {
            {"Feedback.TrustSafetySentiment.TriggerOccurred", triggered_areas},
            {"Feedback.TrustSafetySentiment.SurveyRequested", surveyed_areas}};

    for (const auto& histogram_expected : histogram_to_expected) {
      const auto& histogram_name = histogram_expected.first;
      const auto& expected = histogram_expected.second;
      histogram_tester()->ExpectTotalCount(histogram_name, expected.size());
      for (auto area : expected) {
        histogram_tester()->ExpectBucketCount(histogram_name, area, 1);
      }
    }
  }

  TrustSafetySentimentService* service() {
    return TrustSafetySentimentServiceFactory::GetForProfile(profile());
  }
  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }
  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }
  MockHatsService* mock_hats_service() { return mock_hats_service_; }
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfile profile_;
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
  raw_ptr<MockHatsService> mock_hats_service_;
};

TEST_F(TrustSafetySentimentServiceTest, Eligibility_NtpOpens) {
  // A survey should not be shown if not enough NTPs have been opened since
  // the most recent trigger action.
  FeatureParams params;
  params.privacy_settings_probability = "1.0";
  params.trusted_surface_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "2";
  params.ntp_visits_max_range = "2";
  SetupFeatureParameters(params);

  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);

  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kPrivacySettings, {});
  service()->OpenedNewTabPage();
  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kTrustedSurface, {});

  // The Trusted Surface trigger should prevent a survey from being shown, as
  // it still has 1 NTP to open.
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  CheckHistograms({TrustSafetySentimentService::FeatureArea::kPrivacySettings,
                   TrustSafetySentimentService::FeatureArea::kTrustedSurface},
                  {});

  // The next NTP should be eligible for a survey.
  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _));
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, Eligibility_Time) {
  // A survey should not be shown if any trigger action occurred to recently, or
  // if all trigger actions occurred too long ago.
  FeatureParams params;
  params.privacy_settings_probability = "1.0";
  params.trusted_surface_probability = "1.0";
  params.min_time_to_prompt = "1m";
  params.max_time_to_prompt = "10m";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kPrivacySettings, {});
  service()->OpenedNewTabPage();

  task_environment()->AdvanceClock(base::Minutes(2));
  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kTrustedSurface, {});
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // Moving the clock forward such that only the trusted surface trigger is
  // within the window should guarantee it is the survey shown.
  task_environment()->AdvanceClock(base::Minutes(9));
  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyTrustedSurface, _, _, _, _));
  service()->OpenedNewTabPage();

  CheckHistograms({TrustSafetySentimentService::FeatureArea::kPrivacySettings,
                   TrustSafetySentimentService::FeatureArea::kTrustedSurface},
                  {TrustSafetySentimentService::FeatureArea::kTrustedSurface});
}

TEST_F(TrustSafetySentimentServiceTest, TriggerProbability) {
  // Triggers which fail the probability check should not be considered.
  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  FeatureParams params;
  params.trusted_surface_probability = "0.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  SetupFeatureParameters(params);

  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kTrustedSurface, {});
  service()->OpenedNewTabPage();
  CheckHistograms({}, {});
}

TEST_F(TrustSafetySentimentServiceTest, TriggersClearOnLaunch) {
  // Check that all active triggers are cleared when a survey is launched.
  FeatureParams params;
  params.trusted_surface_probability = "1.0";
  params.privacy_settings_probability = "1.0";
  params.transactions_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kPrivacySettings, {});
  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kTrustedSurface, {});

  CheckHistograms({TrustSafetySentimentService::FeatureArea::kPrivacySettings,
                   TrustSafetySentimentService::FeatureArea::kTrustedSurface},
                  {});

  // The launched survey will be randomly selected from the two triggers.
  std::string requested_survey_trigger;
  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _))
      .WillOnce(testing::SaveArg<0>(&requested_survey_trigger));
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());
  auto surveyed_feature_area =
      requested_survey_trigger == kHatsSurveyTriggerTrustSafetyPrivacySettings
          ? TrustSafetySentimentService::FeatureArea::kPrivacySettings
          : TrustSafetySentimentService::FeatureArea::kTrustedSurface;

  // The trigger which did not result in a survey should no longer be
  // considered.
  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // Repeated triggers post survey launch should however be considered.
  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyTransactions, _, _, _, _));
  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kTransactions, {});
  service()->OpenedNewTabPage();

  CheckHistograms({TrustSafetySentimentService::FeatureArea::kPrivacySettings,
                   TrustSafetySentimentService::FeatureArea::kTrustedSurface,
                   TrustSafetySentimentService::FeatureArea::kTransactions},
                  {surveyed_feature_area,
                   TrustSafetySentimentService::FeatureArea::kTransactions});
}

TEST_F(TrustSafetySentimentServiceTest, SettingsWatcher_PrivacySettings) {
  FeatureParams params;
  params.privacy_settings_probability = "1.0";
  params.privacy_settings_time = "10s";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  // Create and navigate a test web contents to settings.
  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(GURL(chrome::kChromeUISettingsURL));

  // Interacting with setting shouldn't causes a survey to be immediately
  // displayed, but should require the user to stay on settings for some time.
  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  service()->InteractedWithPrivacySettings(web_contents.get());
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // Once the user has spent the appropriate amount of time on settings, they
  // should be eligible for a survey.
  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _, _, _));
  task_environment()->AdvanceClock(base::Seconds(20));
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // Leaving settings before the required time should disqualify the user from
  // receiving a survey.
  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  service()->InteractedWithPrivacySettings(web_contents.get());
  task_environment()->AdvanceClock(base::Seconds(5));
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();

  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(GURL("http://unrelated.com"));
  task_environment()->AdvanceClock(base::Seconds(15));
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, SettingsWatcher_PasswordManager) {
  FeatureParams params;
  params.transactions_probability = "1.0";
  params.transactions_password_manager_time = "10s";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  // Check that after being informed of a visit to the password manager page,
  // the service correctly watches the provided WebContents to check if the
  // user stays on settings.
  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(GURL(chrome::kChromeUISettingsURL));

  // A survey should not be shown unless the user spends at least the required
  // time on settings after opening password manager.
  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  service()->OpenedPasswordManager(web_contents.get());
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // Once the user has spent sufficient time on settings after visiting the
  // password manager, they should be eligible for the Transactions copy of the
  // survey.
  SurveyBitsData expected_psd = {{"Saved password", false}};
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyTransactions, _, _,
                           expected_psd, _));

  task_environment()->AdvanceClock(base::Seconds(20));
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // Leaving settings before the required time should not make the user
  // eligible.
  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  service()->OpenedPasswordManager(web_contents.get());
  task_environment()->AdvanceClock(base::Seconds(5));
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();

  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(GURL("http://unrelated.com"));
  task_environment()->AdvanceClock(base::Seconds(15));
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, RanSafetyCheck) {
  // Running the safety check is considered a trigger, and should make a user
  // eligible to receive a survey.
  FeatureParams params;
  params.privacy_settings_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _, _, _));
  service()->RanSafetyCheck();
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, SavedPassword) {
  // Saving a password is considered a trigger, and should make a user eligible
  // to receive a survey.
  FeatureParams params;
  params.transactions_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  SurveyBitsData expected_psd = {{"Saved password", true}};

  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyTransactions, _, _,
                           expected_psd, _));
  service()->SavedPassword();
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, SavedCard) {
  // Saving a card is considered a trigger, and should make a user eligible
  // to receive a survey.
  FeatureParams params;
  params.transactions_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  SurveyBitsData expected_psd = {{"Saved password", false}};

  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyTransactions, _, _,
                           expected_psd, _));
  service()->SavedCard();
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, PrivacySettingsProductSpecificData) {
  // Check the product specific data accompanying surveys for the Privacy
  // Settings feature area correctly records whether the user has a non default
  // privacy setting.
  FeatureParams params;
  params.privacy_settings_probability = "1.0";
  params.privacy_settings_time = "0s";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(GURL(chrome::kChromeUISettingsURL));

  SurveyBitsData expected_psd = {{"Non default setting", false},
                                 {"Ran safety check", false}};

  // By default, a user should have no non-default settings.
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _,
                           expected_psd, _));
  service()->InteractedWithPrivacySettings(web_contents.get());
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  expected_psd["Ran safety check"] = true;
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _,
                           expected_psd, _));
  service()->RanSafetyCheck();
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // Check that default content settings are considered.
  expected_psd["Non default setting"] = true;
  auto* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile());
  content_settings->SetDefaultContentSetting(
      ContentSettingsType::SOUND, ContentSetting::CONTENT_SETTING_BLOCK);
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _,
                           expected_psd, _));
  service()->RanSafetyCheck();
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());
  content_settings->SetDefaultContentSetting(
      ContentSettingsType::SOUND, ContentSetting::CONTENT_SETTING_DEFAULT);

  // Check that preferences are considered.
  expected_psd["Ran safety check"] = false;
  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kEnableDoNotTrack, std::make_unique<base::Value>(true));
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _,
                           expected_psd, _));
  service()->InteractedWithPrivacySettings(web_contents.get());
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());
  profile()->GetPrefs()->ClearPref(prefs::kEnableDoNotTrack);

  // Check that sync state defaults are handled correctly.
  profile()->GetTestingPrefService()->SetUserPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _,
                           expected_psd, _));
  service()->InteractedWithPrivacySettings(web_contents.get());
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // UKM is only non default while no sync consent is present.
  expected_psd["Non default setting"] = false;
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _,
                           expected_psd, _));
  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kGoogleServicesConsentedToSync,
      std::make_unique<base::Value>(true));
  service()->InteractedWithPrivacySettings(web_contents.get());
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();

  // A preference or content setting changed via policy should not be considered
  // as non-default.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kEnableDoNotTrack, std::make_unique<base::Value>(true));
  auto managed_provider = std::make_unique<content_settings::MockProvider>();
  managed_provider->SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES,
      base::Value(ContentSetting::CONTENT_SETTING_BLOCK));
  content_settings::TestUtils::OverrideProvider(
      content_settings, std::move(managed_provider),
      HostContentSettingsMap::POLICY_PROVIDER);
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _,
                           expected_psd, _));
  service()->InteractedWithPrivacySettings(web_contents.get());
  task_environment()->RunUntilIdle();
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, ActiveIncognitoPreventsSurvey) {
  // Check that when an incognito profile exists that a survey is not shown.
  FeatureParams params;
  params.privacy_settings_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  auto* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  service()->RanSafetyCheck();
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  profile()->DestroyOffTheRecordProfile(otr_profile);
  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _, _, _));
  service()->OpenedNewTabPage();
  CheckHistograms({TrustSafetySentimentService::FeatureArea::kPrivacySettings,
                   TrustSafetySentimentService::FeatureArea::kIneligible},
                  {TrustSafetySentimentService::FeatureArea::kPrivacySettings});
}

TEST_F(TrustSafetySentimentServiceTest, ClosingIncognitoDelaysSurvey) {
  // Check that closing an incognito session delays when a survey is shown
  // by the minimum time to prompt, and the maximum of the range of NTP visits.
  FeatureParams params;
  params.privacy_settings_probability = "1.0";
  params.min_time_to_prompt = "1m";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "2";
  SetupFeatureParameters(params);

  auto* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_CALL(*mock_hats_service(), LaunchSurvey(_, _, _, _, _)).Times(0);
  service()->RanSafetyCheck();

  // Record 2 visits to the NTP so regardless of the random NTP count chosen,
  // the Privacy Settings trigger will be eligible, but currently blocked by
  // the presence of an incognito profile.
  for (int i = 0; i < 2; i++)
    service()->OpenedNewTabPage();

  profile()->DestroyOffTheRecordProfile(otr_profile);

  // The first NTP opened after closing the incognito session should never
  // result in a survey, as the maximum of the range is 2.
  service()->OpenedNewTabPage();

  CheckHistograms({TrustSafetySentimentService::FeatureArea::kPrivacySettings,
                   TrustSafetySentimentService::FeatureArea::kIneligible},
                  {});

  // The second visit to the NTP should not trigger a survey if it takes place
  // less than the minimum time to prompt after closing an incognito session.
  task_environment()->AdvanceClock(base::Seconds(30));
  service()->OpenedNewTabPage();

  // Up to this point no attempt to show any survey should have been made.
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  EXPECT_CALL(
      *mock_hats_service(),
      LaunchSurvey(kHatsSurveyTriggerTrustSafetyPrivacySettings, _, _, _, _));

  // The next tab open which occurs after the required number of opens, and the
  // minimum time has passed, should trigger a survey.
  task_environment()->AdvanceClock(base::Minutes(1));
  service()->OpenedNewTabPage();

  CheckHistograms({TrustSafetySentimentService::FeatureArea::kPrivacySettings,
                   TrustSafetySentimentService::FeatureArea::kIneligible},
                  {TrustSafetySentimentService::FeatureArea::kPrivacySettings});
}
