// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_LAUNCHER_MODEL_H_
#define UI_AURA_SHELL_LAUNCHER_MODEL_H_
#pragma once

#include <vector>

#include "base/observer_list.h"
#include "ui/aura_shell/aura_shell_export.h"

namespace views {
class View;
}

namespace aura_shell {

class LauncherModelObserver;

// Model used by LauncherView.
class AURA_SHELL_EXPORT LauncherModel {
 public:
  LauncherModel();
  ~LauncherModel();

  // Adds a new view to the model. The model takes ownership of the view.
  void AddItem(views::View* view, int index, bool draggable);

  // Removes an item by the view.
  void RemoveItem(views::View* view);

  // Removes the item at |index|.
  void RemoveItemAt(int index);

  // The selected index; -1 if nothing is selected.
  void SetSelectedIndex(int index);
  int selected_index() const { return selected_index_; }

  // Returns the index of |view|, or -1 if view isn't contained in this model.
  int IndexOfItemByView(views::View* view);
  int item_count() const { return static_cast<int>(items_.size()); }
  views::View* view_at(int index) { return items_[index].view; }
  bool is_draggable(int index) { return items_[index].draggable; }

  void AddObserver(LauncherModelObserver* observer);
  void RemoveObserver(LauncherModelObserver* observer);

 private:
  struct Item {
    Item() : view(NULL), draggable(false) {}

    // The view, we own this.
    views::View* view;
    bool draggable;
  };
  typedef std::vector<Item> Items;

  Items items_;
  int selected_index_;
  ObserverList<LauncherModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(LauncherModel);
};

}  // namespace aura_shell

#endif  // UI_AURA_SHELL_LAUNCHER_MODEL_H_
