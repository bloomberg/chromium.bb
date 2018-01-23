// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_PAGE_SWITCHER_H_
#define UI_APP_LIST_VIEWS_PAGE_SWITCHER_H_

#include "ui/views/view.h"

namespace app_list {

// PageSwitcher represents its underlying PaginationModel with a button strip.
// Each page in the PageinationModel has a button in the strip and when the
// button is clicked, the corresponding page becomes selected.
class PageSwitcher : public views::View {
 public:
  // Returns the page index of the page switcher button under the point. If no
  // page switcher button is under the point, -1 is return. |point| is in
  // PageSwitcher's coordinates.
  virtual int GetPageForPoint(const gfx::Point& point) const = 0;

  // Shows hover for button under the point. |point| is in PageSwitcher's
  // coordinates.
  virtual void UpdateUIForDragPoint(const gfx::Point& point) = 0;

  // Gets the screen bounds of the buttons in the page switcher.
  virtual gfx::Rect GetButtonsBoundsInScreen() = 0;
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_PAGE_SWITCHER_H_
