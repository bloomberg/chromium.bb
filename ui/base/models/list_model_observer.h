// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_LIST_MODEL_OBSERVER_H_
#define UI_BASE_MODELS_LIST_MODEL_OBSERVER_H_

#include <stddef.h>

#include "ui/base/ui_base_export.h"

namespace ui {

class UI_BASE_EXPORT ListModelObserver {
 public:
  // Invoked after items has been added to the model.
  virtual void ListItemsAdded(size_t start, size_t count) = 0;

  // Invoked after items has been removed. |start| is the index before the
  // removal.
  virtual void ListItemsRemoved(size_t start, size_t count) = 0;

  // Invoked after an item has been moved. See ListModel::Move() for details
  // of the arguments.
  virtual void ListItemMoved(size_t index, size_t target_index) = 0;

  // Invoked after items has been changed.
  virtual void ListItemsChanged(size_t start, size_t count) = 0;

 protected:
  virtual ~ListModelObserver() {}
};

}  // namespace ui

#endif  // UI_BASE_MODELS_LIST_MODEL_OBSERVER_H_
