// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APPS_GRID_VIEW_TEST_API_H_
#define ASH_APP_LIST_VIEWS_APPS_GRID_VIEW_TEST_API_H_

#include <string>

#include "ash/app_list/views/apps_grid_view.h"
#include "base/macros.h"

namespace gfx {
class Rect;
}

namespace views {
class View;
}

namespace ash {

class AppListItemView;
class PagedViewStructure;

namespace test {

class AppsGridViewTestApi {
 public:
  explicit AppsGridViewTestApi(AppsGridView* view);

  AppsGridViewTestApi(const AppsGridViewTestApi&) = delete;
  AppsGridViewTestApi& operator=(const AppsGridViewTestApi&) = delete;

  ~AppsGridViewTestApi();

  views::View* GetViewAtModelIndex(int index) const;

  void LayoutToIdealBounds();

  // Returns tile bounds for item in the provided `row` and `col` on the current
  // apps grid page. It does not require an item to exist in the provided spot.
  // NOTE: In RTL layout column with index 0 will be rightmost column.
  gfx::Rect GetItemTileRectOnCurrentPageAt(int row, int col) const;

  void PressItemAt(int index);

  int TilesPerPage(int page) const;

  int AppsOnPage(int page) const;

  AppListItemView* GetViewAtIndex(GridIndex index) const;

  AppListItemView* GetViewAtVisualIndex(int page, int slot) const;

  // Returns the name of the item specified by the grid location.
  const std::string& GetNameAtVisualIndex(int page, int slot) const;

  // Returns tile bounds for item in the provided grid `slot` and `page`.
  // Item slot indicates the index of the item in the apps grid.
  // NOTE: In RTL UI, slot 0 is the top right position in the grid.
  gfx::Rect GetItemTileRectAtVisualIndex(int page, int slot) const;

  void WaitForItemMoveAnimationDone();

  void Update() { view_->Update(); }

  // Returns the drag icon proxy view's bounds in the apps grid coordinates.
  // Returns empty bounds if the icon proxy has not been created.
  gfx::Rect GetDragIconBoundsInAppsGridView();

  AppListItemList* GetItemList() { return view_->item_list_; }

  PagedViewStructure* GetPagedViewStructure() {
    return &view_->view_structure_;
  }

 private:
  AppsGridView* view_;
};

}  // namespace test
}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APPS_GRID_VIEW_TEST_API_H_
