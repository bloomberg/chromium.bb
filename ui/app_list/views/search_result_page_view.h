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

  int selected_index() const { return selected_index_; }
  bool HasSelection() const { return selected_index_ > -1; }
  void SetSelection(bool select);  // Set or unset result selection.

  void AddSearchResultContainerView(
      AppListModel::SearchResults* result_model,
      SearchResultContainerView* result_container);

  const std::vector<SearchResultContainerView*>& result_container_views() {
    return result_container_views_;
  }

  // Overridden from views::View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;

  // AppListPage overrides:
  gfx::Rect GetPageBoundsForState(AppListModel::State state) const override;
  void OnAnimationUpdated(double progress,
                          AppListModel::State from_state,
                          AppListModel::State to_state) override;
  void OnHidden() override;
  gfx::Rect GetSearchBoxBounds() const override;
  views::View* GetSelectedView() const override;

  void ClearSelectedIndex();

  // Overridden from SearchResultContainerView::Delegate :
  void OnSearchResultContainerResultsChanged() override;

  views::View* contents_view() { return contents_view_; }

  views::View* first_result_view() const { return first_result_view_; }

 private:
  // Separator between SearchResultContainerView.
  class HorizontalSeparator;

  // |directional_movement| is true if the navigation was caused by directional
  // controls (eg, arrow keys), as opposed to linear controls (eg, Tab).
  void SetSelectedIndex(int index, bool directional_movement);
  bool IsValidSelectionIndex(int index);

  // Sort the result container views.
  void ReorderSearchResultContainers();

  // The SearchResultContainerViews that compose the search page. All owned by
  // the views hierarchy.
  std::vector<SearchResultContainerView*> result_container_views_;

  std::vector<HorizontalSeparator*> separators_;

  // -1 indicates no selection.
  int selected_index_;

  // Whether the app list focus is enabled.
  const bool is_app_list_focus_enabled_;

  // View containing SearchCardView instances. Owned by view hierarchy.
  views::View* const contents_view_;

  // The first search result's view or nullptr if there's no search result.
  views::View* first_result_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SearchResultPageView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_VIEW_H_
