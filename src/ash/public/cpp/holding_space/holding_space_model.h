// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_MODEL_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_MODEL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "base/callback.h"
#include "base/observer_list.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash {

class HoldingSpaceModelObserver;

// The data model for the temporary holding space UI. It contains the list of
// items that should be shown in the temporary holding space UI - each item will
// represent a piece of data added to the holding space by the user (for
// example, text, URLs, or images).
// The main goal of the class is to provide UI implementation agnostic
// information about items added to the holding space, and to provide an
// interface to propagate holding space changes between ash and Chrome.
class ASH_PUBLIC_EXPORT HoldingSpaceModel {
 public:
  using ItemList = std::vector<std::unique_ptr<HoldingSpaceItem>>;

  // A class which performs an atomic update of a single holding space item on
  // destruction, notifying model observers of the event if a change in state
  // did in fact occur.
  class ScopedItemUpdate {
   public:
    ScopedItemUpdate(const ScopedItemUpdate&) = delete;
    ScopedItemUpdate& operator=(const ScopedItemUpdate&) = delete;
    ~ScopedItemUpdate();

    // Sets the backing file for the item and returns a reference to `this`.
    ScopedItemUpdate& SetBackingFile(const base::FilePath& file_path,
                                     const GURL& file_system_url);

    // Sets if progress of the item is `paused` and returns a ref to `this`.
    // NOTE: Only in-progress holding space items can be paused.
    ScopedItemUpdate& SetPaused(bool paused);

    // Sets the `progress` of the item and returns a reference to `this`.
    // NOTE: Only in-progress holding space items can be progressed.
    ScopedItemUpdate& SetProgress(const HoldingSpaceProgress& progress);

    // Sets the secondary text that should be shown for the item and returns a
    // reference to `this`.
    ScopedItemUpdate& SetSecondaryText(
        const absl::optional<std::u16string>& secondary_text);

    // Sets the text that should be shown for the item and returns a reference
    // to `this`. If absent, the lossy display name of the backing file will be
    // used.
    ScopedItemUpdate& SetText(const absl::optional<std::u16string>& text);

   private:
    friend class HoldingSpaceModel;
    ScopedItemUpdate(HoldingSpaceModel* model, HoldingSpaceItem* item);

    HoldingSpaceModel* const model_;
    HoldingSpaceItem* const item_;

    absl::optional<base::FilePath> file_path_;
    absl::optional<GURL> file_system_url_;
    absl::optional<bool> paused_;
    absl::optional<HoldingSpaceProgress> progress_;
    absl::optional<absl::optional<std::u16string>> secondary_text_;
    absl::optional<absl::optional<std::u16string>> text_;
  };

  HoldingSpaceModel();
  HoldingSpaceModel(const HoldingSpaceModel& other) = delete;
  HoldingSpaceModel& operator=(const HoldingSpaceModel& other) = delete;
  ~HoldingSpaceModel();

  // Adds a single holding space item to the model.
  void AddItem(std::unique_ptr<HoldingSpaceItem> item);

  // Adds multiple holding space items to the model.
  void AddItems(std::vector<std::unique_ptr<HoldingSpaceItem>> items);

  // Removes a single holding space item from the model.
  void RemoveItem(const std::string& id);

  // Removes multiple holding space items from the model.
  void RemoveItems(const std::set<std::string>& ids);

  // Fully initializes a partially initialized holding space item using the
  // provided `file_system_url`. The item will be removed if `file_system_url`
  // is empty.
  void InitializeOrRemoveItem(const std::string& id,
                              const GURL& file_system_url);

  // Returns an object which, upon its destruction, performs an atomic update to
  // the holding space item associated with the specified `id`.
  std::unique_ptr<ScopedItemUpdate> UpdateItem(const std::string& id);

  // Removes all holding space items from the model for which the specified
  // `predicate` returns true.
  using Predicate = base::RepeatingCallback<bool(const HoldingSpaceItem*)>;
  void RemoveIf(Predicate predicate);

  // Invalidates image representations for items for which the specified
  // `predicate` returns true.
  void InvalidateItemImageIf(Predicate predicate);

  // Removes all the items from the model.
  void RemoveAll();

  // Gets a single holding space item.
  // Returns nullptr if the item does not exist in the model.
  const HoldingSpaceItem* GetItem(const std::string& id) const;

  // Gets a single holding space item with the specified `type` backed by the
  // specified `file_path`. Returns `nullptr` if the item does not exist in the
  // model.
  const HoldingSpaceItem* GetItem(HoldingSpaceItem::Type type,
                                  const base::FilePath& file_path) const;

  // Returns whether or not there exists a holding space item of the specified
  // `type` backed by the specified `file_path`.
  bool ContainsItem(HoldingSpaceItem::Type type,
                    const base::FilePath& file_path) const;

  // Returns `true` if the model contains any initialized items of the specified
  // `type`, `false` otherwise.
  bool ContainsInitializedItemOfType(HoldingSpaceItem::Type type) const;

  const ItemList& items() const { return items_; }

  void AddObserver(HoldingSpaceModelObserver* observer);
  void RemoveObserver(HoldingSpaceModelObserver* observer);

 private:
  // The list of items added to the model in the order they have been added to
  // the model.
  ItemList items_;

  // Caches the count of initialized items in the model for each holding space
  // item type. Used to quickly look up whether the model contains any
  // initialized items of a given type.
  std::map<HoldingSpaceItem::Type, size_t> initialized_item_counts_by_type_;

  base::ObserverList<HoldingSpaceModelObserver> observers_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_MODEL_H_
