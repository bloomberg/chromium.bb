// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_FOLDER_HEADER_VIEW_H_
#define UI_APP_LIST_VIEWS_FOLDER_HEADER_VIEW_H_

#include <string>

#include "ui/app_list/app_list_item_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
}  // namespace views

namespace app_list {

class AppListFolderItem;
class AppListFolderView;
class FolderHeaderViewDelegate;

// FolderHeaderView contains a back button and an editable folder name field.
class FolderHeaderView : public views::View,
                         public views::TextfieldController,
                         public views::ButtonListener,
                         public AppListItemObserver {
 public:
  explicit FolderHeaderView(FolderHeaderViewDelegate* delegate);
  virtual ~FolderHeaderView();

  void SetFolderItem(AppListFolderItem* folder_item);
  void UpdateFolderNameVisibility(bool visible);
  void OnFolderItemRemoved();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

 private:
  class FolderNameView;

  // Updates UI.
  void Update();

  // views::View overrides:
  virtual void Layout() OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // views::TextfieldController overrides:
  virtual void ContentsChanged(views::Textfield* sender,
                               const base::string16& new_contents) OVERRIDE;

  // views::ButtonListener overrides:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // AppListItemObserver overrides:
  virtual void ItemIconChanged() OVERRIDE;
  virtual void ItemNameChanged() OVERRIDE;
  virtual void ItemHighlightedChanged() OVERRIDE;
  virtual void ItemIsInstallingChanged() OVERRIDE;
  virtual void ItemPercentDownloadedChanged() OVERRIDE;

  AppListFolderItem* folder_item_;  // Not owned.

  views::ImageButton* back_button_;  // Owned by views hierarchy.
  FolderNameView* folder_name_view_;  // Owned by views hierarchy.

  FolderHeaderViewDelegate* delegate_;

  bool folder_name_visible_;

  DISALLOW_COPY_AND_ASSIGN(FolderHeaderView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_FOLDER_HEADER_VIEW_H_
