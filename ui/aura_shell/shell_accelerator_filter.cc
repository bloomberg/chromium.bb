// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/shell_accelerator_filter.h"

#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura_shell/shell.h"
#include "ui/aura_shell/shell_accelerator_controller.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"

namespace {
const int kModifierFlagMask = (ui::EF_SHIFT_DOWN |
                               ui::EF_CONTROL_DOWN |
                               ui::EF_ALT_DOWN);
}  // namespace

namespace aura_shell {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// ShellAcceleratorFilter, public:

ShellAcceleratorFilter::ShellAcceleratorFilter()
    : EventFilter(aura::Desktop::GetInstance()) {
}

ShellAcceleratorFilter::~ShellAcceleratorFilter() {
}

////////////////////////////////////////////////////////////////////////////////
// ShellAcceleratorFilter, EventFilter implementation:

bool ShellAcceleratorFilter::PreHandleKeyEvent(aura::Window* target,
                                               aura::KeyEvent* event) {
  if (event->type() == ui::ET_KEY_PRESSED &&
      Shell::GetInstance()->accelerator_controller()->Process(
          ui::Accelerator(event->key_code(),
                          event->flags() & kModifierFlagMask))) {
    return true;
  }
  return false;
}

bool ShellAcceleratorFilter::PreHandleMouseEvent(aura::Window* target,
                                                 aura::MouseEvent* event) {
  return false;
}

ui::TouchStatus ShellAcceleratorFilter::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

}  // namespace internal
}  // namespace aura_shell
