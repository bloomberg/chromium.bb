// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_FOLDER_ITEM_H_
#define UI_APP_LIST_APP_LIST_FOLDER_ITEM_H_

#include <string>
#include <vector>

#include "ui/app_list/app_list_export.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_item_list_observer.h"
#include "ui/app_list/app_list_item_observer.h"
#include "ui/app_list/folder_image.h"

namespace gfx {
class ImageSkia;
class Rect;
}

namespace app_list {

class AppListItemList;

// AppListFolderItem implements the model/controller for folders.
class APP_LIST_EXPORT AppListFolderItem : public AppListItem,
                                          public FolderImageObserver {
 public:
  // The folder type affects folder behavior.
  enum FolderType {
    // Default folder type.
    FOLDER_TYPE_NORMAL,
    // Items can not be moved to/from OEM folders in the UI.
    FOLDER_TYPE_OEM
  };

  static const char kItemType[];

  AppListFolderItem(const std::string& id, FolderType folder_type);
  ~AppListFolderItem() override;

  // Returns the icon of one of the top items with |item_index|.
  const gfx::ImageSkia& GetTopIcon(size_t item_index);

  // Returns the target icon bounds for |item| to fly back to its parent folder
  // icon in animation UI. If |item| is one of the top item icon, this will
  // match its corresponding top item icon in the folder icon. Otherwise,
  // the target icon bounds is centered at the |folder_icon_bounds| with
  // the same size of the top item icon.
  // The Rect returned is in the same coordinates of |folder_icon_bounds|.
  gfx::Rect GetTargetIconRectInFolderForItem(
      AppListItem* item, const gfx::Rect& folder_icon_bounds);

  AppListItemList* item_list() { return item_list_.get(); }
  const AppListItemList* item_list() const { return item_list_.get(); }

  // For tests.
  const FolderImage& folder_image() const { return folder_image_; }

  FolderType folder_type() const { return folder_type_; }

  // AppListItem overrides:
  void Activate(int event_flags) override;
  const char* GetItemType() const override;
  ui::MenuModel* GetContextMenuModel() override;
  AppListItem* FindChildItem(const std::string& id) override;
  size_t ChildItemCount() const override;
  void OnExtensionPreferenceChanged() override;
  bool CompareForTest(const AppListItem* other) const override;

  // Returns an id for a new folder.
  static std::string GenerateId();

  // FolderImageObserver overrides:
  void OnFolderImageUpdated() override;

 private:
  // The type of folder; may affect behavior of folder views.
  const FolderType folder_type_;

  // List of items in the folder.
  scoped_ptr<AppListItemList> item_list_;

  FolderImage folder_image_;

  DISALLOW_COPY_AND_ASSIGN(AppListFolderItem);
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_FOLDER_ITEM_H_
