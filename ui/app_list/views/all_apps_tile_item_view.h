// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_ALL_APPS_TILE_ITEM_VIEW_H_
#define UI_APP_LIST_VIEWS_ALL_APPS_TILE_ITEM_VIEW_H_

#include <vector>

#include "ui/app_list/folder_image.h"
#include "ui/app_list/views/tile_item_view.h"

namespace app_list {

class AppListItemList;
class ContentsView;

// A tile item for the "All apps" button on the start page.
class AllAppsTileItemView : public TileItemView, public FolderImageObserver {
 public:
  AllAppsTileItemView(ContentsView* contents_view, AppListItemList* item_list);

  ~AllAppsTileItemView() override;

  // Generates the folder's icon from the icons of the items in the item list.
  void UpdateIcon();

  // TileItemView overrides:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // FolderImageObserver overrides:
  void OnFolderImageUpdated() override;

 private:
  ContentsView* contents_view_;

  FolderImage folder_image_;
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_ALL_APPS_TILE_ITEM_VIEW_H_
