// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_stats_data_store.h"

#include "chrome/browser/metrics/tab_stats_tracker.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

using TabsStats = TabStatsDataStore::TabsStats;

class TabStatsDataStoreTest : public ChromeRenderViewHostTestHarness {
 protected:
  TabStatsDataStoreTest() {
    TabStatsTracker::RegisterPrefs(pref_service_.registry());
    data_store_ = std::make_unique<TabStatsDataStore>(&pref_service_);
  }

  std::unique_ptr<content::WebContents> CreateTestWebContents() {
    std::unique_ptr<content::WebContents> contents =
        ChromeRenderViewHostTestHarness::CreateTestWebContents();
    RecentlyAudibleHelper::CreateForWebContents(contents.get());
    return contents;
  }

  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<TabStatsDataStore> data_store_;
};

}  // namespace

TEST_F(TabStatsDataStoreTest, TabStatsGetsReloadedFromLocalState) {
  // This tests creates add some tabs/windows to a data store instance and then
  // reinitializes it (and so the count of active tabs/windows drops to zero).
  // As the TabStatsTracker constructor restores its state from the pref service
  // the maximums should be restored.
  size_t expected_tab_count = 12;
  std::vector<std::unique_ptr<content::WebContents>> test_web_contents_vec;
  for (size_t i = 0; i < expected_tab_count; ++i) {
    test_web_contents_vec.emplace_back(CreateTestWebContents());
    data_store_->OnTabAdded(test_web_contents_vec.back().get());
  }
  size_t expected_window_count = 5;
  for (size_t i = 0; i < expected_window_count; ++i)
    data_store_->OnWindowAdded();
  size_t expected_max_tab_per_window = expected_tab_count - 1;
  data_store_->UpdateMaxTabsPerWindowIfNeeded(expected_max_tab_per_window);

  TabsStats stats = data_store_->tab_stats();
  EXPECT_EQ(expected_tab_count, stats.total_tab_count_max);
  EXPECT_EQ(expected_max_tab_per_window, stats.max_tab_per_window);
  EXPECT_EQ(expected_window_count, stats.window_count_max);

  // Reset the |tab_stats_tracker_| and ensure that the maximums are restored.
  data_store_.reset(new TabStatsDataStore(&pref_service_));

  TabsStats stats2 = data_store_->tab_stats();
  EXPECT_EQ(stats.total_tab_count_max, stats2.total_tab_count_max);
  EXPECT_EQ(stats.max_tab_per_window, stats2.max_tab_per_window);
  EXPECT_EQ(stats.window_count_max, stats2.window_count_max);
  // The actual number of tabs/window should be 0 as it's not stored in the
  // pref service.
  EXPECT_EQ(0U, stats2.window_count);
  EXPECT_EQ(0U, stats2.total_tab_count);
}

TEST_F(TabStatsDataStoreTest, TrackTabUsageDuringInterval) {
  std::vector<std::unique_ptr<content::WebContents>> web_contents_vec;
  for (size_t i = 0; i < 3; ++i) {
    web_contents_vec.emplace_back(CreateTestWebContents());
    // Make sure that these WebContents are initially not visible.
    web_contents_vec.back()->WasHidden();
  }

  // Creates a test interval.
  TabStatsDataStore::TabsStateDuringIntervalMap* interval_map =
      data_store_->AddInterval();
  EXPECT_TRUE(interval_map->empty());

  std::vector<TabStatsDataStore::TabID> web_contents_id_vec;
  // Adds the tabs to the data store.
  for (auto& iter : web_contents_vec) {
    data_store_->OnTabAdded(iter.get());
    web_contents_id_vec.push_back(
        data_store_->GetTabIDForTesting(iter.get()).value());
  }

  EXPECT_EQ(web_contents_vec.size(), interval_map->size());

  for (const auto& iter : web_contents_id_vec) {
    auto map_iter = interval_map->find(iter);
    EXPECT_TRUE(map_iter != interval_map->end());

    // The tabs have been inserted after the creation of the interval.
    EXPECT_FALSE(map_iter->second.existed_before_interval);
    EXPECT_FALSE(map_iter->second.visible_or_audible_during_interval);
    EXPECT_FALSE(map_iter->second.interacted_during_interval);
    EXPECT_TRUE(map_iter->second.exists_currently);
  }

  // Interact with a tab.
  data_store_->OnTabInteraction(web_contents_vec[1].get());
  EXPECT_FALSE(interval_map->find(web_contents_id_vec[0])
                   ->second.interacted_during_interval);
  EXPECT_TRUE(interval_map->find(web_contents_id_vec[1])
                  ->second.interacted_during_interval);
  EXPECT_FALSE(interval_map->find(web_contents_id_vec[2])
                   ->second.interacted_during_interval);

  data_store_->ResetIntervalData(interval_map);
  EXPECT_FALSE(interval_map->find(web_contents_id_vec[0])
                   ->second.interacted_during_interval);
  EXPECT_FALSE(interval_map->find(web_contents_id_vec[1])
                   ->second.interacted_during_interval);
  EXPECT_FALSE(interval_map->find(web_contents_id_vec[2])
                   ->second.interacted_during_interval);

  // Make the first WebContents visible.
  web_contents_vec[0].get()->WasShown();
  data_store_->OnTabVisible(web_contents_vec[0].get());
  EXPECT_TRUE(interval_map->find(web_contents_id_vec[0])
                  ->second.visible_or_audible_during_interval);
  EXPECT_FALSE(interval_map->find(web_contents_id_vec[1])
                   ->second.visible_or_audible_during_interval);
  EXPECT_FALSE(interval_map->find(web_contents_id_vec[2])
                   ->second.visible_or_audible_during_interval);

  // Make one of the WebContents audible.
  auto* audible_helper_2 =
      RecentlyAudibleHelper::FromWebContents(web_contents_vec[2].get());
  audible_helper_2->SetRecentlyAudibleForTesting();
  data_store_->ResetIntervalData(interval_map);
  data_store_->OnTabAudible(web_contents_vec[2].get());
  EXPECT_TRUE(interval_map->find(web_contents_id_vec[0])
                  ->second.visible_or_audible_during_interval);
  EXPECT_FALSE(interval_map->find(web_contents_id_vec[1])
                   ->second.visible_or_audible_during_interval);
  EXPECT_TRUE(interval_map->find(web_contents_id_vec[2])
                  ->second.visible_or_audible_during_interval);

  // Make sure that the tab stats get properly copied when a tab is replaced.
  TabStatsDataStore::TabStateDuringInterval tab_stats_copy =
      interval_map->find(web_contents_id_vec[1])->second;
  std::unique_ptr<content::WebContents> new_contents = CreateTestWebContents();
  content::WebContents* old_contents = web_contents_vec[1].get();
  data_store_->OnTabReplaced(old_contents, new_contents.get());
  EXPECT_EQ(data_store_->GetTabIDForTesting(new_contents.get()).value(),
            web_contents_id_vec[1]);
  web_contents_vec[1] = std::move(new_contents);
  // |old_contents| is invalid starting from here.
  EXPECT_FALSE(data_store_->GetTabIDForTesting(old_contents));
  auto iter = interval_map->find(web_contents_id_vec[1]);
  EXPECT_TRUE(iter != interval_map->end());
  EXPECT_EQ(tab_stats_copy.existed_before_interval,
            iter->second.existed_before_interval);
  EXPECT_EQ(tab_stats_copy.exists_currently, iter->second.exists_currently);
  EXPECT_EQ(tab_stats_copy.visible_or_audible_during_interval,
            iter->second.visible_or_audible_during_interval);
  EXPECT_EQ(tab_stats_copy.interacted_during_interval,
            iter->second.interacted_during_interval);
}

TEST_F(TabStatsDataStoreTest, OnTabReplaced) {
  // Creates a tab and make sure that it's not visible.
  std::unique_ptr<content::WebContents> web_contents_1(CreateTestWebContents());
  web_contents_1->WasHidden();
  // Creates a test interval.
  TabStatsDataStore::TabsStateDuringIntervalMap* interval_map =
      data_store_->AddInterval();

  data_store_->OnTabAdded(web_contents_1.get());
  TabStatsDataStore::TabID tab_id =
      data_store_->GetTabIDForTesting(web_contents_1.get()).value();

  // Interact with the tab.
  data_store_->OnTabInteraction(web_contents_1.get());
  EXPECT_TRUE(interval_map->find(tab_id)->second.interacted_during_interval);
  EXPECT_FALSE(
      interval_map->find(tab_id)->second.visible_or_audible_during_interval);

  std::unique_ptr<content::WebContents> web_contents_2(CreateTestWebContents());
  web_contents_2->WasHidden();

  // Replace the tab, make sure that the |visible_or_audible_during_interval|
  // bit is still set.
  data_store_->OnTabReplaced(web_contents_1.get(), web_contents_2.get());
  EXPECT_FALSE(data_store_->GetTabIDForTesting(web_contents_1.get()));
  EXPECT_EQ(tab_id, data_store_->GetTabIDForTesting(web_contents_2.get()));
  EXPECT_EQ(1U, interval_map->size());
  EXPECT_TRUE(interval_map->find(tab_id)->second.interacted_during_interval);

  // Mark the tab as audible and verify that the corresponding bit is set.
  auto* audible_helper_2 =
      RecentlyAudibleHelper::FromWebContents(web_contents_2.get());
  audible_helper_2->SetNotRecentlyAudibleForTesting();
  data_store_->OnTabAudible(web_contents_2.get());
  EXPECT_TRUE(
      interval_map->find(tab_id)->second.visible_or_audible_during_interval);

  // Close the tab and make sure that the |exists_currently| bit gets cleared.
  data_store_->OnTabRemoved(web_contents_2.get());
  EXPECT_FALSE(interval_map->find(tab_id)->second.exists_currently);

  // Add a new tab with a WebContents pointer that has already been used in the
  // past, make sure that this gets treated as a new tab.
  data_store_->OnTabAdded(web_contents_1.get());
  EXPECT_EQ(2U, interval_map->size());
  EXPECT_NE(tab_id, data_store_->GetTabIDForTesting(web_contents_1.get()));
}

}  // namespace metrics
