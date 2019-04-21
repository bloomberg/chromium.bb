// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_ACCELERATOR_HISTORY_H_
#define UI_BASE_ACCELERATORS_ACCELERATOR_HISTORY_H_

#include <set>

#include "base/macros.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ui_base_export.h"

namespace ui {

// Keeps track of the system-wide current and the most recent previous
// key accelerators.
class UI_BASE_EXPORT AcceleratorHistory {
 public:
  AcceleratorHistory();
  ~AcceleratorHistory();

  // Returns the most recent recorded accelerator.
  const Accelerator& current_accelerator() const {
    return current_accelerator_;
  }

  // Returns the most recent previously recorded accelerator that is different
  // than the current.
  const Accelerator& previous_accelerator() const {
    return previous_accelerator_;
  }

  const std::set<KeyboardCode>& currently_pressed_keys() const {
    return currently_pressed_keys_;
  }

  // Stores the given |accelerator| only if it's different than the currently
  // stored one.
  void StoreCurrentAccelerator(const Accelerator& accelerator);

  void InterruptCurrentAccelerator();

 private:
  Accelerator current_accelerator_;
  Accelerator previous_accelerator_;

  std::set<KeyboardCode> currently_pressed_keys_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorHistory);
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_ACCELERATOR_HISTORY_H_
