// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_VIEW_H_
#define UI_APP_LIST_APP_LIST_VIEW_H_

#include "base/memory/scoped_ptr.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/search_box_view_delegate.h"
#include "ui/app_list/search_result_list_view_delegate.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"

namespace app_list {

class AppListBubbleBorder;
class AppListModel;
class AppListViewDelegate;
class ContentsView;
class PaginationModel;
class SearchBoxView;

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
  void InitAsBubble(gfx::NativeView parent,
                    PaginationModel* pagination_model,
                    views::View* anchor,
                    const gfx::Point& anchor_point,
                    views::BubbleBorder::ArrowLocation arrow_location);

  void SetBubbleArrowLocation(
      views::BubbleBorder::ArrowLocation arrow_location);

  void Close();

  void UpdateBounds();

 private:
  // Creates models to use.
  void CreateModel();

  // Overridden from views::WidgetDelegateView:
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual gfx::ImageSkia GetWindowAppIcon() OVERRIDE;
  virtual bool HasHitTestMask() const OVERRIDE;
  virtual void GetHitTestMask(gfx::Path* mask) const OVERRIDE;

  // Overridden from views::View:
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from views::BubbleDelegate:
  virtual gfx::Rect GetBubbleBounds() OVERRIDE;

  // Overridden from SearchBoxViewDelegate:
  virtual void QueryChanged(SearchBoxView* sender) OVERRIDE;

  // Overridden from SearchResultListViewDelegate:
  virtual void OpenResult(const SearchResult& result,
                          int event_flags) OVERRIDE;
  virtual void InvokeResultAction(const SearchResult& result,
                                  int action_index,
                                  int event_flags) OVERRIDE;

  scoped_ptr<AppListModel> model_;
  scoped_ptr<AppListViewDelegate> delegate_;

  AppListBubbleBorder* bubble_border_;  // Owned by views hierarchy.
  SearchBoxView* search_box_view_;  // Owned by views hierarchy.
  ContentsView* contents_view_;  // Owned by views hierarchy.

  DISALLOW_COPY_AND_ASSIGN(AppListView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_VIEW_H_
