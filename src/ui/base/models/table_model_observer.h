// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_TABLE_MODEL_OBSERVER_H_
#define UI_BASE_MODELS_TABLE_MODEL_OBSERVER_H_

#include "ui/base/ui_base_export.h"

namespace ui {

// Observer for a TableModel. Anytime the model changes, it must notify its
// observer.
class UI_BASE_EXPORT TableModelObserver {
 public:
  // Invoked when the model has been completely changed.
  virtual void OnModelChanged() = 0;

  // Invoked when a range of items has changed.
  virtual void OnItemsChanged(int start, int length) = 0;

  // Invoked when new items have been added.
  virtual void OnItemsAdded(int start, int length) = 0;

  // Invoked when a range of items has been removed.
  virtual void OnItemsRemoved(int start, int length) = 0;

  // Invoked when a range of items has been moved to a different position.
  virtual void OnItemsMoved(int old_start, int length, int new_start) {}

 protected:
  virtual ~TableModelObserver() {}
};

}  // namespace ui

#endif  // UI_BASE_MODELS_TABLE_MODEL_OBSERVER_H_
