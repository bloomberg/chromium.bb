// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_DESKTOP_EVENT_FILTER_H_
#define UI_AURA_SHELL_DESKTOP_EVENT_FILTER_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/aura/event_filter.h"
#include "ui/aura_shell/aura_shell_export.h"

namespace aura_shell {
namespace internal {

class AURA_SHELL_EXPORT DesktopEventFilter : public aura::EventFilter {
 public:
  DesktopEventFilter();
  virtual ~DesktopEventFilter();

  // Overridden from EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;

 private:
  // If necessary, activates |window| and changes focus.
  void ActivateIfNecessary(aura::Window* window, aura::Event* event);

  // Updates the cursor if the target provides a custom one, and provides
  // default resize cursors for window edges.
  void HandleMouseMoved(aura::Window* target, aura::MouseEvent* event);

  DISALLOW_COPY_AND_ASSIGN(DesktopEventFilter);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_DESKTOP_EVENT_FILTER_H_
