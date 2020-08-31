// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_IN_PRODUCT_HELP_ACTIVE_TAB_TRACKER_H_
#define CHROME_BROWSER_UI_IN_PRODUCT_HELP_ACTIVE_TAB_TRACKER_H_

#include <unordered_map>

#include "base/callback.h"
#include "base/time/time.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

namespace base {
class TickClock;
}

class TabStripModel;

// Tracks when tabs become active and notifies client when active tabs are
// closed. The client must register and unregister |TabStripModel|s and set a
// callback to be called when an active tab is closed.
class ActiveTabTracker : public TabStripModelObserver {
 public:
  // Callback to be called when an active tab is closed. The callback takes two
  // arguments: the TabStripModel that changed and how long the tab was active.
  using ActiveTabClosedCallback =
      base::RepeatingCallback<void(TabStripModel*, base::TimeDelta)>;

  ActiveTabTracker(const base::TickClock* clock,
                   ActiveTabClosedCallback callback);
  ~ActiveTabTracker() override;

  // Observes |tab_strip_model|. Its last activation time is set to the current
  // time.
  void AddTabStripModel(TabStripModel* tab_strip_model);

  // Stops observing |tab_strip_model|. The |tab_strip_model| must have been
  // previously added.
  void RemoveTabStripModel(TabStripModel* tab_strip_model);

 private:
  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  const base::TickClock* const clock_;
  const ActiveTabClosedCallback active_tab_closed_callback_;
  // Map containing the latest time the active tab changed for each tab strip
  // model. Also serves as the list of all registered |TabStripModel|s.
  std::unordered_map<TabStripModel*, base::TimeTicks> active_tab_changed_times_;

  DISALLOW_COPY_AND_ASSIGN(ActiveTabTracker);
};

#endif  // CHROME_BROWSER_UI_IN_PRODUCT_HELP_ACTIVE_TAB_TRACKER_H_
