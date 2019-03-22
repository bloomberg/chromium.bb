// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TASKS_TASK_TAB_HELPER_H_
#define CHROME_BROWSER_ANDROID_TASKS_TASK_TAB_HELPER_H_

#include <map>

#include "base/macros.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace tasks {

class TaskTabHelper : public content::WebContentsObserver,
                      public content::WebContentsUserData<TaskTabHelper> {
 public:
  ~TaskTabHelper() override;

  // WebContentsObserver
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  void NavigationListPruned(
      const content::PrunedDetails& pruned_details) override;

 protected:
  explicit TaskTabHelper(content::WebContents* web_contents);

  enum class HubType { DEFAULT_SEARCH_ENGINE, FORM_SUBMIT, OTHER };

  virtual HubType GetSpokeEntryHubType() const;

  // For testing
  int GetSpokesForTesting(int id) {
    return entry_index_to_spoke_count_map_[id];
  }

 private:
  friend class content::WebContentsUserData<TaskTabHelper>;

  void RecordHubAndSpokeNavigationUsage(int sample);

  int last_pruned_navigation_entry_index_;
  std::map<int, int> entry_index_to_spoke_count_map_;

  DISALLOW_COPY_AND_ASSIGN(TaskTabHelper);
};

}  // namespace tasks

#endif  // CHROME_BROWSER_ANDROID_TASKS_TASK_TAB_HELPER_H_
