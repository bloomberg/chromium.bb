// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_TILE_ITEM_VIEW_H_
#define UI_APP_LIST_VIEWS_TILE_ITEM_VIEW_H_

#include "ui/app_list/app_list_export.h"
#include "ui/app_list/search_result_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/custom_button.h"

namespace views {
class ImageView;
class Label;
}

namespace app_list {

class SearchResult;

// The view for a tile in the app list on the start/search page.
class APP_LIST_EXPORT TileItemView : public views::CustomButton,
                                     public views::ButtonListener,
                                     public SearchResultObserver {
 public:
  TileItemView();
  virtual ~TileItemView();

  void SetSearchResult(SearchResult* item);

 private:
  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from SearchResultObserver:
  virtual void OnIconChanged() OVERRIDE;
  virtual void OnResultDestroying() OVERRIDE;

  // Owned by the model provided by the AppListViewDelegate.
  SearchResult* item_;

  views::ImageView* icon_;  // Owned by views hierarchy.
  views::Label* title_;     // Owned by views hierarchy.

  DISALLOW_COPY_AND_ASSIGN(TileItemView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_TILE_ITEM_VIEW_H_
