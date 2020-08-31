// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_HIGHLIGHT_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_HIGHLIGHT_H_

#include "components/tab_groups/tab_group_id.h"
#include "ui/views/view.h"

class TabGroupViews;

// View for tab group highlights in the tab strip, which indicate that a group
// is in a selected state. There is one highlight for each group, which is
// positioned across all tabs in the group and painted by the tab strip.
class TabGroupHighlight : public views::View {
 public:
  TabGroupHighlight(TabGroupViews* tab_group_views,
                    const tab_groups::TabGroupId& group);

  const tab_groups::TabGroupId& group() const { return group_; }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  bool CanProcessEventsWithinSubtree() const override;

 private:
  // Returns the highlight shape, which immitates the tab highlight shape.
  SkPath GetPath() const;

  TabGroupViews* const tab_group_views_;
  const tab_groups::TabGroupId group_;

  DISALLOW_COPY_AND_ASSIGN(TabGroupHighlight);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_HIGHLIGHT_H_
