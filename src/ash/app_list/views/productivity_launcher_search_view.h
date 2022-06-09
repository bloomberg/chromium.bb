// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_PRODUCTIVITY_LAUNCHER_SEARCH_VIEW_H_
#define ASH_APP_LIST_VIEWS_PRODUCTIVITY_LAUNCHER_SEARCH_VIEW_H_

#include <memory>
#include <vector>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/views/search_result_container_view.h"
#include "ash/ash_export.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class AppListViewDelegate;
class ResultSelectionController;
class SearchBoxView;
class SearchResultPageDialogController;

// The search results view for productivity launcher. Contains a scrolling list
// of search results. Does not include the search box, which is owned by a
// parent view.
class ASH_EXPORT ProductivityLauncherSearchView
    : public views::View,
      public SearchResultContainerView::Delegate,
      public AppListModelProvider::Observer {
 public:
  METADATA_HEADER(ProductivityLauncherSearchView);

  ProductivityLauncherSearchView(
      AppListViewDelegate* view_delegate,
      SearchResultPageDialogController* dialog_controller,
      SearchBoxView* search_box_view);
  ProductivityLauncherSearchView(const ProductivityLauncherSearchView&) =
      delete;
  ProductivityLauncherSearchView& operator=(
      const ProductivityLauncherSearchView&) = delete;
  ~ProductivityLauncherSearchView() override;

  // SearchResultContainerView::Delegate:
  void OnSearchResultContainerResultsChanging() override;
  void OnSearchResultContainerResultsChanged() override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // AppListModelProvider::Observer:
  void OnActiveAppListModelsChanged(AppListModel* model,
                                    SearchModel* search_model) override;

  // Returns true if there are search results that can be keyboard selected.
  bool CanSelectSearchResults();

  // Sums the heights of all search_result_list_views_ owned by this view.
  int TabletModePreferredHeight();

  std::vector<SearchResultContainerView*> result_container_views_for_test() {
    return result_container_views_;
  }

  ResultSelectionController* result_selection_controller_for_test() {
    return result_selection_controller_.get();
  }

 private:
  // Passed to |result_selection_controller_| as a callback that gets called
  // when the currently selected result changes.
  // Scrolls the list view to the newly selected result.
  void OnSelectedResultChanged();

  // Sets whether changes in search result containers should be hidden from the
  // accessibility framework.
  // This is set while search results are being updated to reduce noisy updates
  // sent to the accessibility framework while the search result containers are
  // being rebuilt.
  // The |ignore| value is reset in NotifyA11yResultsChanged(), at which time
  // accessibility framework is notified that the view value/selected children
  // have changed.
  void SetIgnoreResultChangesForA11y(bool ignore);

  // Schedules a call to |NotifyA11yResultsChanged|. Called from
  // OnSearchResultContainerResultsChanged() when all result containers have
  // finished changing. The goal of the delay is to silence bursts of A11Y
  // events caused by from rapidly changing user queries and consecutive search
  // result updates.
  void ScheduleResultsChangedA11yNotification();

  // Notifies the accessibility framework that the set of search results has
  // changed.
  // Note: This ensures that results changes are not being hidden from a11y
  // framework.
  void NotifyA11yResultsChanged();

  // Send a kSelection a11y notification for the currently selected search
  // result view unless overridden by |ignore_result_changes_for_a11y_|.
  void MaybeNotifySelectedResultChanged();

  SearchResultPageDialogController* const dialog_controller_;

  SearchBoxView* const search_box_view_;

  // The scroll view that contains all the result_container_views_.
  views::ScrollView* scroll_view_ = nullptr;

  // Whether changes in search result containers are hidden from the
  // accessibility framework.
  bool ignore_result_changes_for_a11y_ = false;

  // Containers for search result views. The contained views are owned by the
  // views hierarchy. Used by result_selection_controller_.
  std::vector<SearchResultContainerView*> result_container_views_;

  // Handles search result selection.
  std::unique_ptr<ResultSelectionController> result_selection_controller_;

  // Timer used to delay calls to NotifyA11yResultsChanged().
  base::OneShotTimer notify_a11y_results_changed_timer_;

  // The last reported number of search results shown by all containers.
  int last_search_result_count_ = 0;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_PRODUCTIVITY_LAUNCHER_SEARCH_VIEW_H_
