// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_LAUNCHER_MODEL_OBSERVER_H_
#define UI_AURA_SHELL_LAUNCHER_MODEL_OBSERVER_H_
#pragma once

#include "ui/aura_shell/aura_shell_export.h"

namespace aura_shell {

class AURA_SHELL_EXPORT LauncherModelObserver {
 public:
  // Invoked after an item has been added to the model.
  virtual void LauncherItemAdded(int index) = 0;

  // Invoked after an item has been removed. |index| is the index the item was
  // at.
  virtual void LauncherItemRemoved(int index) = 0;

  // Invoked when the selection changes.
  virtual void LauncherSelectionChanged() = 0;

 protected:
  ~LauncherModelObserver() {}
};

}  // namespace aura_shell

#endif  // UI_AURA_SHELL_LAUNCHER_MODEL_OBSERVER_H_
