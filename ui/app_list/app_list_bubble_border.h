// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_BUBBLE_BORDER_H_
#define UI_APP_LIST_APP_LIST_BUBBLE_BORDER_H_

#include "base/basictypes.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/bubble/bubble_border_2.h"

namespace app_list {

// A class to paint bubble border and background.
class AppListBubbleBorder : public views::BubbleBorder2 {
 public:
  AppListBubbleBorder(views::View* app_list_view,
                      views::View* search_box_view);
  virtual ~AppListBubbleBorder();

 private:
  // views::ImagelessBubbleBorder overrides:
  void PaintBackground(gfx::Canvas* canvas,
                       const gfx::Rect& bounds) const;

  // AppListView hosted inside this bubble.
  const views::View* app_list_view_;  // Owned by views hierarchy.

  // Children view of AppListView that needs to paint background.
  const views::View* search_box_view_;  // Owned by views hierarchy.

  DISALLOW_COPY_AND_ASSIGN(AppListBubbleBorder);
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_BUBBLE_BORDER_H_
