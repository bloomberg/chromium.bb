// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_SHELL_TOOLTIP_MANAGER_H_
#define UI_AURA_SHELL_SHELL_TOOLTIP_MANAGER_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/timer.h"
#include "ui/aura/client/tooltip_client.h"
#include "ui/aura/event_filter.h"
#include "ui/aura/window_observer.h"
#include "ui/aura_shell/aura_shell_export.h"
#include "ui/gfx/point.h"

namespace aura {
class KeyEvent;
class MouseEvent;
class TouchEvent;
class Window;
}

namespace aura_shell {

// ShellTooltipManager provides tooltip functionality for aura shell.
class AURA_SHELL_EXPORT ShellTooltipManager : public aura::TooltipClient,
                                              public aura::EventFilter,
                                              public aura::WindowObserver {
 public:
  ShellTooltipManager();
  virtual ~ShellTooltipManager();

  // Overridden from aura::TooltipClient.
  void UpdateTooltip(aura::Window* target);

  // Overridden from aura::EventFilter.
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;

  // Overridden from aura::WindowObserver.
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

 private:
  class Tooltip;

  void TooltipTimerFired();

  // Updates the tooltip if required (if there is any change in the tooltip
  // text or the aura::Window.
  void UpdateIfRequired();

  aura::Window* tooltip_window_;
  string16 tooltip_text_;
  scoped_ptr<Tooltip> tooltip_;

  base::RepeatingTimer<ShellTooltipManager> tooltip_timer_;

  gfx::Point curr_mouse_loc_;

  DISALLOW_COPY_AND_ASSIGN(ShellTooltipManager);
};

}  // namespace aura_shell

#endif  // UI_AURA_SHELL_SHELL_TOOLTIP_MANAGER_H_
