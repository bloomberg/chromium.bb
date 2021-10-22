// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_RECENT_APPS_VIEW_H_
#define ASH_APP_LIST_VIEWS_RECENT_APPS_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class AppListModel;
class AppListConfig;
class AppListItemView;
class AppListViewDelegate;
class SearchModel;

// The recent apps row in the "Continue" section of the bubble launcher. Shows
// a list of app icons.
class ASH_EXPORT RecentAppsView : public views::View {
 public:
  METADATA_HEADER(RecentAppsView);

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Requests that focus move up and out (usually to the continue tasks).
    virtual void MoveFocusUpFromRecents() = 0;

    // Requests that focus move down and out (usually to the apps grid).
    // `column` is the column of the items that was focused in the recent apps
    // list. The delegate should choose an appropriate item to focus.
    virtual void MoveFocusDownFromRecents(int column) = 0;
  };

  RecentAppsView(Delegate* delegate, AppListViewDelegate* view_delegate);
  RecentAppsView(const RecentAppsView&) = delete;
  RecentAppsView& operator=(const RecentAppsView&) = delete;
  ~RecentAppsView() override;

  // Sets the `AppListConfig` that should be used to configure layout of
  // `AppListItemViews` shown within this view.
  void UpdateAppListConfig(const AppListConfig* app_list_config);

  // Updates the recent apps view contents to show results provided by the
  // search model. Should be called at least once, otherwise the recent apps
  // view will not display any results.
  void ShowResults(SearchModel* search_model, AppListModel* model);

  // Returns the number of AppListItemView children.
  int GetItemViewCount() const;

  // Returns an AppListItemView child. `index` must be valid.
  AppListItemView* GetItemViewAt(int index) const;

  // See AppsGridView::DisableFocusForShowingActiveFolder().
  void DisableFocusForShowingActiveFolder(bool disabled);

  // views::View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;

 private:
  // Requests that focus move up and out (usually to the continue tasks).
  void MoveFocusUp();

  // Requests that focus move down and out (usually to the apps grid).
  void MoveFocusDown();

  // Returns the visual column of the child with focus, or -1 if there is none.
  int GetColumnOfFocusedChild() const;

  Delegate* const delegate_;
  AppListViewDelegate* const view_delegate_;
  const AppListConfig* app_list_config_ = nullptr;

  // The grid delegate for each AppListItemView.
  class GridDelegateImpl;
  std::unique_ptr<GridDelegateImpl> grid_delegate_;

  // The recent app items. Stored here because this view has child views for
  // spacing that are not AppListItemViews.
  std::vector<AppListItemView*> item_views_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_RECENT_APPS_VIEW_H_
