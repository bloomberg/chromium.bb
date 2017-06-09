// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_ALL_APPS_TILE_ITEM_VIEW_H_
#define UI_APP_LIST_VIEWS_ALL_APPS_TILE_ITEM_VIEW_H_

#include <vector>

#include "ui/app_list/views/tile_item_view.h"

namespace app_list {

class AppListView;
class ContentsView;

// A tile item for the "All apps" button on the start page.
class AllAppsTileItemView : public TileItemView {
 public:
  AllAppsTileItemView(ContentsView* contents_view, AppListView* app_list_view);

  ~AllAppsTileItemView() override;

  void UpdateIcon();

  // TileItemView overrides:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  ContentsView* contents_view_;
  AppListView* const app_list_view_;  // Owned by the views hierarchy.

  DISALLOW_COPY_AND_ASSIGN(AllAppsTileItemView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_ALL_APPS_TILE_ITEM_VIEW_H_
