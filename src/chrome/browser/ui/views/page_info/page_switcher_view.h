// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_SWITCHER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_SWITCHER_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"

// A container view that contains one view at a time and can switch between
// views with animation.
// TODO(crbug.com/1188101): Implement animation when switching.
class PageSwitcherView : public views::View {
 public:
  explicit PageSwitcherView(std::unique_ptr<views::View> initial_page);
  PageSwitcherView(const PageSwitcherView&) = delete;
  PageSwitcherView& operator=(const PageSwitcherView&) = delete;
  ~PageSwitcherView() override;

  void SwitchToPage(std::unique_ptr<views::View> page);

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;

 private:
  raw_ptr<views::View> current_page_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_SWITCHER_VIEW_H_
