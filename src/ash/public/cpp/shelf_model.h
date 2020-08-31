// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SHELF_MODEL_H_
#define ASH_PUBLIC_CPP_SHELF_MODEL_H_

#include <map>
#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/shelf_item.h"
#include "base/macros.h"
#include "base/observer_list.h"

class AppWindowLauncherItemController;

namespace ash {

class ShelfItemDelegate;
class ShelfModelObserver;

// An id for the AppList item, which is added in the ShelfModel constructor.
// Generated as crx_file::id_util::GenerateId("org.chromium.applist")
ASH_PUBLIC_EXPORT extern const char kAppListId[];

// An id for the BackButton item, which is added in the ShelfModel constructor.
// Generated as crx_file::id_util::GenerateId("org.chromium.backbutton")
ASH_PUBLIC_EXPORT extern const char kBackButtonId[];

// Model used for shelf items. Owns ShelfItemDelegates but does not create them.
class ASH_PUBLIC_EXPORT ShelfModel {
 public:
  // Get or set a weak pointer to the singleton ShelfModel instance, not owned.
  static ShelfModel* Get();
  static void SetInstance(ShelfModel* shelf_model);

  // Used to mark the current shelf model mutation as user-triggered, while
  // the instance of this class is in scope.
  class ScopedUserTriggeredMutation {
   public:
    explicit ScopedUserTriggeredMutation(ShelfModel* model) : model_(model) {
      model_->current_mutation_is_user_triggered_++;
    }

    ~ScopedUserTriggeredMutation() {
      model_->current_mutation_is_user_triggered_--;
      DCHECK_GE(model_->current_mutation_is_user_triggered_, 0);
    }

   private:
    ShelfModel* model_ = nullptr;
  };

  ShelfModel();
  ~ShelfModel();

  // Pins an app with |app_id| to shelf. A running instance will get pinned.
  // If there is no running instance, a new shelf item is created and pinned.
  void PinAppWithID(const std::string& app_id);

  // Checks if the app with |app_id_| is pinned to the shelf.
  bool IsAppPinned(const std::string& app_id);

  // Unpins app item with |app_id|.
  void UnpinAppWithID(const std::string& app_id);

  // Cleans up the ShelfItemDelegates.
  void DestroyItemDelegates();

  // Adds a new item to the model. Returns the resulting index.
  int Add(const ShelfItem& item);

  // Adds the item. |index| is the requested insertion index, which may be
  // modified to meet type-based ordering. Returns the actual insertion index.
  int AddAt(int index, const ShelfItem& item);

  // Removes the item at |index|.
  void RemoveItemAt(int index);

  // Removes the item with id |shelf_id| and passes ownership of its
  // ShelfItemDelegate to the caller. This is useful if you want to remove an
  // item from the shelf temporarily and be able to restore its behavior later.
  std::unique_ptr<ShelfItemDelegate> RemoveItemAndTakeShelfItemDelegate(
      const ShelfID& shelf_id);

  // Returns whether the item with the given index can be swapped with the
  // next (or previous) item. Example cases when a swap cannot happen are:
  // trying to swap the first item with the previous one, trying to swap
  // the last item with the next one, trying to swap a pinned item with an
  // unpinned item.
  bool CanSwap(int index, bool with_next) const;

  // Swaps the item at the given index with the next one if |with_next| is
  // true, or with the previous one if |with_next| is false. Returns true
  // if the requested swap has happened, and false otherwise.
  bool Swap(int index, bool with_next);

  // Moves the item at |index| to |target_index|. |target_index| is in terms
  // of the model *after* the item at |index| is removed.
  void Move(int index, int target_index);

  // Resets the item at the specified index. The item's id should not change.
  void Set(int index, const ShelfItem& item);

  // Returns the ID of the currently active item, or an empty ShelfID if
  // nothing is currently active.
  const ShelfID& active_shelf_id() const { return active_shelf_id_; }

  // Returns whether the mutation that is currently being made in the model
  // was user-triggered.
  bool is_current_mutation_user_triggered() const {
    return current_mutation_is_user_triggered_ > 0;
  }

  // Sets |shelf_id| to be the newly active shelf item.
  void SetActiveShelfID(const ShelfID& shelf_id);

  // Notifies observers that the status of the item corresponding to |id|
  // has changed.
  void OnItemStatusChanged(const ShelfID& id);

  // Notifies observers that an item has been dragged off the shelf (it is still
  // being dragged).
  void OnItemRippedOff();

  // Notifies observers that an item that was dragged off the shelf has been
  // dragged back onto the shelf (it is still being dragged).
  void OnItemReturnedFromRipOff(int index);

  // Adds a record of the notification with this app id and notifies observers.
  void AddNotificationRecord(const std::string& app_id,
                             const std::string& notification_id);

  // Removes the record of the notification with matching ID and notifies
  // observers.
  void RemoveNotificationRecord(const std::string& notification_id);

  // Returns the index of the item with id |shelf_id|, or -1 if none exists.
  int ItemIndexByID(const ShelfID& shelf_id) const;

  // Returns the |index| of the item matching |type| in |items_|.
  // Returns -1 if the matching item is not found.
  int GetItemIndexForType(ShelfItemType type);

  // Returns the index of the first running application or the index where the
  // first running application would go if there are no running (non pinned)
  // applications yet.
  int FirstRunningAppIndex() const;

  // Returns an iterator into items() for the item with the specified id, or
  // items().end() if there is no item with the specified id.
  ShelfItems::const_iterator ItemByID(const ShelfID& shelf_id) const;

  // Returns the index of the matching ShelfItem or -1 if the |app_id| doesn't
  // match a ShelfItem.
  int ItemIndexByAppID(const std::string& app_id) const;

  const ShelfItems& items() const { return items_; }
  int item_count() const { return static_cast<int>(items_.size()); }

  // Sets |item_delegate| for the given |shelf_id| and takes ownership.
  void SetShelfItemDelegate(const ShelfID& shelf_id,
                            std::unique_ptr<ShelfItemDelegate> item_delegate);

  // Returns ShelfItemDelegate for |shelf_id|, or nullptr if none exists.
  ShelfItemDelegate* GetShelfItemDelegate(const ShelfID& shelf_id) const;

  // Returns AppWindowLauncherItemController for |shelf_id|, or nullptr if none
  // exists.
  AppWindowLauncherItemController* GetAppWindowLauncherItemController(
      const ShelfID& shelf_id);

  void AddObserver(ShelfModelObserver* observer);
  void RemoveObserver(ShelfModelObserver* observer);

 private:
  // Makes sure |index| is in line with the type-based order of items. If that
  // is not the case, adjusts index by shifting it to the valid range and
  // returns the new value.
  int ValidateInsertionIndex(ShelfItemType type, int index) const;

  // Finds the app corresponding to |app_id|, sets ShelfItem.has_notification,
  // and notifies observers.
  void UpdateItemNotificationsAndNotifyObservers(const std::string& app_id);

  ShelfItems items_;

  // The shelf ID of the currently active shelf item, or an empty ID if
  // nothing is active.
  ShelfID active_shelf_id_;

  // A counter to determine whether any mutation currently in progress in
  // the model is the result of a manual user intervention. If a shelf item
  // is added once an app has been installed, it is not considered a direct
  // user interaction.
  int current_mutation_is_user_triggered_ = 0;

  // Maps one app id to a set of all matching notification ids.
  std::map<std::string, std::set<std::string>> app_id_to_notification_id_;
  // Maps one notification id to one app id.
  std::map<std::string, std::string> notification_id_to_app_id_;

  base::ObserverList<ShelfModelObserver>::Unchecked observers_;

  std::map<ShelfID, std::unique_ptr<ShelfItemDelegate>>
      id_to_item_delegate_map_;

  DISALLOW_COPY_AND_ASSIGN(ShelfModel);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELF_MODEL_H_
