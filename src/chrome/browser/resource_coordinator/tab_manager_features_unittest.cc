// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_features.h"

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_piece.h"
#include "components/variations/variations_params_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

class TabManagerFeaturesTest : public testing::Test {
 public:
  // Enables the proactive tab discarding feature, and sets up the associated
  // variations parameter values.
  void EnableProactiveTabFreezeAndDiscard() {
    std::set<std::string> features;
    features.insert(features::kProactiveTabFreezeAndDiscard.name);
    variations_manager_.SetVariationParamsWithFeatureAssociations(
        "DummyTrial", params_, features);
  }

  // Enables the site characteristics database feature, and sets up the
  // associated variations parameter values.
  void EnableSiteCharacteristicsDatabase() {
    std::set<std::string> features;
    features.insert(features::kSiteCharacteristicsDatabase.name);
    variations_manager_.SetVariationParamsWithFeatureAssociations(
        "DummyTrial", params_, features);
  }

  // Enables the site characteristics database feature, and sets up the
  // associated variations parameter values.
  void EnableInfiniteSessionRestore() {
    std::set<std::string> features;
    features.insert(features::kInfiniteSessionRestore.name);
    variations_manager_.SetVariationParamsWithFeatureAssociations(
        "DummyTrial", params_, features);
  }

  void SetParam(base::StringPiece key, base::StringPiece value) {
    params_[key.as_string()] = value.as_string();
  }

  void ExpectProactiveTabFreezeAndDiscardParams(
      bool should_proactively_discard,
      bool should_periodically_unfreeze,
      bool should_protect_tabs_sharing_browsing_instance,
      int low_loaded_tab_count,
      int moderate_loaded_tab_count,
      int high_loaded_tab_count,
      int memory_in_gb,
      base::TimeDelta low_occluded_timeout,
      base::TimeDelta moderate_occluded_timeout,
      base::TimeDelta high_occluded_timeout,
      base::TimeDelta freeze_timeout,
      base::TimeDelta unfreeze_timeout,
      base::TimeDelta refreeze_timeout) {
    ProactiveTabFreezeAndDiscardParams params =
        GetProactiveTabFreezeAndDiscardParams(memory_in_gb);

    EXPECT_EQ(should_proactively_discard, params.should_proactively_discard);
    EXPECT_EQ(should_periodically_unfreeze,
              params.should_periodically_unfreeze);
    EXPECT_EQ(should_protect_tabs_sharing_browsing_instance,
              params.should_protect_tabs_sharing_browsing_instance);
    EXPECT_EQ(low_loaded_tab_count, params.low_loaded_tab_count);
    EXPECT_EQ(moderate_loaded_tab_count, params.moderate_loaded_tab_count);

    // Enforce that |moderate_loaded_tab_count| is within [low_loaded_tab_count,
    // high_loaded_tab_count].
    EXPECT_GE(params.moderate_loaded_tab_count, params.low_loaded_tab_count);
    EXPECT_LE(params.moderate_loaded_tab_count, params.high_loaded_tab_count);

    EXPECT_EQ(high_loaded_tab_count, params.high_loaded_tab_count);
    EXPECT_EQ(low_occluded_timeout, params.low_occluded_timeout);
    EXPECT_EQ(moderate_occluded_timeout, params.moderate_occluded_timeout);
    EXPECT_EQ(high_occluded_timeout, params.high_occluded_timeout);

    EXPECT_EQ(freeze_timeout, params.freeze_timeout);
    EXPECT_EQ(unfreeze_timeout, params.unfreeze_timeout);
    EXPECT_EQ(refreeze_timeout, params.refreeze_timeout);
  }

  void ExpectSiteCharacteristicsDatabaseParams(
      base::TimeDelta favicon_update_observation_window,
      base::TimeDelta title_update_observation_window,
      base::TimeDelta audio_usage_observation_window,
      base::TimeDelta notifications_usage_observation_window,
      base::TimeDelta title_or_favicon_change_grace_period,
      base::TimeDelta audio_usage_grace_period) {
    SiteCharacteristicsDatabaseParams params =
        GetSiteCharacteristicsDatabaseParams();

    EXPECT_EQ(favicon_update_observation_window,
              params.favicon_update_observation_window);
    EXPECT_EQ(title_update_observation_window,
              params.title_update_observation_window);
    EXPECT_EQ(audio_usage_observation_window,
              params.audio_usage_observation_window);
    EXPECT_EQ(notifications_usage_observation_window,
              params.notifications_usage_observation_window);
    EXPECT_EQ(title_or_favicon_change_grace_period,
              params.title_or_favicon_change_grace_period);
    EXPECT_EQ(audio_usage_grace_period, params.audio_usage_grace_period);
  }

  void ExpectInfiniteSessionRestoreParams(
      uint32_t min_simultaneous_tab_loads,
      uint32_t max_simultaneous_tab_loads,
      uint32_t cores_per_simultaneous_tab_load,
      uint32_t min_tabs_to_restore,
      uint32_t max_tabs_to_restore,
      uint32_t mb_free_memory_per_tab_to_restore,
      base::TimeDelta max_time_since_last_use_to_restore,
      uint32_t min_site_engagement_to_restore) {
    InfiniteSessionRestoreParams params = GetInfiniteSessionRestoreParams();
    EXPECT_EQ(min_simultaneous_tab_loads, params.min_simultaneous_tab_loads);
    EXPECT_EQ(max_simultaneous_tab_loads, params.max_simultaneous_tab_loads);
    EXPECT_EQ(cores_per_simultaneous_tab_load,
              params.cores_per_simultaneous_tab_load);
    EXPECT_EQ(min_tabs_to_restore, params.min_tabs_to_restore);
    EXPECT_EQ(max_tabs_to_restore, params.max_tabs_to_restore);
    EXPECT_EQ(mb_free_memory_per_tab_to_restore,
              params.mb_free_memory_per_tab_to_restore);
    EXPECT_EQ(max_time_since_last_use_to_restore,
              params.max_time_since_last_use_to_restore);
    EXPECT_EQ(min_site_engagement_to_restore,
              params.min_site_engagement_to_restore);
  }

  void ExpectDefaultProactiveTabFreezeAndDiscardParams() {
    int memory_in_gb = 4;
    ExpectProactiveTabFreezeAndDiscardParams(
        kProactiveTabFreezeAndDiscard_ShouldProactivelyDiscardDefault,
        kProactiveTabFreezeAndDiscard_ShouldPeriodicallyUnfreezeDefault,
        kProactiveTabFreezeAndDiscard_ShouldProtectTabsSharingBrowsingInstanceDefault,
        kProactiveTabFreezeAndDiscard_LowLoadedTabCountDefault,
        kProactiveTabFreezeAndDiscard_ModerateLoadedTabsPerGbRamDefault *
            memory_in_gb,
        kProactiveTabFreezeAndDiscard_HighLoadedTabCountDefault, memory_in_gb,
        kProactiveTabFreezeAndDiscard_LowOccludedTimeoutDefault,
        kProactiveTabFreezeAndDiscard_ModerateOccludedTimeoutDefault,
        kProactiveTabFreezeAndDiscard_HighOccludedTimeoutDefault,
        kProactiveTabFreezeAndDiscard_FreezeTimeoutDefault,
        kProactiveTabFreezeAndDiscard_UnfreezeTimeoutDefault,
        kProactiveTabFreezeAndDiscard_RefreezeTimeoutDefault);
  }

  void ExpectDefaultSiteCharacteristicsDatabaseParams() {
    ExpectSiteCharacteristicsDatabaseParams(
        kSiteCharacteristicsDb_FaviconUpdateObservationWindow_Default,
        kSiteCharacteristicsDb_TitleUpdateObservationWindow_Default,
        kSiteCharacteristicsDb_AudioUsageObservationWindow_Default,
        kSiteCharacteristicsDb_NotificationsUsageObservationWindow_Default,
        kSiteCharacteristicsDb_TitleOrFaviconChangeGracePeriod_Default,
        kSiteCharacteristicsDb_AudioUsageGracePeriod_Default);
  }

  void ExpectDefaultInfiniteSessionRestoreParams() {
    ExpectInfiniteSessionRestoreParams(
        kInfiniteSessionRestore_MinSimultaneousTabLoadsDefault,
        kInfiniteSessionRestore_MaxSimultaneousTabLoadsDefault,
        kInfiniteSessionRestore_CoresPerSimultaneousTabLoadDefault,
        kInfiniteSessionRestore_MinTabsToRestoreDefault,
        kInfiniteSessionRestore_MaxTabsToRestoreDefault,
        kInfiniteSessionRestore_MbFreeMemoryPerTabToRestoreDefault,
        kInfiniteSessionRestore_MaxTimeSinceLastUseToRestoreDefault,
        kInfiniteSessionRestore_MinSiteEngagementToRestoreDefault);
  }

 private:
  std::map<std::string, std::string> params_;
  variations::testing::VariationParamsManager variations_manager_;
};

}  // namespace

TEST_F(TabManagerFeaturesTest,
       GetProactiveTabFreezeAndDiscardParamsDisabledFeatureGoesToDefault) {
  // Do not enable the proactive tab discarding feature.
  ExpectDefaultProactiveTabFreezeAndDiscardParams();
}

TEST_F(TabManagerFeaturesTest,
       GetProactiveTabFreezeAndDiscardParamsNoneGoesToDefault) {
  EnableProactiveTabFreezeAndDiscard();
  ExpectDefaultProactiveTabFreezeAndDiscardParams();
}

TEST_F(TabManagerFeaturesTest,
       GetProactiveTabFreezeAndDiscardParamsInvalidGoesToDefault) {
  SetParam(kProactiveTabFreezeAndDiscard_ShouldProactivelyDiscardParam, "blah");
  SetParam(kProactiveTabFreezeAndDiscard_ShouldPeriodicallyUnfreezeParam,
           "blah");
  SetParam(
      kProactiveTabFreezeAndDiscard_ShouldProtectTabsSharingBrowsingInstanceParam,
      "bleh");
  SetParam(kProactiveTabFreezeAndDiscard_LowLoadedTabCountParam, "ab");
  SetParam(kProactiveTabFreezeAndDiscard_ModerateLoadedTabsPerGbRamParam,
           "27.8");
  SetParam(kProactiveTabFreezeAndDiscard_HighLoadedTabCountParam, "4e8");
  SetParam(kProactiveTabFreezeAndDiscard_LowOccludedTimeoutParam, "---");
  SetParam(kProactiveTabFreezeAndDiscard_ModerateOccludedTimeoutParam, " ");
  SetParam(kProactiveTabFreezeAndDiscard_HighOccludedTimeoutParam, "");
  SetParam(kProactiveTabFreezeAndDiscard_FreezeTimeoutParam, "b");
  SetParam(kProactiveTabFreezeAndDiscard_UnfreezeTimeoutParam, "i");
  SetParam(kProactiveTabFreezeAndDiscard_RefreezeTimeoutParam, "m");
  EnableProactiveTabFreezeAndDiscard();
  ExpectDefaultProactiveTabFreezeAndDiscardParams();
}

TEST_F(TabManagerFeaturesTest, GetProactiveTabFreezeAndDiscardParams) {
  SetParam(kProactiveTabFreezeAndDiscard_ShouldProactivelyDiscardParam, "true");
  SetParam(kProactiveTabFreezeAndDiscard_ShouldPeriodicallyUnfreezeParam,
           "true");
  SetParam(
      kProactiveTabFreezeAndDiscard_ShouldProtectTabsSharingBrowsingInstanceParam,
      "true");
  SetParam(kProactiveTabFreezeAndDiscard_LowLoadedTabCountParam, "7");
  SetParam(kProactiveTabFreezeAndDiscard_ModerateLoadedTabsPerGbRamParam, "4");
  SetParam(kProactiveTabFreezeAndDiscard_HighLoadedTabCountParam, "42");
  // These are expressed in seconds.
  SetParam(kProactiveTabFreezeAndDiscard_LowOccludedTimeoutParam, "60");
  SetParam(kProactiveTabFreezeAndDiscard_ModerateOccludedTimeoutParam, "120");
  SetParam(kProactiveTabFreezeAndDiscard_HighOccludedTimeoutParam, "247");
  SetParam(kProactiveTabFreezeAndDiscard_FreezeTimeoutParam, "10");
  SetParam(kProactiveTabFreezeAndDiscard_UnfreezeTimeoutParam, "20");
  SetParam(kProactiveTabFreezeAndDiscard_RefreezeTimeoutParam, "30");
  EnableProactiveTabFreezeAndDiscard();

  // Should snap |moderate_loaded_tab_count| to |low_loaded_tab_count|, when the
  // amount of physical memory is so low that (|memory_in_gb| *
  // |moderate_tab_count_per_gb_ram|) < |low_loaded_tab_count|).
  int memory_in_gb_low = 1;
  ExpectProactiveTabFreezeAndDiscardParams(
      true, true, true, 7, 7, 42, memory_in_gb_low,
      base::TimeDelta::FromSeconds(60), base::TimeDelta::FromSeconds(120),
      base::TimeDelta::FromSeconds(247), base::TimeDelta::FromSeconds(10),
      base::TimeDelta::FromSeconds(20), base::TimeDelta::FromSeconds(30));

  // Should snap |moderate_loaded_tab_count| to |high_loaded_tab_count|, when
  // the amount of physical memory is so high that (|memory_in_gb| *
  // |moderate_tab_count_per_gb_ram|) > |high_loaded_tab_count|).
  int memory_in_gb_high = 100;
  ExpectProactiveTabFreezeAndDiscardParams(
      true, true, true, 7, 42, 42, memory_in_gb_high,
      base::TimeDelta::FromSeconds(60), base::TimeDelta::FromSeconds(120),
      base::TimeDelta::FromSeconds(247), base::TimeDelta::FromSeconds(10),
      base::TimeDelta::FromSeconds(20), base::TimeDelta::FromSeconds(30));

  // Tests normal case where |memory_in gb| * |moderate_tab_count_per_gb_ram| is
  // in the interval [low_loaded_tab_count, high_loaded_tab_count].
  int memory_in_gb_normal = 4;
  ExpectProactiveTabFreezeAndDiscardParams(
      true, true, true, 7, 16, 42, memory_in_gb_normal,
      base::TimeDelta::FromSeconds(60), base::TimeDelta::FromSeconds(120),
      base::TimeDelta::FromSeconds(247), base::TimeDelta::FromSeconds(10),
      base::TimeDelta::FromSeconds(20), base::TimeDelta::FromSeconds(30));
}

TEST_F(TabManagerFeaturesTest,
       GetSiteCharacteristicsDatabaseParamsDisabledFeatureGoesToDefault) {
  // Do not enable the site characteristics database.
  ExpectDefaultSiteCharacteristicsDatabaseParams();
}

TEST_F(TabManagerFeaturesTest,
       GetSiteCharacteristicsDatabaseParamsParamsNoneGoesToDefault) {
  ExpectDefaultSiteCharacteristicsDatabaseParams();
}

TEST_F(TabManagerFeaturesTest,
       GetSiteCharacteristicsDatabaseParamsInvalidGoesToDefault) {
  SetParam(kSiteCharacteristicsDb_FaviconUpdateObservationWindow, "    ");
  SetParam(kSiteCharacteristicsDb_TitleUpdateObservationWindow, "foo");
  SetParam(kSiteCharacteristicsDb_AudioUsageObservationWindow, ".");
  SetParam(kSiteCharacteristicsDb_NotificationsUsageObservationWindow, "abc");
  SetParam(kSiteCharacteristicsDb_TitleOrFaviconChangeGracePeriod, "bleh");
  SetParam(kSiteCharacteristicsDb_AudioUsageGracePeriod, "!!!");
  EnableSiteCharacteristicsDatabase();
  ExpectDefaultSiteCharacteristicsDatabaseParams();
}

TEST_F(TabManagerFeaturesTest, GetSiteCharacteristicsDatabaseParams) {
  SetParam(kSiteCharacteristicsDb_FaviconUpdateObservationWindow, "3600");
  SetParam(kSiteCharacteristicsDb_TitleUpdateObservationWindow, "36000");
  SetParam(kSiteCharacteristicsDb_AudioUsageObservationWindow, "360000");
  SetParam(kSiteCharacteristicsDb_NotificationsUsageObservationWindow,
           "3600000");
  SetParam(kSiteCharacteristicsDb_TitleOrFaviconChangeGracePeriod, "42");
  SetParam(kSiteCharacteristicsDb_AudioUsageGracePeriod, "43");

  EnableSiteCharacteristicsDatabase();

  ExpectSiteCharacteristicsDatabaseParams(
      base::TimeDelta::FromSeconds(3600), base::TimeDelta::FromSeconds(36000),
      base::TimeDelta::FromSeconds(360000),
      base::TimeDelta::FromSeconds(3600000), base::TimeDelta::FromSeconds(42),
      base::TimeDelta::FromSeconds(43));
}

TEST_F(TabManagerFeaturesTest,
       GetInfiniteSessionRestoreParamsInvalidGoesToDefault) {
  SetParam(kInfiniteSessionRestore_MinSimultaneousTabLoads, "  ");
  SetParam(kInfiniteSessionRestore_MaxSimultaneousTabLoads, "a.b");
  SetParam(kInfiniteSessionRestore_CoresPerSimultaneousTabLoad, "-- ");
  SetParam(kInfiniteSessionRestore_MinTabsToRestore, "hey");
  SetParam(kInfiniteSessionRestore_MaxTabsToRestore, ".");
  SetParam(kInfiniteSessionRestore_MbFreeMemoryPerTabToRestore, "0x0");
  SetParam(kInfiniteSessionRestore_MaxTimeSinceLastUseToRestore, "foo");
  SetParam(kInfiniteSessionRestore_MinSiteEngagementToRestore, "bar");
  EnableInfiniteSessionRestore();
  ExpectDefaultInfiniteSessionRestoreParams();
}

TEST_F(TabManagerFeaturesTest, GetInfiniteSessionRestoreParams) {
  SetParam(kInfiniteSessionRestore_MinSimultaneousTabLoads, "10");
  SetParam(kInfiniteSessionRestore_MaxSimultaneousTabLoads, "20");
  SetParam(kInfiniteSessionRestore_CoresPerSimultaneousTabLoad, "2");
  SetParam(kInfiniteSessionRestore_MinTabsToRestore, "13");
  SetParam(kInfiniteSessionRestore_MaxTabsToRestore, "27");
  SetParam(kInfiniteSessionRestore_MbFreeMemoryPerTabToRestore, "1337");
  SetParam(kInfiniteSessionRestore_MaxTimeSinceLastUseToRestore, "60");
  SetParam(kInfiniteSessionRestore_MinSiteEngagementToRestore, "9");
  EnableInfiniteSessionRestore();
  ExpectInfiniteSessionRestoreParams(10, 20, 2, 13, 27, 1337,
                                     base::TimeDelta::FromMinutes(1), 9);
}

}  // namespace resource_coordinator
