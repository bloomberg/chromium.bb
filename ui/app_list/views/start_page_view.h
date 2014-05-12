// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_START_PAGE_VIEW_H_
#define UI_APP_LIST_VIEWS_START_PAGE_VIEW_H_

#include "base/basictypes.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace content {
class WebContents;
}

namespace app_list {

class AppListMainView;

// The start page for the experimental app list.
class StartPageView : public views::View, public views::ButtonListener {
 public:
  StartPageView(AppListMainView* app_list_main_view,
                content::WebContents* start_page_web_contents);
  virtual ~StartPageView();

  void Reset();

 private:
  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // The parent view of ContentsView which is the parent of this view.
  AppListMainView* app_list_main_view_;

  views::View* instant_container_;  // Owned by views hierarchy.

  DISALLOW_COPY_AND_ASSIGN(StartPageView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_START_PAGE_VIEW_H_
