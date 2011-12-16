// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_LAUNCHER_MODEL_H_
#define UI_AURA_SHELL_LAUNCHER_MODEL_H_
#pragma once

#include <vector>

#include "base/observer_list.h"
#include "ui/aura_shell/aura_shell_export.h"
#include "ui/aura_shell/launcher/launcher_types.h"

namespace aura {
class Window;
}

namespace aura_shell {

class LauncherModelObserver;

// Model used by LauncherView.
class AURA_SHELL_EXPORT LauncherModel {
 public:
  LauncherModel();
  ~LauncherModel();

  // Adds a new item to the model.
  void Add(int index, const LauncherItem& item);

  // Removes the item at |index|.
  void RemoveItemAt(int index);

  // Moves the item at |index| to |target_index|. |target_index| is in terms
  // of the model *after* the item at |index| is removed.
  void Move(int index, int target_index);

  // Changes the images of the specified item.
  void SetTabbedImages(int index, const LauncherTabbedImages& images);
  void SetAppImage(int index, const SkBitmap& image);

  // Sends LauncherItemImagesWillChange() to the observers. Used when the images
  // are going to change for an item, but not for a while.
  void SetPendingUpdate(int index);

  // Returns the index of the item with the specified window.
  int ItemIndexByWindow(aura::Window* window);

  LauncherItems::const_iterator ItemByWindow(aura::Window* window) const;

  const LauncherItems& items() const { return items_; }
  int item_count() const { return static_cast<int>(items_.size()); }

  void AddObserver(LauncherModelObserver* observer);
  void RemoveObserver(LauncherModelObserver* observer);

 private:
  LauncherItems items_;
  ObserverList<LauncherModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(LauncherModel);
};

}  // namespace aura_shell

#endif  // UI_AURA_SHELL_LAUNCHER_MODEL_H_
