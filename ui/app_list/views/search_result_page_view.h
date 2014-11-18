// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_VIEW_H_
#define UI_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_VIEW_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/app_list_model.h"
#include "ui/views/view.h"

namespace app_list {

class AppListMainView;
class AppListViewDelegate;
class SearchResultContainerView;

// The start page for the experimental app list.
class APP_LIST_EXPORT SearchResultPageView : public views::View {
 public:
  SearchResultPageView(AppListMainView* app_list_main_view,
                       AppListViewDelegate* view_delegate);
  ~SearchResultPageView() override;

  // Overridden from views::View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void ChildPreferredSizeChanged(views::View* child) override;

 private:
  void AddSearchResultContainerView(
      AppListModel::SearchResults* result_model,
      SearchResultContainerView* result_container);

  // The SearchResultContainerViews that compose the search page. All owned by
  // the views hierarchy.
  std::vector<SearchResultContainerView*> result_container_views_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultPageView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_VIEW_H_
