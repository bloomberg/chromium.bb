// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_decision_auto_blocker.h"

#include <map>
#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"

namespace permissions {
namespace {

bool FilterGoogle(const GURL& url) {
  return url == "https://www.google.com/";
}

bool FilterAll(const GURL& url) {
  return true;
}

}  // namespace

class PermissionDecisionAutoBlockerUnitTest : public testing::Test {
 protected:
  PermissionDecisionAutoBlockerUnitTest() {
    feature_list_.InitWithFeatures({features::kBlockPromptsIfDismissedOften,
                                    features::kBlockPromptsIfIgnoredOften},
                                   {});
    last_embargoed_status_ = false;
    autoblocker()->SetClockForTesting(&clock_);
    callback_was_run_ = false;
  }

  PermissionDecisionAutoBlocker* autoblocker() {
    return PermissionsClient::Get()->GetPermissionDecisionAutoBlocker(
        &browser_context_);
  }

  void SetLastEmbargoStatus(base::Closure quit_closure, bool status) {
    callback_was_run_ = true;
    last_embargoed_status_ = status;
    if (quit_closure) {
      quit_closure.Run();
      quit_closure.Reset();
    }
  }

  bool last_embargoed_status() { return last_embargoed_status_; }

  bool callback_was_run() { return callback_was_run_; }

  base::SimpleTestClock* clock() { return &clock_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::SimpleTestClock clock_;
  base::test::ScopedFeatureList feature_list_;
  content::TestBrowserContext browser_context_;
  TestPermissionsClient permissions_client_;
  bool last_embargoed_status_;
  bool callback_was_run_;
};

// Check removing the the embargo for a single permission on a site works, and
// that it doesn't interfere with other embargoed permissions or the same
// permission embargoed on other sites.
TEST_F(PermissionDecisionAutoBlockerUnitTest, RemoveEmbargoAndResetCounts) {
  GURL url1("https://www.google.com");
  GURL url2("https://www.example.com");

  // Record dismissals for location and notifications in |url1|.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::GEOLOCATION, false));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::GEOLOCATION, false));
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::GEOLOCATION, false));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::NOTIFICATIONS, false));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::NOTIFICATIONS, false));
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::NOTIFICATIONS, false));
  // Record dismissals for location in |url2|.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url2, ContentSettingsType::GEOLOCATION, false));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url2, ContentSettingsType::GEOLOCATION, false));
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url2, ContentSettingsType::GEOLOCATION, false));

  // Verify all dismissals recorded above resulted in embargo.
  PermissionResult result =
      autoblocker()->GetEmbargoResult(url1, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
  result =
      autoblocker()->GetEmbargoResult(url1, ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
  result =
      autoblocker()->GetEmbargoResult(url2, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);

  // Remove the embargo on notifications. Verify it is no longer under embargo,
  // but location still is.
  autoblocker()->RemoveEmbargoAndResetCounts(
      url1, ContentSettingsType::NOTIFICATIONS);
  result =
      autoblocker()->GetEmbargoResult(url1, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
  result =
      autoblocker()->GetEmbargoResult(url1, ContentSettingsType::NOTIFICATIONS);
  // If not under embargo, GetEmbargoResult() returns a setting of ASK.
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
  // Verify |url2|'s embargo is still intact as well.
  result =
      autoblocker()->GetEmbargoResult(url2, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
}

// Test it does not take one more dismissal to re-trigger embargo after
// removing the embargo status for a site.
TEST_F(PermissionDecisionAutoBlockerUnitTest,
       DismissAfterRemovingEmbargoByURL) {
  GURL url("https://www.example.com");

  // Record dismissals for location.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION, false));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION, false));
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION, false));

  // Verify location is under embargo.
  PermissionResult result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);

  // Remove embargo and verify this is true.
  autoblocker()->RemoveEmbargoAndResetCounts(url,
                                             ContentSettingsType::GEOLOCATION);
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Record another dismissal and verify location is not under embargo again.
  autoblocker()->RecordDismissAndEmbargo(url, ContentSettingsType::GEOLOCATION,
                                         false);
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
}

TEST_F(PermissionDecisionAutoBlockerUnitTest,
       NonembargoedOriginRemoveEmbargoCounts) {
  GURL gurl_to_embargo("https://www.google.com");

  // Make sure that an origin's Dismiss count is 0.
  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   gurl_to_embargo, ContentSettingsType::GEOLOCATION));

  // Dismiss the origin a few times but do not add under embargo.
  autoblocker()->RecordDismissAndEmbargo(
      gurl_to_embargo, ContentSettingsType::GEOLOCATION, false);
  autoblocker()->RecordDismissAndEmbargo(
      gurl_to_embargo, ContentSettingsType::GEOLOCATION, false);

  EXPECT_EQ(2, autoblocker()->GetDismissCount(
                   gurl_to_embargo, ContentSettingsType::GEOLOCATION));

  autoblocker()->RemoveEmbargoAndResetCounts(gurl_to_embargo,
                                             ContentSettingsType::GEOLOCATION);

  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   gurl_to_embargo, ContentSettingsType::GEOLOCATION));

  autoblocker()->RecordIgnoreAndEmbargo(gurl_to_embargo,
                                        ContentSettingsType::MIDI_SYSEX, false);
  autoblocker()->RecordIgnoreAndEmbargo(gurl_to_embargo,
                                        ContentSettingsType::MIDI_SYSEX, false);

  EXPECT_EQ(2, autoblocker()->GetIgnoreCount(gurl_to_embargo,
                                             ContentSettingsType::MIDI_SYSEX));

  autoblocker()->RemoveEmbargoAndResetCounts(gurl_to_embargo,
                                             ContentSettingsType::MIDI_SYSEX);

  EXPECT_EQ(0, autoblocker()->GetIgnoreCount(gurl_to_embargo,
                                             ContentSettingsType::MIDI_SYSEX));
}

TEST_F(PermissionDecisionAutoBlockerUnitTest, RemoveEmbargoCounts) {
  GURL gurl_to_embargo("https://www.google.com");

  // Add an origin under embargo for 2 dismissed and 1 ignored
  // ContentSettingsType.
  autoblocker()->RecordDismissAndEmbargo(
      gurl_to_embargo, ContentSettingsType::GEOLOCATION, false);
  autoblocker()->RecordDismissAndEmbargo(
      gurl_to_embargo, ContentSettingsType::GEOLOCATION, false);
  autoblocker()->RecordDismissAndEmbargo(
      gurl_to_embargo, ContentSettingsType::GEOLOCATION, false);

  EXPECT_EQ(3, autoblocker()->GetDismissCount(
                   gurl_to_embargo, ContentSettingsType::GEOLOCATION));

  autoblocker()->RecordDismissAndEmbargo(
      gurl_to_embargo, ContentSettingsType::NOTIFICATIONS, false);
  autoblocker()->RecordDismissAndEmbargo(
      gurl_to_embargo, ContentSettingsType::NOTIFICATIONS, false);
  autoblocker()->RecordDismissAndEmbargo(
      gurl_to_embargo, ContentSettingsType::NOTIFICATIONS, false);

  EXPECT_EQ(3, autoblocker()->GetDismissCount(
                   gurl_to_embargo, ContentSettingsType::NOTIFICATIONS));

  autoblocker()->RecordIgnoreAndEmbargo(gurl_to_embargo,
                                        ContentSettingsType::MIDI_SYSEX, false);
  autoblocker()->RecordIgnoreAndEmbargo(gurl_to_embargo,
                                        ContentSettingsType::MIDI_SYSEX, false);
  autoblocker()->RecordIgnoreAndEmbargo(gurl_to_embargo,
                                        ContentSettingsType::MIDI_SYSEX, false);
  autoblocker()->RecordIgnoreAndEmbargo(gurl_to_embargo,
                                        ContentSettingsType::MIDI_SYSEX, false);

  EXPECT_EQ(4, autoblocker()->GetIgnoreCount(gurl_to_embargo,
                                             ContentSettingsType::MIDI_SYSEX));

  autoblocker()->RemoveEmbargoAndResetCounts(gurl_to_embargo,
                                             ContentSettingsType::GEOLOCATION);

  // GEOLOCATION has been cleared, a dismiss count should be 0.
  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   gurl_to_embargo, ContentSettingsType::GEOLOCATION));
  // GEOLOCATION has been cleared, but other counts should be
  // preservet.
  EXPECT_EQ(3, autoblocker()->GetDismissCount(
                   gurl_to_embargo, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(4, autoblocker()->GetIgnoreCount(gurl_to_embargo,
                                             ContentSettingsType::MIDI_SYSEX));
}

TEST_F(PermissionDecisionAutoBlockerUnitTest, RemoveEmbargoAndResetCounts_All) {
  GURL url1("https://www.google.com");
  GURL url2("https://www.example.com");

  // Record some dismissals.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::GEOLOCATION, false));
  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url1, ContentSettingsType::GEOLOCATION));

  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::GEOLOCATION, false));
  EXPECT_EQ(2, autoblocker()->GetDismissCount(
                   url1, ContentSettingsType::GEOLOCATION));

  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::GEOLOCATION, false));
  EXPECT_EQ(3, autoblocker()->GetDismissCount(
                   url1, ContentSettingsType::GEOLOCATION));

  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url2, ContentSettingsType::GEOLOCATION, false));
  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url2, ContentSettingsType::GEOLOCATION));

  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::NOTIFICATIONS, false));
  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url1, ContentSettingsType::NOTIFICATIONS));

  // Record some ignores.
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url1, ContentSettingsType::MIDI_SYSEX, false));
  EXPECT_EQ(
      1, autoblocker()->GetIgnoreCount(url1, ContentSettingsType::MIDI_SYSEX));
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url1, ContentSettingsType::DURABLE_STORAGE, false));
  EXPECT_EQ(1, autoblocker()->GetIgnoreCount(
                   url1, ContentSettingsType::DURABLE_STORAGE));
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url2, ContentSettingsType::GEOLOCATION, false));
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url2, ContentSettingsType::GEOLOCATION, false));
  EXPECT_EQ(
      2, autoblocker()->GetIgnoreCount(url2, ContentSettingsType::GEOLOCATION));

  autoblocker()->RemoveEmbargoAndResetCounts(base::Bind(&FilterGoogle));

  // Expect that url1's actions are gone, but url2's remain.
  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   url1, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   url1, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(
      0, autoblocker()->GetIgnoreCount(url1, ContentSettingsType::MIDI_SYSEX));
  EXPECT_EQ(0, autoblocker()->GetIgnoreCount(
                   url1, ContentSettingsType::DURABLE_STORAGE));

  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url2, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(
      2, autoblocker()->GetIgnoreCount(url2, ContentSettingsType::GEOLOCATION));

  // Add some more actions.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::GEOLOCATION, false));
  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url1, ContentSettingsType::GEOLOCATION));

  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::NOTIFICATIONS, false));
  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url1, ContentSettingsType::NOTIFICATIONS));

  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url2, ContentSettingsType::GEOLOCATION, false));
  EXPECT_EQ(2, autoblocker()->GetDismissCount(
                   url2, ContentSettingsType::GEOLOCATION));

  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url1, ContentSettingsType::GEOLOCATION, false));
  EXPECT_EQ(
      1, autoblocker()->GetIgnoreCount(url1, ContentSettingsType::GEOLOCATION));
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url1, ContentSettingsType::NOTIFICATIONS, false));
  EXPECT_EQ(1, autoblocker()->GetIgnoreCount(
                   url1, ContentSettingsType::NOTIFICATIONS));
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url1, ContentSettingsType::DURABLE_STORAGE, false));
  EXPECT_EQ(1, autoblocker()->GetIgnoreCount(
                   url1, ContentSettingsType::DURABLE_STORAGE));
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url2, ContentSettingsType::MIDI_SYSEX, false));
  EXPECT_EQ(
      1, autoblocker()->GetIgnoreCount(url2, ContentSettingsType::MIDI_SYSEX));

  // Remove everything and expect that it's all gone.
  autoblocker()->RemoveEmbargoAndResetCounts(base::Bind(&FilterAll));

  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   url1, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   url1, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   url2, ContentSettingsType::GEOLOCATION));

  EXPECT_EQ(
      0, autoblocker()->GetIgnoreCount(url1, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(0, autoblocker()->GetIgnoreCount(
                   url1, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(
      0, autoblocker()->GetIgnoreCount(url2, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(0, autoblocker()->GetIgnoreCount(
                   url2, ContentSettingsType::DURABLE_STORAGE));
  EXPECT_EQ(
      0, autoblocker()->GetIgnoreCount(url2, ContentSettingsType::MIDI_SYSEX));
}

// Check that we do not apply embargo to the plugins content type, as prompts
// should be triggered only when necessary by Html5ByDefault.
TEST_F(PermissionDecisionAutoBlockerUnitTest,
       PluginsNotEmbargoedByMultipleDismissesOrIgnores) {
  GURL url("https://www.google.com");

  // Check dismisses first.
  autoblocker()->RecordDismissAndEmbargo(url, ContentSettingsType::PLUGINS,
                                         false);
  autoblocker()->RecordDismissAndEmbargo(url, ContentSettingsType::PLUGINS,
                                         false);
  PermissionResult result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::PLUGINS);

  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
  EXPECT_EQ(2,
            autoblocker()->GetDismissCount(url, ContentSettingsType::PLUGINS));

  // The third dismiss would normally embargo, but this shouldn't happen for
  // plugins.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::PLUGINS, false));
  result = autoblocker()->GetEmbargoResult(url, ContentSettingsType::PLUGINS);

  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
  EXPECT_EQ(3,
            autoblocker()->GetDismissCount(url, ContentSettingsType::PLUGINS));

  // Extra one for sanity checking.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::PLUGINS, false));
  result = autoblocker()->GetEmbargoResult(url, ContentSettingsType::PLUGINS);

  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
  EXPECT_EQ(4,
            autoblocker()->GetDismissCount(url, ContentSettingsType::PLUGINS));

  // Check ignores.
  autoblocker()->RecordIgnoreAndEmbargo(url, ContentSettingsType::PLUGINS,
                                        false);
  autoblocker()->RecordIgnoreAndEmbargo(url, ContentSettingsType::PLUGINS,
                                        false);
  autoblocker()->RecordIgnoreAndEmbargo(url, ContentSettingsType::PLUGINS,
                                        false);
  result = autoblocker()->GetEmbargoResult(url, ContentSettingsType::PLUGINS);

  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
  EXPECT_EQ(3,
            autoblocker()->GetIgnoreCount(url, ContentSettingsType::PLUGINS));

  // The fourth ignore would normally embargo, but this shouldn't happen for
  // plugins.
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url, ContentSettingsType::PLUGINS, false));
  result = autoblocker()->GetEmbargoResult(url, ContentSettingsType::PLUGINS);

  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
  EXPECT_EQ(4,
            autoblocker()->GetIgnoreCount(url, ContentSettingsType::PLUGINS));

  // Extra one for sanity checking.
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url, ContentSettingsType::PLUGINS, false));
  result = autoblocker()->GetEmbargoResult(url, ContentSettingsType::PLUGINS);

  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
  EXPECT_EQ(5,
            autoblocker()->GetIgnoreCount(url, ContentSettingsType::PLUGINS));
}

// Check that GetEmbargoedOrigins only returns origins where embargo is the
// effective permission enforcement mechanism.
TEST_F(PermissionDecisionAutoBlockerUnitTest, CheckEmbargoedOrigins) {
  GURL url1("https://www.google.com");
  GURL url2("https://www.google.com:8443");
  std::vector<ContentSettingsType> content_types = {
      ContentSettingsType::GEOLOCATION, ContentSettingsType::NOTIFICATIONS};
  std::set<GURL> origins;
  clock()->SetNow(base::Time::Now());

  EXPECT_EQ(0UL, autoblocker()->GetEmbargoedOrigins(content_types).size());

  // Place both origins under embargo and verify.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::GEOLOCATION, false));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::GEOLOCATION, false));
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url1, ContentSettingsType::GEOLOCATION, false));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url2, ContentSettingsType::NOTIFICATIONS, false));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url2, ContentSettingsType::NOTIFICATIONS, false));
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url2, ContentSettingsType::NOTIFICATIONS, false));

  origins = autoblocker()->GetEmbargoedOrigins(content_types);
  EXPECT_EQ(2UL, origins.size());
  EXPECT_EQ(1UL, origins.count(url1));
  EXPECT_EQ(1UL, origins.count(url2));

  // Check no leakage between content types
  origins =
      autoblocker()->GetEmbargoedOrigins(ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(1UL, origins.count(url1));
  origins =
      autoblocker()->GetEmbargoedOrigins(ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(1UL, origins.count(url2));

  // Remove an embargo and confirm it's removed from origins
  autoblocker()->RemoveEmbargoAndResetCounts(url1,
                                             ContentSettingsType::GEOLOCATION);
  origins = autoblocker()->GetEmbargoedOrigins(content_types);
  EXPECT_EQ(1UL, origins.size());
  EXPECT_EQ(1UL, origins.count(url2));

  // Expire the remaining embargo and confirm the origin is removed
  clock()->Advance(base::TimeDelta::FromDays(8));
  origins = autoblocker()->GetEmbargoedOrigins(content_types);
  EXPECT_EQ(0UL, origins.size());
}

// Check that GetEmbargoResult returns the correct value when the embargo is set
// and expires.
TEST_F(PermissionDecisionAutoBlockerUnitTest, CheckEmbargoStatus) {
  GURL url("https://www.google.com");
  clock()->SetNow(base::Time::Now());

  // Check the default state.
  PermissionResult result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Place under embargo and verify.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION, false));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION, false));
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION, false));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);

  // Check that the origin is not under embargo for a different permission.
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Confirm embargo status during the embargo period.
  clock()->Advance(base::TimeDelta::FromDays(5));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);

  // Check embargo is lifted on expiry day. A small offset after the exact
  // embargo expiration date has been added to account for any precision errors
  // when removing the date stored as a double from the permission dictionary.
  clock()->Advance(base::TimeDelta::FromHours(3 * 24 + 1));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Check embargo is lifted well after the expiry day.
  clock()->Advance(base::TimeDelta::FromDays(1));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Place under embargo again and verify the embargo status.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::NOTIFICATIONS, false));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::NOTIFICATIONS, false));
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::NOTIFICATIONS, false));
  clock()->Advance(base::TimeDelta::FromDays(1));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);
}

// Check that GetEmbargoStartTime returns the correct time for embargoes whether
// they are nonexistent, expired or active.
TEST_F(PermissionDecisionAutoBlockerUnitTest, CheckEmbargoStartTime) {
  GURL url("https://www.google.com");

  // The time recorded for embargoes will be stored as a double, which will
  // cause aliasing to a limited set of base::Time values upon retrieval. We
  // thus pick a base::Time for our test time that is part of this set via
  // aliasing the current time by passing it through a double. This allows us
  // to directly compare the test time and times retrieved from storage.
  base::Time test_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(static_cast<double>(
          base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds())));
  clock()->SetNow(test_time);

  // Check the default non embargod state.
  base::Time embargo_start_time =
      autoblocker()->GetEmbargoStartTime(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(base::Time(), embargo_start_time);

  // Ensure that dismissing less than the required number for an embargo
  // does not record an embargo start time.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION, false));
  embargo_start_time =
      autoblocker()->GetEmbargoStartTime(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(base::Time(), embargo_start_time);

  // Place site under geolocation dismissal embargo.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION, false));
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION, false));

  // Confirm embargo is recorded as starting at the correct time.
  embargo_start_time =
      autoblocker()->GetEmbargoStartTime(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(test_time, embargo_start_time);

  // Ensure moving clock while within embargo period does not affect embargo
  // start time.
  clock()->Advance(base::TimeDelta::FromDays(5));
  embargo_start_time =
      autoblocker()->GetEmbargoStartTime(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(test_time, embargo_start_time);

  // Move clock beyond embaro (plus a small offset for potential precision
  // errors) and confirm start time is unaffected but embargo is suspended.
  clock()->Advance(base::TimeDelta::FromHours(3 * 24 + 1));
  embargo_start_time =
      autoblocker()->GetEmbargoStartTime(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(test_time, embargo_start_time);
  PermissionResult result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Advance time, reinstate embargo and confirm that time is updated.
  test_time += base::TimeDelta::FromDays(9);
  test_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(static_cast<double>(
          test_time.ToDeltaSinceWindowsEpoch().InMicroseconds())));
  clock()->SetNow(test_time);

  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION, false));
  embargo_start_time =
      autoblocker()->GetEmbargoStartTime(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(test_time, embargo_start_time);

  // Advance time to expire dismiss embargo and create new embargo for ignoring.
  test_time += base::TimeDelta::FromDays(7);
  test_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(static_cast<double>(
          test_time.ToDeltaSinceWindowsEpoch().InMicroseconds())));
  clock()->SetNow(test_time);

  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url, ContentSettingsType::GEOLOCATION, false));
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url, ContentSettingsType::GEOLOCATION, false));
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url, ContentSettingsType::GEOLOCATION, false));
  EXPECT_TRUE(autoblocker()->RecordIgnoreAndEmbargo(
      url, ContentSettingsType::GEOLOCATION, false));

  // Confirm the most recent embargo is updated to match new ignore embargo.
  embargo_start_time =
      autoblocker()->GetEmbargoStartTime(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(test_time, embargo_start_time);

  // Advance time, reinstate the dismiss embargo via a single action, and
  // confirm that time is updated.
  test_time += base::TimeDelta::FromDays(1);
  test_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(static_cast<double>(
          test_time.ToDeltaSinceWindowsEpoch().InMicroseconds())));
  clock()->SetNow(test_time);

  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION, false));
  embargo_start_time =
      autoblocker()->GetEmbargoStartTime(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(test_time, embargo_start_time);

  // Remove records of dismiss and ignore embargoes and confirm start time
  // reverts to default.
  autoblocker()->RemoveEmbargoAndResetCounts(base::Bind(&FilterGoogle));
  embargo_start_time =
      autoblocker()->GetEmbargoStartTime(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(base::Time(), embargo_start_time);
}

// Tests the alternating pattern of the block on multiple dismiss behaviour. On
// N dismissals, the origin to be embargoed for the requested permission and
// automatically blocked. Each time the embargo is lifted, the site gets another
// chance to request the permission, but if it is again dismissed it is placed
// under embargo again and its permission requests blocked.
TEST_F(PermissionDecisionAutoBlockerUnitTest, TestDismissEmbargoBackoff) {
  GURL url("https://www.google.com");
  clock()->SetNow(base::Time::Now());
  base::HistogramTester histograms;

  // Record some dismisses.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION, false));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION, false));

  // A request with < 3 prior dismisses should not be automatically blocked.
  PermissionResult result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // After the 3rd dismiss subsequent permission requests should be autoblocked.
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION, false));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);

  // Accelerate time forward, check that the embargo status is lifted and the
  // request won't be automatically blocked.
  clock()->Advance(base::TimeDelta::FromDays(8));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Record another dismiss, subsequent requests should be autoblocked again.
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION, false));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);

  // Accelerate time again, check embargo is lifted and another permission
  // request is let through.
  clock()->Advance(base::TimeDelta::FromDays(8));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Record another dismiss, subsequent requests should be autoblocked again.
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::GEOLOCATION, false));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
}

// Tests the alternating pattern of the block on multiple ignores behaviour.
TEST_F(PermissionDecisionAutoBlockerUnitTest, TestIgnoreEmbargoBackoff) {
  GURL url("https://www.google.com");
  clock()->SetNow(base::Time::Now());
  base::HistogramTester histograms;

  // Record some ignores.
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url, ContentSettingsType::MIDI_SYSEX, false));
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url, ContentSettingsType::MIDI_SYSEX, false));

  // A request with < 4 prior ignores should not be automatically blocked.
  PermissionResult result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::MIDI_SYSEX);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // After the 4th ignore subsequent permission requests should be autoblocked.
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url, ContentSettingsType::MIDI_SYSEX, false));
  EXPECT_TRUE(autoblocker()->RecordIgnoreAndEmbargo(
      url, ContentSettingsType::MIDI_SYSEX, false));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::MIDI_SYSEX);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_IGNORES, result.source);

  // Accelerate time forward, check that the embargo status is lifted and the
  // request won't be automatically blocked.
  clock()->Advance(base::TimeDelta::FromDays(8));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::MIDI_SYSEX);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Record another dismiss, subsequent requests should be autoblocked again.
  EXPECT_TRUE(autoblocker()->RecordIgnoreAndEmbargo(
      url, ContentSettingsType::MIDI_SYSEX, false));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::MIDI_SYSEX);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_IGNORES, result.source);

  // Accelerate time again, check embargo is lifted and another permission
  // request is let through.
  clock()->Advance(base::TimeDelta::FromDays(8));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::MIDI_SYSEX);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Record another dismiss, subsequent requests should be autoblocked again.
  EXPECT_TRUE(autoblocker()->RecordIgnoreAndEmbargo(
      url, ContentSettingsType::MIDI_SYSEX, false));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::MIDI_SYSEX);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_IGNORES, result.source);
}

// Test that quiet ui embargo has a different threshold for ignores.
TEST_F(PermissionDecisionAutoBlockerUnitTest, TestIgnoreEmbargoUsingQuietUi) {
  GURL url("https://www.google.com");
  clock()->SetNow(base::Time::Now());

  // Check the default state.
  PermissionResult result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // One quiet ui ignore is not enough to trigger embargo.
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url, ContentSettingsType::NOTIFICATIONS, true));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Loud ui ignores are counted separately.
  EXPECT_FALSE(autoblocker()->RecordIgnoreAndEmbargo(
      url, ContentSettingsType::NOTIFICATIONS, false));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // The second quiet ui ignore puts the url under embargo.
  EXPECT_TRUE(autoblocker()->RecordIgnoreAndEmbargo(
      url, ContentSettingsType::NOTIFICATIONS, true));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_IGNORES, result.source);
}

// Test that quiet ui embargo has a different threshold for dismisses.
TEST_F(PermissionDecisionAutoBlockerUnitTest, TestDismissEmbargoUsingQuietUi) {
  GURL url("https://www.google.com");
  clock()->SetNow(base::Time::Now());

  // Check the default state.
  PermissionResult result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // One loud ui dismiss does not trigger embargo.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::NOTIFICATIONS, false));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // One quiet ui dismiss puts the url under embargo.
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, ContentSettingsType::NOTIFICATIONS, true));
  result =
      autoblocker()->GetEmbargoResult(url, ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
}

}  // namespace permissions
