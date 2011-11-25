// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_ACCELERATOR_MANAGER_H_
#define UI_BASE_ACCELERATORS_ACCELERATOR_MANAGER_H_
#pragma once

#include <list>
#include <map>

#include "base/basictypes.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ui_export.h"

namespace ui {

// The AcceleratorManger is used to handle keyboard accelerators.
class UI_EXPORT AcceleratorManager {
 public:
  AcceleratorManager();
  ~AcceleratorManager();

  // Register a keyboard accelerator for the specified target. If multiple
  // targets are registered for an accelerator, a target registered later has
  // higher priority.
  // Note that we are currently limited to accelerators that are either:
  // - a key combination including Ctrl or Alt
  // - the escape key
  // - the enter key
  // - any F key (F1, F2, F3 ...)
  // - any browser specific keys (as available on special keyboards)
  void Register(const Accelerator& accelerator, AcceleratorTarget* target);

  // Unregister the specified keyboard accelerator for the specified target.
  void Unregister(const Accelerator& accelerator, AcceleratorTarget* target);

  // Unregister all keyboard accelerator for the specified target.
  void UnregisterAll(AcceleratorTarget* target);

  // Activate the target associated with the specified accelerator.
  // First, AcceleratorPressed handler of the most recently registered target
  // is called, and if that handler processes the event (i.e. returns true),
  // this method immediately returns. If not, we do the same thing on the next
  // target, and so on.
  // Returns true if an accelerator was activated.
  bool Process(const Accelerator& accelerator);

  // Returns the AcceleratorTarget that should be activated for the specified
  // keyboard accelerator, or NULL if no view is registered for that keyboard
  // accelerator.
  AcceleratorTarget* GetCurrentTarget(const Accelerator& accelertor) const;

 private:
  // The accelerators and associated targets.
  typedef std::list<AcceleratorTarget*> AcceleratorTargetList;
  typedef std::map<Accelerator, AcceleratorTargetList> AcceleratorMap;
  AcceleratorMap accelerators_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorManager);
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_ACCELERATOR_MANAGER_H_
