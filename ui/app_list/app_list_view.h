// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_VIEW_H_
#define UI_APP_LIST_APP_LIST_VIEW_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/search_box_view_delegate.h"
#include "ui/app_list/search_result_list_view_delegate.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"

namespace views {
class View;
}

namespace app_list {

class AppListBubbleBorder;
class AppListModel;
class AppsGridView;
class AppListViewDelegate;
class PageSwitcher;
class PaginationModel;
class SearchBoxView;
class SearchResultListView;

// AppListView is the top-level view and controller of app list UI. It creates
// and hosts a AppsGridView and passes AppListModel to it for display.
class APP_LIST_EXPORT AppListView : public views::BubbleDelegateView,
                                    public views::ButtonListener,
                                    public SearchBoxViewDelegate,
                                    public SearchResultListViewDelegate {
 public:
  // Takes ownership of |delegate|.
  explicit AppListView(AppListViewDelegate* delegate);
  virtual ~AppListView();

  // Initializes the widget.
  void InitAsFullscreenWidget(gfx::NativeView parent,
                              const gfx::Rect& screen_bounds,
                              const gfx::Rect& work_area);
  void InitAsBubble(gfx::NativeView parent,
                    views::View* anchor,
                    views::BubbleBorder::ArrowLocation arrow_location);

  void SetBubbleArrowLocation(
      views::BubbleBorder::ArrowLocation arrow_location);

  void AnimateShow(int duration_ms);
  void AnimateHide(int duration_ms);

  void Close();

  void UpdateBounds(const gfx::Rect& screen_bounds,
                    const gfx::Rect& work_area);

  bool bubble_style() const { return bubble_style_; }
  SearchBoxView* search_box() const { return search_box_view_; }

 private:
  // Creates models to use.
  void CreateModel();

  // Overridden from views::WidgetDelegateView:
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual bool OnKeyPressed(const views::KeyEvent& event) OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // Overridden from views::BubbleDelegate:
  virtual gfx::Rect GetBubbleBounds() OVERRIDE;

  // Overridden from SearchBoxViewDelegate:
  virtual void QueryChanged(SearchBoxView* sender) OVERRIDE;

  // Overridden from SearchResultListViewDelegate:
  virtual void OpenResult(const SearchResult& result,
                          int event_flags) OVERRIDE;

  scoped_ptr<AppListModel> model_;
  scoped_ptr<AppListViewDelegate> delegate_;

  // PaginationModel for model view and page switcher.
  scoped_ptr<PaginationModel> pagination_model_;

  bool bubble_style_;
  AppListBubbleBorder* bubble_border_;  // Owned by views hierarchy.

  AppsGridView* apps_view_;  // Owned by views hierarchy.
  PageSwitcher* page_switcher_view_;  // Owned by views hierarchy.
  SearchBoxView* search_box_view_;  // Owned by views hierarchy.
  SearchResultListView* search_results_view_;  // Owned by views hierarchy.

  // Work area in screen coordinates to layout app list. This is used for
  // full screen mode.
  gfx::Rect work_area_;

  DISALLOW_COPY_AND_ASSIGN(AppListView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_VIEW_H_
