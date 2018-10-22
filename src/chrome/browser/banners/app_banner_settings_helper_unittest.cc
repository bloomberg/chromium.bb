// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/installable/installable_logging.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"

namespace {

const char kTestURL[] = "https://www.google.com";
const char kSameOriginTestURL[] = "https://www.google.com/foo.html";
const char kDifferentOriginTestURL[] = "https://www.example.com";
const char kTestPackageName[] = "test.package";

base::Time GetReferenceTime() {
  base::Time::Exploded exploded_reference_time;
  exploded_reference_time.year = 2015;
  exploded_reference_time.month = 1;
  exploded_reference_time.day_of_month = 30;
  exploded_reference_time.day_of_week = 5;
  exploded_reference_time.hour = 11;
  exploded_reference_time.minute = 0;
  exploded_reference_time.second = 0;
  exploded_reference_time.millisecond = 0;

  base::Time out_time;
  EXPECT_TRUE(
      base::Time::FromLocalExploded(exploded_reference_time, &out_time));
  return out_time;
}

class AppBannerSettingsHelperTest : public ChromeRenderViewHostTestHarness {
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    AppBannerSettingsHelper::SetDefaultParameters();
  }
};

}  // namespace

TEST_F(AppBannerSettingsHelperTest, SingleEvents) {
  GURL url(kTestURL);
  NavigateAndCommit(url);

  base::Time reference_time = GetReferenceTime();
  base::Time other_time = reference_time - base::TimeDelta::FromDays(3);
  for (int event = AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW;
       event < AppBannerSettingsHelper::APP_BANNER_EVENT_NUM_EVENTS; ++event) {
    // Check that by default, there is no event.
    base::Time event_time = AppBannerSettingsHelper::GetSingleBannerEvent(
        web_contents(), url, kTestPackageName,
        AppBannerSettingsHelper::AppBannerEvent(event));
    EXPECT_TRUE(event_time.is_null());

    // Check that a time can be recorded.
    AppBannerSettingsHelper::RecordBannerEvent(
        web_contents(), url, kTestPackageName,
        AppBannerSettingsHelper::AppBannerEvent(event), reference_time);

    event_time = AppBannerSettingsHelper::GetSingleBannerEvent(
        web_contents(), url, kTestPackageName,
        AppBannerSettingsHelper::AppBannerEvent(event));
    EXPECT_EQ(reference_time, event_time);

    // Check that another time can be recorded.
    AppBannerSettingsHelper::RecordBannerEvent(
        web_contents(), url, kTestPackageName,
        AppBannerSettingsHelper::AppBannerEvent(event), other_time);

    event_time = AppBannerSettingsHelper::GetSingleBannerEvent(
        web_contents(), url, kTestPackageName,
        AppBannerSettingsHelper::AppBannerEvent(event));

    // COULD_SHOW events are not overwritten, but other events are.
    if (event == AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW)
      EXPECT_EQ(reference_time, event_time);
    else
      EXPECT_EQ(other_time, event_time);
  }
}

TEST_F(AppBannerSettingsHelperTest, ShouldShowFromEngagement) {
  GURL url(kTestURL);
  SiteEngagementService* service = SiteEngagementService::Get(profile());

  // By default the banner should not be shown.
  EXPECT_FALSE(
      AppBannerSettingsHelper::HasSufficientEngagement(service->GetScore(url)));

  // Add 1 engagement, it still should not be shown.
  service->ResetBaseScoreForURL(url, 1);
  EXPECT_FALSE(
      AppBannerSettingsHelper::HasSufficientEngagement(service->GetScore(url)));

  // Add 1 more engagement; now it should be shown.
  service->ResetBaseScoreForURL(url, 2);
  EXPECT_TRUE(
      AppBannerSettingsHelper::HasSufficientEngagement(service->GetScore(url)));
}

TEST_F(AppBannerSettingsHelperTest, ReportsWhetherBannerWasRecentlyBlocked) {
  GURL url(kTestURL);
  NavigateAndCommit(url);

  base::Time reference_time = GetReferenceTime();
  base::Time two_months_ago = reference_time - base::TimeDelta::FromDays(60);
  base::Time one_year_ago = reference_time - base::TimeDelta::FromDays(366);

  EXPECT_FALSE(AppBannerSettingsHelper::WasBannerRecentlyBlocked(
      web_contents(), url, kTestPackageName, reference_time));

  // Block the site a long time ago. This should not be considered "recent".
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_BLOCK, one_year_ago);
  EXPECT_FALSE(AppBannerSettingsHelper::WasBannerRecentlyBlocked(
      web_contents(), url, kTestPackageName, reference_time));

  // Block the site more recently.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_BLOCK, two_months_ago);
  EXPECT_TRUE(AppBannerSettingsHelper::WasBannerRecentlyBlocked(
      web_contents(), url, kTestPackageName, reference_time));

  // Change the number of days enforced.
  AppBannerSettingsHelper::ScopedTriggerSettings trigger_settings(59, 14);

  EXPECT_FALSE(AppBannerSettingsHelper::WasBannerRecentlyBlocked(
      web_contents(), url, kTestPackageName, reference_time));
}

TEST_F(AppBannerSettingsHelperTest, ReportsWhetherBannerWasRecentlyIgnored) {
  GURL url(kTestURL);
  NavigateAndCommit(url);

  base::Time reference_time = GetReferenceTime();
  base::Time one_week_ago = reference_time - base::TimeDelta::FromDays(7);
  base::Time one_year_ago = reference_time - base::TimeDelta::FromDays(366);

  EXPECT_FALSE(AppBannerSettingsHelper::WasBannerRecentlyIgnored(
      web_contents(), url, kTestPackageName, reference_time));

  // Show the banner a long time ago. This should not be considered "recent".
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_SHOW, one_year_ago);
  EXPECT_FALSE(AppBannerSettingsHelper::WasBannerRecentlyIgnored(
      web_contents(), url, kTestPackageName, reference_time));

  // Show the site more recently.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_SHOW, one_week_ago);
  EXPECT_TRUE(AppBannerSettingsHelper::WasBannerRecentlyIgnored(
      web_contents(), url, kTestPackageName, reference_time));

  // Change the number of days enforced.
  AppBannerSettingsHelper::ScopedTriggerSettings trigger_settings(90, 6);

  EXPECT_FALSE(AppBannerSettingsHelper::WasBannerRecentlyIgnored(
      web_contents(), url, kTestPackageName, reference_time));
}

TEST_F(AppBannerSettingsHelperTest, ReportsWhetherSiteWasEverAdded) {
  GURL url(kTestURL);
  base::Time one_year_ago = GetReferenceTime() - base::TimeDelta::FromDays(366);

  EXPECT_FALSE(AppBannerSettingsHelper::HasBeenInstalled(web_contents(), url,
                                                         kTestPackageName));

  // Add the site a long time ago.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
      one_year_ago);

  EXPECT_TRUE(AppBannerSettingsHelper::HasBeenInstalled(web_contents(), url,
                                                        kTestPackageName));
}

TEST_F(AppBannerSettingsHelperTest, OperatesOnOrigins) {
  GURL url(kTestURL);
  GURL otherURL(kSameOriginTestURL);

  SiteEngagementService* service = SiteEngagementService::Get(profile());

  // By default the banner should not be shown.
  EXPECT_FALSE(
      AppBannerSettingsHelper::HasSufficientEngagement(service->GetScore(url)));

  // Add engagement such that the banner should show.
  service->ResetBaseScoreForURL(url, 4);
  EXPECT_TRUE(
      AppBannerSettingsHelper::HasSufficientEngagement(service->GetScore(url)));

  // The banner should show as settings are per-origin.
  EXPECT_TRUE(AppBannerSettingsHelper::HasSufficientEngagement(
      service->GetScore(otherURL)));

  base::Time reference_time = GetReferenceTime();
  base::Time one_week_ago = reference_time - base::TimeDelta::FromDays(7);

  // If url is blocked, otherURL will also be reported as blocked.
  EXPECT_FALSE(AppBannerSettingsHelper::WasBannerRecentlyBlocked(
      web_contents(), otherURL, kTestPackageName, reference_time));
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_BLOCK, one_week_ago);
  EXPECT_TRUE(AppBannerSettingsHelper::WasBannerRecentlyBlocked(
      web_contents(), otherURL, kTestPackageName, reference_time));

  // If url is ignored, otherURL will also be reported as ignored.
  EXPECT_FALSE(AppBannerSettingsHelper::WasBannerRecentlyIgnored(
      web_contents(), otherURL, kTestPackageName, reference_time));
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_SHOW, one_week_ago);
  EXPECT_TRUE(AppBannerSettingsHelper::WasBannerRecentlyIgnored(
      web_contents(), otherURL, kTestPackageName, reference_time));
}

TEST_F(AppBannerSettingsHelperTest, ShouldShowWithHigherTotal) {
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(10);
  GURL url(kTestURL);
  SiteEngagementService* service = SiteEngagementService::Get(profile());

  // By default the banner should not be shown.
  EXPECT_FALSE(
      AppBannerSettingsHelper::HasSufficientEngagement(service->GetScore(url)));

  // Add engagement such that the banner should show.
  service->ResetBaseScoreForURL(url, 2);
  EXPECT_FALSE(
      AppBannerSettingsHelper::HasSufficientEngagement(service->GetScore(url)));

  service->ResetBaseScoreForURL(url, 4);
  EXPECT_FALSE(
      AppBannerSettingsHelper::HasSufficientEngagement(service->GetScore(url)));

  service->ResetBaseScoreForURL(url, 6);
  EXPECT_FALSE(
      AppBannerSettingsHelper::HasSufficientEngagement(service->GetScore(url)));

  service->ResetBaseScoreForURL(url, 8);
  EXPECT_FALSE(
      AppBannerSettingsHelper::HasSufficientEngagement(service->GetScore(url)));

  service->ResetBaseScoreForURL(url, 10);
  EXPECT_TRUE(
      AppBannerSettingsHelper::HasSufficientEngagement(service->GetScore(url)));
}

TEST_F(AppBannerSettingsHelperTest, WasLaunchedRecently) {
  GURL url(kTestURL);
  GURL url_same_origin(kSameOriginTestURL);
  GURL url2(kDifferentOriginTestURL);
  NavigateAndCommit(url);

  base::Time reference_time = GetReferenceTime();
  base::Time first_day = reference_time + base::TimeDelta::FromDays(1);
  base::Time ninth_day = reference_time + base::TimeDelta::FromDays(9);
  base::Time tenth_day = reference_time + base::TimeDelta::FromDays(10);
  base::Time eleventh_day = reference_time + base::TimeDelta::FromDays(11);

  EXPECT_FALSE(AppBannerSettingsHelper::WasLaunchedRecently(profile(), url,
                                                            reference_time));

  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
      reference_time);
  EXPECT_TRUE(AppBannerSettingsHelper::WasLaunchedRecently(profile(), url,
                                                           reference_time));
  EXPECT_TRUE(
      AppBannerSettingsHelper::WasLaunchedRecently(profile(), url, first_day));
  EXPECT_TRUE(
      AppBannerSettingsHelper::WasLaunchedRecently(profile(), url, ninth_day));
  EXPECT_TRUE(
      AppBannerSettingsHelper::WasLaunchedRecently(profile(), url, tenth_day));
  EXPECT_FALSE(AppBannerSettingsHelper::WasLaunchedRecently(profile(), url,
                                                            eleventh_day));

  // Make sure a different path under the same origin also returns true.
  EXPECT_TRUE(AppBannerSettingsHelper::WasLaunchedRecently(
      profile(), url_same_origin, reference_time));
  EXPECT_TRUE(AppBannerSettingsHelper::WasLaunchedRecently(
      profile(), url_same_origin, first_day));
  EXPECT_TRUE(AppBannerSettingsHelper::WasLaunchedRecently(
      profile(), url_same_origin, ninth_day));
  EXPECT_TRUE(AppBannerSettingsHelper::WasLaunchedRecently(
      profile(), url_same_origin, tenth_day));
  EXPECT_FALSE(AppBannerSettingsHelper::WasLaunchedRecently(
      profile(), url_same_origin, eleventh_day));

  // Check a different event type.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url2, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_SHOW, reference_time);

  EXPECT_FALSE(AppBannerSettingsHelper::WasLaunchedRecently(profile(), url2,
                                                            reference_time));
  EXPECT_FALSE(
      AppBannerSettingsHelper::WasLaunchedRecently(profile(), url2, first_day));
  EXPECT_FALSE(
      AppBannerSettingsHelper::WasLaunchedRecently(profile(), url2, ninth_day));

  // Make sure that the most recent time the event is recorded is used.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
      first_day);
  EXPECT_TRUE(
      AppBannerSettingsHelper::WasLaunchedRecently(profile(), url, first_day));
  EXPECT_TRUE(
      AppBannerSettingsHelper::WasLaunchedRecently(profile(), url, ninth_day));
  EXPECT_TRUE(
      AppBannerSettingsHelper::WasLaunchedRecently(profile(), url, tenth_day));
  EXPECT_TRUE(AppBannerSettingsHelper::WasLaunchedRecently(profile(), url,
                                                           eleventh_day));
}
