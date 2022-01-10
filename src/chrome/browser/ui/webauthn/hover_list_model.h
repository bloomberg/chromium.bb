// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_HOVER_LIST_MODEL_H_
#define CHROME_BROWSER_UI_WEBAUTHN_HOVER_LIST_MODEL_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/check.h"
#include "ui/gfx/vector_icon_types.h"

// List model that controls which item is added to WebAuthN UI views.
// HoverListModel is observed by Observer which represents the actual
// UI view component.
class HoverListModel {
 public:
  enum class ListItemChangeType {
    kAddToViewComponent,
    kRemoveFromViewComponent,
  };

  class Observer {
   public:
    virtual void OnListItemAdded(int item_tag) = 0;
    virtual void OnListItemRemoved(int removed_list_item_tag) = 0;
    virtual void OnListItemChanged(int changed_list_item_tag,
                                   ListItemChangeType type) = 0;
  };

  HoverListModel() = default;

  HoverListModel(const HoverListModel&) = delete;
  HoverListModel& operator=(const HoverListModel&) = delete;

  virtual ~HoverListModel() = default;

  virtual bool ShouldShowPlaceholderForEmptyList() const = 0;
  virtual std::u16string GetPlaceholderText() const = 0;
  // GetPlaceholderIcon may return nullptr to indicate that no icon should be
  // added. This is distinct from using an empty icon as the latter will still
  // take up as much space as any other icon.
  virtual const gfx::VectorIcon* GetPlaceholderIcon() const = 0;
  virtual std::vector<int> GetThrobberTags() const = 0;
  virtual std::vector<int> GetButtonTags() const = 0;
  virtual std::u16string GetItemText(int item_tag) const = 0;
  virtual std::u16string GetDescriptionText(int item_tag) const = 0;
  // GetItemIcon may return nullptr to indicate that no icon should be added.
  // This is distinct from using an empty icon as the latter will still take up
  // as much space as any other icon.
  virtual const gfx::VectorIcon* GetItemIcon(int item_tag) const = 0;
  virtual void OnListItemSelected(int item_tag) = 0;
  virtual size_t GetPreferredItemCount() const = 0;
  // StyleForTwoLines returns true if the items in the list should lay out
  // with the assumption that there will be both item and description text.
  // This causes items with no description text to be larger than strictly
  // necessary so that all items, including those with descriptions, are the
  // same height.
  virtual bool StyleForTwoLines() const = 0;

  void SetObserver(Observer* observer) {
    DCHECK(!observer_);
    observer_ = observer;
  }

  void RemoveObserver() { observer_ = nullptr; }

 protected:
  Observer* observer() { return observer_; }

 private:
  Observer* observer_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_HOVER_LIST_MODEL_H_
