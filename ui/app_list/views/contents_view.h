// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_CONTENTS_VIEW_H_
#define UI_APP_LIST_VIEWS_CONTENTS_VIEW_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/pagination_model_observer.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace gfx {
class Rect;
}

namespace app_list {

class AppsGridView;
class ApplicationDragAndDropHost;
class AppListFolderItem;
class AppListMainView;
class AppListModel;
class AppListViewDelegate;
class AppsContainerView;
class ContentsSwitcherView;
class PaginationModel;
class SearchResultListView;
class SearchResultPageView;
class StartPageView;

// A view to manage launcher pages within the Launcher (eg. start page, apps
// grid view, search results). There can be any number of launcher pages, only
// one of which can be active at a given time. ContentsView provides the user
// interface for switching between launcher pages, and animates the transition
// between them.
class APP_LIST_EXPORT ContentsView : public views::View,
                                     public PaginationModelObserver {
 public:
  ContentsView(AppListMainView* app_list_main_view);
  ~ContentsView() override;

  // Initialize the pages of the launcher. In the experimental launcher, should
  // be called after set_contents_switcher_view(), or switcher buttons will not
  // be created.
  void Init(AppListModel* model, AppListViewDelegate* view_delegate);

  // The app list gets closed and drag and drop operations need to be cancelled.
  void CancelDrag();

  // If |drag_and_drop| is not NULL it will be called upon drag and drop
  // operations outside the application list.
  void SetDragAndDropHostOfCurrentAppList(
      ApplicationDragAndDropHost* drag_and_drop_host);

  void SetContentsSwitcherView(ContentsSwitcherView* contents_switcher_view);

  // Shows/hides the search results. Hiding the search results will cause the
  // app list to return to the page that was displayed before
  // ShowSearchResults(true) was invoked.
  void ShowSearchResults(bool show);
  bool IsShowingSearchResults() const;

  void ShowFolderContent(AppListFolderItem* folder);

  // Sets the active launcher page and animates the pages into place.
  void SetActivePage(int page_index);

  // The index of the currently active launcher page.
  int GetActivePageIndex() const;

  // True if |state| is the current active laucher page.
  bool IsStateActive(AppListModel::State state) const;

  // Gets the index of a launcher page in |view_model_|, by State. Returns
  // -1 if there is no view for |state|.
  int GetPageIndexForState(AppListModel::State state) const;

  int NumLauncherPages() const;

  void Prerender();

  AppsContainerView* apps_container_view() { return apps_container_view_; }
  StartPageView* start_page_view() { return start_page_view_; }
  SearchResultListView* search_results_list_view() {
    return search_results_list_view_;
  }
  SearchResultPageView* search_results_page_view() {
    return search_results_page_view_;
  }
  views::View* GetPageView(int index);

  // Adds a blank launcher page. For use in tests only.
  void AddBlankPageForTesting();

  // Returns the pagination model for the ContentsView.
  const PaginationModel& pagination_model() { return pagination_model_; }

  // Returns search box bounds to use for content views that do not specify
  // their own custom layout.
  gfx::Rect GetDefaultSearchBoxBounds() const;

  // Returns the content area bounds to use for content views that do not
  // specify their own custom layout.
  gfx::Rect GetDefaultContentsBounds() const;

  // Overridden from views::View:
  gfx::Size GetPreferredSize() const override;
  void Layout() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // Overridden from PaginationModelObserver:
  void TotalPagesChanged() override;
  void SelectedPageChanged(int old_selected, int new_selected) override;
  void TransitionStarted() override;
  void TransitionChanged() override;

 private:
  // Sets the active launcher page, accounting for whether the change is for
  // search results.
  void SetActivePageInternal(int page_index, bool show_search_results);

  // Invoked when active view is changed.
  void ActivePageChanged();

  // Returns the size of the default content area.
  gfx::Size GetDefaultContentsSize() const;

  // Gets the origin (the off-screen resting place) for a given launcher page
  // with index |page_index|.
  gfx::Rect GetOffscreenPageBounds(int page_index) const;

  // Calculates and sets the bounds for the subviews. If there is currently an
  // animation, this positions the views as appropriate for the current frame.
  void UpdatePageBounds();

  // Adds |view| as a new page to the end of the list of launcher pages. The
  // view is inserted as a child of the ContentsView, and a button with
  // |resource_id| is added to the ContentsSwitcherView. There is no name
  // associated with the page. Returns the index of the new page.
  int AddLauncherPage(views::View* view, int resource_id);

  // Adds |view| as a new page to the end of the list of launcher pages. The
  // view is inserted as a child of the ContentsView, and a button with
  // |resource_id| is added to the ContentsSwitcherView. The page is associated
  // with the name |state|. Returns the index of the new page.
  int AddLauncherPage(views::View* view,
                      int resource_id,
                      AppListModel::State state);

  // Gets the PaginationModel owned by the AppsGridView.
  // Note: This is different to |pagination_model_|, which manages top-level
  // launcher-page pagination.
  PaginationModel* GetAppsPaginationModel();

  // Special sub views of the ContentsView. All owned by the views hierarchy.
  AppsContainerView* apps_container_view_;

  // Only used in the normal app list.
  SearchResultListView* search_results_list_view_;

  // Only used in the experimental app list.
  SearchResultPageView* search_results_page_view_;
  StartPageView* start_page_view_;

  AppListMainView* app_list_main_view_;     // Parent view, owns this.
  // Sibling view, owned by |app_list_main_view_|.
  ContentsSwitcherView* contents_switcher_view_;

  scoped_ptr<views::ViewModel> view_model_;

  // Maps State onto |view_model_| indices.
  std::map<AppListModel::State, int> state_to_view_;

  // Maps |view_model_| indices onto State.
  std::map<int, AppListModel::State> view_to_state_;

  // The page that was showing before ShowSearchResults(true) was invoked.
  int page_before_search_;

  // Manages the pagination for the launcher pages.
  PaginationModel pagination_model_;

  DISALLOW_COPY_AND_ASSIGN(ContentsView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_CONTENTS_VIEW_H_
