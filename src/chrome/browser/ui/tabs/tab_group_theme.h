// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_GROUP_THEME_H_
#define CHROME_BROWSER_UI_TABS_TAB_GROUP_THEME_H_

#include "components/tab_groups/tab_group_color.h"

int GetTabGroupTabStripColorId(tab_groups::TabGroupColorId group_color_id,
                               bool active_frame);

int GetTabGroupDialogColorId(tab_groups::TabGroupColorId group_color_id);

int GetTabGroupContextMenuColorId(tab_groups::TabGroupColorId group_color_id);

#endif  // CHROME_BROWSER_UI_TABS_TAB_GROUP_THEME_H_
