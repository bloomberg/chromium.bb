// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_SHELL_ACCELERATOR_FILTER_H_
#define UI_AURA_SHELL_SHELL_ACCELERATOR_FILTER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/event_filter.h"
#include "ui/aura_shell/aura_shell_export.h"

namespace aura_shell {
namespace internal {

// ShellAcceleratorFilter filters key events for ShellAcceleratorControler
// handling global keyboard accelerators.
class AURA_SHELL_EXPORT ShellAcceleratorFilter : public aura::EventFilter {
 public:
  ShellAcceleratorFilter();
  virtual ~ShellAcceleratorFilter();

  // Overridden from aura::EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellAcceleratorFilter);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_SHELL_ACCELERATOR_FILTER_H_
