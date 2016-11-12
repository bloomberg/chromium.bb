// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_VIEW_H_
#define UI_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_VIEW_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/views/app_list_page.h"
#include "ui/app_list/views/search_result_container_view.h"

namespace app_list {

// The search results page for the app list.
class APP_LIST_EXPORT SearchResultPageView
    : public AppListPage,
      public SearchResultContainerView::Delegate {
 public:
  SearchResultPageView();
  ~SearchResultPageView() override;

  int selected_index() { return selected_index_; }
  bool HasSelection() { return selected_index_ > -1; }
  void SetSelection(bool select);  // Set or unset result selection.

  void AddSearchResultContainerView(
      AppListModel::SearchResults* result_model,
      SearchResultContainerView* result_container);

  const std::vector<SearchResultContainerView*>& result_container_views() {
    return result_container_views_;
  }

  // Overridden from views::View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // AppListPage overrides:
  gfx::Rect GetPageBoundsForState(AppListModel::State state) const override;
  void OnAnimationUpdated(double progress,
                          AppListModel::State from_state,
                          AppListModel::State to_state) override;
  int GetSearchBoxZHeight() const override;
  void OnHidden() override;

  void ClearSelectedIndex();

  // Overridden from SearchResultContainerView::Delegate :
  void OnSearchResultContainerResultsChanged() override;

 private:
  // |directional_movement| is true if the navigation was caused by directional
  // controls (eg, arrow keys), as opposed to linear controls (eg, Tab).
  void SetSelectedIndex(int index, bool directional_movement);
  bool IsValidSelectionIndex(int index);

  // The SearchResultContainerViews that compose the search page. All owned by
  // the views hierarchy.
  std::vector<SearchResultContainerView*> result_container_views_;

  // -1 indicates no selection.
  int selected_index_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultPageView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_VIEW_H_
