// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_SHELL_ACCELERATOR_CONTROLLER_H_
#define UI_AURA_SHELL_SHELL_ACCELERATOR_CONTROLLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura_shell/aura_shell_export.h"
#include "ui/base/accelerators/accelerator.h"

namespace ui {
class AcceleratorManager;
}

namespace aura_shell {

// ShellAcceleratorController provides functions for registering or
// unregistering global keyboard accelerators, which are handled earlier than
// any windows. It also implements several handlers as an accelerator target.
class AURA_SHELL_EXPORT ShellAcceleratorController
    : public ui::AcceleratorTarget {
 public:
  ShellAcceleratorController();
  virtual ~ShellAcceleratorController();

  // Register a global keyboard accelerator for the specified target. If
  // multiple targets are registered for an accelerator, a target registered
  // later has higher priority.
  void Register(const ui::Accelerator& accelerator,
                ui::AcceleratorTarget* target);

  // Unregister the specified keyboard accelerator for the specified target.
  void Unregister(const ui::Accelerator& accelerator,
                  ui::AcceleratorTarget* target);

  // Unregister all keyboard accelerators for the specified target.
  void UnregisterAll(ui::AcceleratorTarget* target);

  // Activate the target associated with the specified accelerator.
  // First, AcceleratorPressed handler of the most recently registered target
  // is called, and if that handler processes the event (i.e. returns true),
  // this method immediately returns. If not, we do the same thing on the next
  // target, and so on.
  // Returns true if an accelerator was activated.
  bool Process(const ui::Accelerator& accelerator);

  // Overridden from ui::AcceleratorTarget:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;

 private:
  scoped_ptr<ui::AcceleratorManager> accelerator_manager_;

  DISALLOW_COPY_AND_ASSIGN(ShellAcceleratorController);
};

}  // namespace aura_shell

#endif  // UI_AURA_SHELL_SHELL_ACCELERATOR_CONTROLLER_H_
