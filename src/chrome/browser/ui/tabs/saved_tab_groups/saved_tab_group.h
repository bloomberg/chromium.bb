// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_H_

#include <string>
#include <vector>

#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "url/gurl.h"

// Preserves the state of a Tab group that was saved from the
// tab_group_editor_bubble_views save toggle button. Additionally, these values
// may change if the tab groups name, color, or urls are changed from the
// tab_group_editor_bubble_view.
struct SavedTabGroup {
  SavedTabGroup(const tab_groups::TabGroupId& group_id,
                const std::u16string& title,
                const tab_groups::TabGroupColorId& color,
                const std::vector<GURL>& urls);
  SavedTabGroup(const SavedTabGroup& other);
  ~SavedTabGroup();

  // The ID associated with this saved tab group. Will also be used when
  // creating a tab group in the tabstrip.
  tab_groups::TabGroupId group_id;
  // The title of the saved tab group.
  std::u16string title;
  // The color of the saved tab group.
  tab_groups::TabGroupColorId color;
  // The URLS and later webcontents (such as favicons) of the saved tab group.
  std::vector<GURL> urls;
};

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_H_
