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
#include "ui/views/view.h"

namespace views {
class BoundsAnimator;
class ViewModel;
}

namespace app_list {

class AppsGridView;
class ApplicationDragAndDropHost;
class AppListFolderItem;
class AppListMainView;
class AppListModel;
class AppListViewDelegate;
class AppsContainerView;
class PaginationModel;
class SearchResultListView;
class StartPageView;

// A view to manage launcher pages within the Launcher (eg. start page, apps
// grid view, search results). There can be any number of launcher pages, only
// one of which can be active at a given time. ContentsView provides the user
// interface for switching between launcher pages, and animates the transition
// between them.
class APP_LIST_EXPORT ContentsView : public views::View {
 public:
  // Values of this enum denote special launcher pages that require hard-coding.
  // Launcher pages are not required to have a NamedPage enum value.
  enum NamedPage {
    NAMED_PAGE_APPS,
    NAMED_PAGE_SEARCH_RESULTS,
    NAMED_PAGE_START,
  };

  ContentsView(AppListMainView* app_list_main_view,
               AppListModel* model,
               AppListViewDelegate* view_delegate);
  virtual ~ContentsView();

  // The app list gets closed and drag and drop operations need to be cancelled.
  void CancelDrag();

  // If |drag_and_drop| is not NULL it will be called upon drag and drop
  // operations outside the application list.
  void SetDragAndDropHostOfCurrentAppList(
      ApplicationDragAndDropHost* drag_and_drop_host);

  void ShowSearchResults(bool show);
  void ShowFolderContent(AppListFolderItem* folder);
  bool IsShowingSearchResults() const;

  // Sets the active launcher page and animates the pages into place.
  void SetActivePage(int page_index);

  // The index of the currently active launcher page.
  int active_page_index() const { return active_page_; }

  // True if |named_page| is the current active laucher page.
  bool IsNamedPageActive(NamedPage named_page) const;

  // Gets the index of a launcher page in |view_model_|, by NamedPage.
  int GetPageIndexForNamedPage(NamedPage named_page) const;

  void Prerender();

  AppsContainerView* apps_container_view() { return apps_container_view_; }
  StartPageView* start_page_view() { return start_page_view_; }
  SearchResultListView* search_results_view() { return search_results_view_; }

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;
  virtual bool OnMouseWheel(const ui::MouseWheelEvent& event) OVERRIDE;

 private:
  // Sets the active launcher page, accounting for whether the change is for
  // search results.
  void SetActivePageInternal(int page_index, bool show_search_results);

  // Invoked when active view is changed.
  void ActivePageChanged(bool show_search_results);

  void CalculateIdealBounds();
  void AnimateToIdealBounds();

  // Adds |view| as a new page to the end of the list of launcher pages. The
  // view is inserted as a child of the ContentsView. There is no name
  // associated with the page. Returns the index of the new page.
  int AddLauncherPage(views::View* view);

  // Adds |view| as a new page to the end of the list of launcher pages. The
  // view is inserted as a child of the ContentsView. The page is associated
  // with the name |named_page|. Returns the index of the new page.
  int AddLauncherPage(views::View* view, NamedPage named_page);

  // Gets the PaginationModel owned by the AppsGridView.
  PaginationModel* GetAppsPaginationModel();

  // Overridden from ui::EventHandler:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;

  // Index into |view_model_| of the currently active page.
  int active_page_;

  // Special sub views of the ContentsView. All owned by the views hierarchy.
  AppsContainerView* apps_container_view_;
  SearchResultListView* search_results_view_;
  StartPageView* start_page_view_;

  AppListMainView* app_list_main_view_;     // Parent view, owns this.

  scoped_ptr<views::ViewModel> view_model_;
  // Maps NamedPage onto |view_model_| indices.
  std::map<NamedPage, int> named_page_to_view_;
  scoped_ptr<views::BoundsAnimator> bounds_animator_;

  DISALLOW_COPY_AND_ASSIGN(ContentsView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_CONTENTS_VIEW_H_
