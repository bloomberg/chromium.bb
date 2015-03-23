// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_CUSTOM_LAUNCHER_PAGE_VIEW_H_
#define UI_APP_LIST_VIEWS_CUSTOM_LAUNCHER_PAGE_VIEW_H_

#include "ui/app_list/app_list_export.h"
#include "ui/app_list/views/app_list_page.h"

namespace app_list {

// A wrapper view around the custom launcher page web view.
class APP_LIST_EXPORT CustomLauncherPageView : public AppListPage {
 public:
  explicit CustomLauncherPageView(View* custom_launcher_page_webview);
  ~CustomLauncherPageView() override;

  // Gets the location of the custom launcher page in "collapsed" state. This is
  // where the page is peeking in from the bottom of the launcher (neither full
  // on-screen or off-screen).
  gfx::Rect GetCollapsedLauncherPageBounds() const;

  View* custom_launcher_page_contents() {
    return custom_launcher_page_contents_;
  }

  // AppListPage overrides:
  gfx::Rect GetPageBoundsForState(AppListModel::State state) const override;
  void OnShown() override;
  void OnWillBeHidden() override;

 private:
  View* custom_launcher_page_contents_;

  DISALLOW_COPY_AND_ASSIGN(CustomLauncherPageView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_CUSTOM_LAUNCHER_PAGE_VIEW_H_
