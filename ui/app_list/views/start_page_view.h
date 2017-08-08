// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_START_PAGE_VIEW_H_
#define UI_APP_LIST_VIEWS_START_PAGE_VIEW_H_

#include <stddef.h>

#include <vector>

#include "base/macros.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/views/app_list_page.h"

namespace app_list {

class AppListMainView;
class AppListView;
class AppListViewDelegate;
class CustomLauncherPageBackgroundView;
class ExpandArrowView;
class SearchResultTileItemView;
class SuggestionsContainerView;
class TileItemView;

// The start page for the app list.
class APP_LIST_EXPORT StartPageView : public AppListPage {
 public:
  StartPageView(AppListMainView* app_list_main_view,
                AppListViewDelegate* view_delegate,
                AppListView* app_list_view);
  ~StartPageView() override;

  static int kNoSelection;           // No view is selected.
  static int kExpandArrowSelection;  // Expand arrow is selected.

  void Reset();

  void UpdateForTesting();

  views::View* instant_container() const { return instant_container_; }
  const std::vector<SearchResultTileItemView*>& tile_views() const;
  TileItemView* all_apps_button() const;

  // Overridden from AppListPage:
  gfx::Rect GetPageBoundsForState(AppListModel::State state) const override;
  gfx::Rect GetSearchBoxBounds() const override;
  void OnShown() override;

  // Overridden from views::View:
  void Layout() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;

  // Used only in the tests to get the selected index in start page view.
  // Returns |kNoSelection|, |kExpandArrowSelection| or an index >= 0 which is
  // the selected index in suggestions container view.
  int GetSelectedIndexForTest() const;

  // Updates the opacity of the items in start page during dragging.
  void UpdateOpacity(float work_area_bottom, bool is_end_gesture);

 private:
  void InitInstantContainer();

  void MaybeOpenCustomLauncherPage();

  void SetCustomLauncherPageSelected(bool selected);

  // Updates opacity of |view_item| in the start page based on |centroid_y|.
  void UpdateOpacityOfItem(views::View* view_item, float centroid_y);

  TileItemView* GetTileItemView(size_t index);

  // Handles key events in fullscreen app list.
  bool HandleKeyPressedFullscreen(const ui::KeyEvent& event);

  AppListView* app_list_view_;

  // The parent view of ContentsView which is the parent of this view.
  AppListMainView* app_list_main_view_;

  AppListViewDelegate* view_delegate_;  // Owned by AppListView.

  // An invisible placeholder view which fills the space for the search box view
  // in a box layout. The search box view itself is a child of the AppListView
  // (because it is visible on many different pages).
  views::View* search_box_spacer_view_;  // Owned by views hierarchy.

  views::View* instant_container_;  // Owned by views hierarchy.
  CustomLauncherPageBackgroundView*
      custom_launcher_page_background_;     // Owned by views hierarchy.
  SuggestionsContainerView*
      suggestions_container_;  // Owned by views hierarchy.
  ExpandArrowView* expand_arrow_view_ = nullptr;  // Owned by views hierarchy.

  const bool is_fullscreen_app_list_enabled_;

  // The bottom of work area.
  float work_area_bottom_ = 0.f;

  // True if it is the end gesture of dragging.
  bool is_end_gesture_ = false;

  DISALLOW_COPY_AND_ASSIGN(StartPageView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_START_PAGE_VIEW_H_
