// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_STATS_RECORDER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_STATS_RECORDER_H_

#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

namespace content {
class WebContents;
}

// TabStripModelStatsRecorder records user tab interaction stats.
// In particular, we record tab's lifetime and state transition probability to
// study user interaction with background tabs. (crbug.com/517335)
class TabStripModelStatsRecorder : public TabStripModelObserver {
 public:
  // TabState represents a lifecycle of a tab in TabStripModel.
  // This should match {Current,Next}TabState defined in
  // tools/metrics/histograms/histograms.xml, and
  // constants in Chrome for Android implementation
  // chrome/android/java/src/org/chromium/chrome/browser/tab/TabUma.java
  enum class TabState {
    // Initial tab state.
    INITIAL = 0,

    // For active tabs visible in one of the browser windows.
    ACTIVE = 1,

    // For inactive tabs which are present in the tab strip, but their contents
    // are not visible.
    INACTIVE = 2,

    // Skip 3 to match Chrome for Android implementation.

    // For tabs that are about to be closed.
    CLOSED = 4,

    MAX,
  };

  TabStripModelStatsRecorder();
  ~TabStripModelStatsRecorder() override;

 private:
  // TabStripModelObserver implementation.
  void TabClosingAt(TabStripModel* tab_strip_model,
                    content::WebContents* contents,
                    int index) override;
  void ActiveTabChanged(content::WebContents* old_contents,
                        content::WebContents* new_contents,
                        int index,
                        int reason) override;
  void TabReplacedAt(TabStripModel* tab_strip_model,
                     content::WebContents* old_contents,
                     content::WebContents* new_contents,
                     int index) override;

  class TabInfo;

  std::vector<content::WebContents*> active_tab_history_;

  BrowserTabStripTracker browser_tab_strip_tracker_;

  // Record a single create and close timestamp to track the time between tab
  // creation. (Tabs actually are not opened in a strict sequence so these
  // timestamps are not accurate, but they'll suffice for an estimate.)
  base::TimeTicks last_creation_time_;
  base::TimeTicks last_close_time_;

  DISALLOW_COPY_AND_ASSIGN(TabStripModelStatsRecorder);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_STATS_RECORDER_H_
