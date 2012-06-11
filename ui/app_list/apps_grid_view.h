// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APPS_GRID_VIEW_H_
#define UI_APP_LIST_APPS_GRID_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/pagination_model_observer.h"
#include "ui/base/models/list_model_observer.h"
#include "ui/views/view.h"

namespace views {
class ButtonListener;
}

namespace app_list {

class AppListItemView;
class PaginationModel;

// AppsGridView displays a grid for AppListModel::Apps sub model.
class APP_LIST_EXPORT AppsGridView : public views::View,
                                     public ui::ListModelObserver,
                                     public PaginationModelObserver {
 public:
  AppsGridView(views::ButtonListener* listener,
               PaginationModel* pagination_model);
  virtual ~AppsGridView();

  // Sets fixed layout parameters. After setting this, CalculateLayout below
  // is no longer called to dynamically choosing those layout params.
  void SetLayout(int icon_size, int cols, int rows_per_page);

  // Sets |model| to use. Note this does not take ownership of |model|.
  void SetModel(AppListModel::Apps* model);

  void SetSelectedItem(AppListItemView* item);
  void ClearSelectedItem(AppListItemView* item);
  bool IsSelectedItem(const AppListItemView* item) const;

  void EnsureItemVisible(const AppListItemView* item);

  int tiles_per_page() const { return cols_ * rows_per_page_; }

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual bool OnKeyPressed(const views::KeyEvent& event) OVERRIDE;
  virtual bool OnKeyReleased(const views::KeyEvent& event) OVERRIDE;
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

 private:
  // Updates from model.
  void Update();

  // Updates total pages and auto select first page is no page is selected.
  void UpdatePaginationModel();

  AppListItemView* CreateViewForItemAtIndex(size_t index);

  AppListItemView* GetItemViewAtIndex(int index);
  void SetSelectedItemByIndex(int index);

  // Overridden from ListModelObserver:
  virtual void ListItemsAdded(size_t start, size_t count) OVERRIDE;
  virtual void ListItemsRemoved(size_t start, size_t count) OVERRIDE;
  virtual void ListItemsChanged(size_t start, size_t count) OVERRIDE;

  // Overridden from PaginationModelObserver:
  virtual void TotalPagesChanged() OVERRIDE;
  virtual void SelectedPageChanged(int old_selected, int new_selected) OVERRIDE;
  virtual void TransitionChanged() OVERRIDE;

  AppListModel::Apps* model_;  // Owned by AppListModel.
  views::ButtonListener* listener_;
  PaginationModel* pagination_model_;  // Owned by AppListView.

  gfx::Size icon_size_;
  int cols_;
  int rows_per_page_;

  int selected_item_index_;

  DISALLOW_COPY_AND_ASSIGN(AppsGridView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_APPS_GRID_VIEW_H_
