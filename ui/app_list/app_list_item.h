// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_ITEM_H_
#define UI_APP_LIST_APP_LIST_ITEM_H_

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/sync/model/string_ordinal.h"
#include "ui/app_list/app_list_export.h"
#include "ui/gfx/image/image_skia.h"

class FastShowPickler;

namespace ui {
class MenuModel;
}

namespace app_list {

class AppListItemList;
class AppListItemListTest;
class AppListItemObserver;
class AppListModel;

// AppListItem provides icon and title to be shown in a AppListItemView
// and action to be executed when the AppListItemView is activated.
class APP_LIST_EXPORT AppListItem {
 public:
  explicit AppListItem(const std::string& id);
  virtual ~AppListItem();

  void SetIcon(const gfx::ImageSkia& icon);
  const gfx::ImageSkia& icon() const { return icon_; }

  const std::string& GetDisplayName() const {
    return short_name_.empty() ? name_ : short_name_;
  }

  const std::string& name() const { return name_; }
  // Should only be used in tests; otheriwse use GetDisplayName().
  const std::string& short_name() const { return short_name_; }

  void set_highlighted(bool highlighted) { highlighted_ = highlighted; }
  bool highlighted() const { return highlighted_; }

  void SetIsInstalling(bool is_installing);
  bool is_installing() const { return is_installing_; }

  void SetPercentDownloaded(int percent_downloaded);
  int percent_downloaded() const { return percent_downloaded_; }

  bool IsInFolder() const { return !folder_id_.empty(); }

  const std::string& id() const { return id_; }
  const std::string& folder_id() const { return folder_id_; }
  const syncer::StringOrdinal& position() const { return position_; }

  void AddObserver(AppListItemObserver* observer);
  void RemoveObserver(AppListItemObserver* observer);

  // Activates (opens) the item. Does nothing by default.
  virtual void Activate(int event_flags);

  // Returns a static const char* identifier for the subclass (defaults to "").
  // Pointers can be compared for quick type checking.
  virtual const char* GetItemType() const;

  // Returns the context menu model for this item, or NULL if there is currently
  // no menu for the item (e.g. during install).
  // Note the returned menu model is owned by this item.
  virtual ui::MenuModel* GetContextMenuModel();

  // Returns the item matching |id| contained in this item (e.g. if the item is
  // a folder), or NULL if the item was not found or this is not a container.
  virtual AppListItem* FindChildItem(const std::string& id);

  // Returns the number of child items if it has any (e.g. is a folder) or 0.
  virtual size_t ChildItemCount() const;

  // Called when the extension preference changed. Used by ExtensionAppItem
  // to update icon overlays.
  virtual void OnExtensionPreferenceChanged();

  // Utility functions for sync integration tests.
  virtual bool CompareForTest(const AppListItem* other) const;
  virtual std::string ToDebugString() const;

 protected:
  friend class ::FastShowPickler;
  friend class AppListItemList;
  friend class AppListItemListTest;
  friend class AppListModel;

  // These should only be called by AppListModel or in tests so that name
  // changes trigger update notifications.

  // Sets the full name of the item. Clears any shortened name.
  void SetName(const std::string& name);

  // Sets the full name and an optional shortened name of the item (e.g. to use
  // if the full name is too long to fit in a view).
  void SetNameAndShortName(const std::string& name,
                           const std::string& short_name);

  void set_position(const syncer::StringOrdinal& new_position) {
    DCHECK(new_position.IsValid());
    position_ = new_position;
  }

  void set_folder_id(const std::string& folder_id) { folder_id_ = folder_id; }

 private:
  friend class AppListModelTest;

  const std::string id_;
  std::string folder_id_;  // Id of containing folder; empty if top level item.
  syncer::StringOrdinal position_;
  gfx::ImageSkia icon_;

  // The full name of an item. Used for display if |short_name_| is empty.
  std::string name_;

  // A shortened name for the item, used for display.
  std::string short_name_;

  bool highlighted_;
  bool is_installing_;
  int percent_downloaded_;

  base::ObserverList<AppListItemObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AppListItem);
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_ITEM_H_
