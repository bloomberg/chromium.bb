// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_FOLDER_HEADER_VIEW_H_
#define UI_APP_LIST_VIEWS_FOLDER_HEADER_VIEW_H_

#include <string>

#include "ui/app_list/app_list_export.h"
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

namespace test {
class FolderHeaderViewTest;
}

// FolderHeaderView contains a back button and an editable folder name field.
class APP_LIST_EXPORT FolderHeaderView : public views::View,
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
  virtual gfx::Size GetPreferredSize() const OVERRIDE;

 private:
  class FolderNameView;
  friend class test::FolderHeaderViewTest;

  // Updates UI.
  void Update();

  // Updates the accessible name of the folder name control.
  void UpdateFolderNameAccessibleName();

  // Gets and sets the folder name for test.
  const base::string16& GetFolderNameForTest();
  void SetFolderNameForTest(const base::string16& name);

  // Returns true if folder name is enabled, only for testing use.
  bool IsFolderNameEnabledForTest() const;

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

  const base::string16 folder_name_placeholder_text_;

  FolderHeaderViewDelegate* delegate_;

  bool folder_name_visible_;

  DISALLOW_COPY_AND_ASSIGN(FolderHeaderView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_FOLDER_HEADER_VIEW_H_
