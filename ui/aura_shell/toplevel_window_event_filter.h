// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_TOPLEVEL_WINDOW_EVENT_FILTER_H_
#define UI_AURA_SHELL_TOPLEVEL_WINDOW_EVENT_FILTER_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/aura/event_filter.h"
#include "ui/aura_shell/aura_shell_export.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

namespace aura {
class Window;
class MouseEvent;
}

namespace aura_shell {

class AURA_SHELL_EXPORT ToplevelWindowEventFilter : public aura::EventFilter {
 public:
  explicit ToplevelWindowEventFilter(aura::Window* owner);
  virtual ~ToplevelWindowEventFilter();

  // Overridden from EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;

 protected:
  // Returns the |window_component_|. See the variable definition below for
  // more details.
  int window_component() const { return window_component_; }

 private:
  // Moves the target window and all of its parents to the front of their
  // respective z-orders.
  // NOTE: this does NOT activate the window.
  void MoveWindowToFront(aura::Window* target);

  // Called during a drag to resize/position the window.
  // The return value is returned by OnMouseEvent() above.
  bool HandleDrag(aura::Window* target, aura::MouseEvent* event);

  // Updates the |window_component_| using the |event|'s location.
  void UpdateWindowComponentForEvent(aura::Window* window,
                                     aura::MouseEvent* event);

  // Calculates the new origin of the window during a drag.
  gfx::Point GetOriginForDrag(int bounds_change,
                              int delta_x,
                              int delta_y) const;

  // Calculates the new size of the |target| window during a drag.
  // If the size is constrained, |delta_x| and |delta_y| may be clamped.
  gfx::Size GetSizeForDrag(int bounds_change,
                           aura::Window* target,
                           int* delta_x,
                           int* delta_y) const;

  // Calculates new width of a window during a drag where the mouse
  // position changed by |delta_x|.  |delta_x| may be clamped if the window
  // size is constrained by |min_width|.
  int GetWidthForDrag(int size_change_direction,
                      int min_width,
                      int* delta_x) const;

  // Calculates new height of a window during a drag where the mouse
  // position changed by |delta_y|.  |delta_y| may be clamped if the window
  // size is constrained by |min_height|.
  int GetHeightForDrag(int size_change_direction,
                       int min_height,
                       int* delta_y) const;

  // The mouse position in the target window when the mouse was pressed, in
  // the target window's parent's coordinates.
  gfx::Point mouse_down_offset_in_parent_;

  // The bounds of the target window when the mouse was pressed.
  gfx::Rect mouse_down_bounds_;

  // The window component (hit-test code) the mouse is currently over.
  int window_component_;

  DISALLOW_COPY_AND_ASSIGN(ToplevelWindowEventFilter);
};

}  // namespace aura

#endif  // UI_AURA_SHELL_TOPLEVEL_WINDOW_EVENT_FILTER_H_
