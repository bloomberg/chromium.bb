// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_TAB_STATS_DATA_STORE_H_
#define CHROME_BROWSER_METRICS_TAB_STATS_DATA_STORE_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"

using mojom::LifecycleUnitDiscardReason;

class PrefService;

namespace content {
class WebContents;
}  // namespace content

namespace metrics {

FORWARD_DECLARE_TEST(TabStatsTrackerBrowserTest,
                     TabDeletionGetsHandledProperly);

// The data store that keeps track of all the information that the
// TabStatsTracker wants to track.
class TabStatsDataStore {
 public:
  // Houses all of the statistics gathered by the TabStatsTracker.
  struct TabsStats {
    // Constructor, initializes everything to zero.
    TabsStats();
    TabsStats(const TabsStats& other);

    // The total number of tabs opened across all the windows.
    size_t total_tab_count;

    // The maximum total number of tabs that has been opened across all the
    // windows at the same time.
    size_t total_tab_count_max;

    // The maximum total number of tabs that has been opened at the same time in
    // a single window.
    size_t max_tab_per_window;

    // The total number of windows opened.
    size_t window_count;

    // The maximum total number of windows opened at the same time.
    size_t window_count_max;

    // The number of tabs discarded, per discard reason.
    std::array<size_t,
               static_cast<size_t>(LifecycleUnitDiscardReason::kMaxValue) + 1>
        tab_discard_counts;

    // The number of tabs reloaded after a discard, per discard reason.
    std::array<size_t,
               static_cast<size_t>(LifecycleUnitDiscardReason::kMaxValue) + 1>
        tab_reload_counts;
  };

  // Structure describing the state of a tab during an interval of time.
  struct TabStateDuringInterval {
    // Indicates if the tab exists at the beginning of the interval.
    bool existed_before_interval;
    // Indicates if the tab still exists.
    bool exists_currently;
    // Indicates if the tab has been visible or audible at any moment during the
    // interval.
    bool visible_or_audible_during_interval;
    // Indicates if the tab has been interacted with or active at any moment
    // during the interval. See the |OnTabInteraction| for the list of possible
    // interactions.
    bool interacted_during_interval;
  };

  // A TabID is used instead of a WebContents* because:
  //   - The WebContents of a tab can change during its lifetime.
  //   - A WebContents* can be reused for a separate tab after a tab has been
  //     closed.
  using TabID = size_t;
  // Represents the state of a set of tabs during an interval of time.
  using TabsStateDuringIntervalMap =
      base::flat_map<TabID, TabStateDuringInterval>;

  explicit TabStatsDataStore(PrefService* pref_service);
  virtual ~TabStatsDataStore();

  // Functions used to update the window/tab count.
  void OnWindowAdded();
  void OnWindowRemoved();
  // Virtual for unittesting.
  virtual void OnTabAdded(content::WebContents* web_contents);
  virtual void OnTabRemoved(content::WebContents* web_contents);
  void OnTabReplaced(content::WebContents* old_contents,
                     content::WebContents* new_contents);

  // Update the maximum number of tabs in a single window if |value| exceeds
  // this.
  // TODO(sebmarchand): Store a list of windows in this class and track the
  // number of tabs per window.
  void UpdateMaxTabsPerWindowIfNeeded(size_t value);

  // Reset all the maximum values to the current state, to be used once the
  // metrics have been reported.
  void ResetMaximumsToCurrentState();

  // Records that there's been a direct user interaction with a tab, see the
  // comment for |DidGetUserInteraction| in
  // content/public/browser/web_contents_observer.h for a list of the possible
  // type of interactions.
  void OnTabInteraction(content::WebContents* web_contents);

  // Records that a tab became audible.
  void OnTabAudible(content::WebContents* web_contents);

  // Records that a tab became visible.
  void OnTabVisible(content::WebContents* web_contents);

  // Creates a new interval map. The returned pointer is owned by |this|.
  TabsStateDuringIntervalMap* AddInterval();

  // Reset |interval_map| with the list of current tabs.
  void ResetIntervalData(TabsStateDuringIntervalMap* interval_map);

  // Updates counters when the discarded state of a tab changes.
  void OnTabDiscardStateChange(LifecycleUnitDiscardReason discard_reason,
                               bool is_discarding);

  // Clears the discard and reload counters. Called after reporting the counter
  // values.
  void ClearTabDiscardAndReloadCounts();

  const TabsStats& tab_stats() const { return tab_stats_; }

  base::Optional<TabID> GetTabIDForTesting(content::WebContents* web_contents);

  base::flat_map<content::WebContents*, TabID>* existing_tabs_for_testing() {
    return &existing_tabs_;
  }

 protected:
  FRIEND_TEST_ALL_PREFIXES(TabStatsTrackerBrowserTest,
                           TabDeletionGetsHandledProperly);

  // Update the maximums metrics if needed.
  void UpdateTotalTabCountMaxIfNeeded();
  void UpdateWindowCountMaxIfNeeded();

  // Adds a tab to an interval map.
  void AddTabToIntervalMap(content::WebContents* web_contents,
                           TabID tab_id,
                           bool existed_before_interval,
                           TabsStateDuringIntervalMap* interval_map);

  // Returns the TabID associated with a tab.
  TabID GetTabID(content::WebContents* web_contents);

 private:
  void OnTabAudibleOrVisible(content::WebContents* web_contents);

  // The tabs stats.
  TabsStats tab_stats_;

  // A raw pointer to the PrefService used to read and write the statistics.
  PrefService* pref_service_;

  // The interval maps, one per period of time that we want to observe.
  std::vector<std::unique_ptr<TabsStateDuringIntervalMap>> interval_maps_;

  // The tabs that currently exist.
  base::flat_map<content::WebContents*, TabID> existing_tabs_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(TabStatsDataStore);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_TAB_STATS_DATA_STORE_H_
