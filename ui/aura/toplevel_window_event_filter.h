// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TOPLEVEL_WINDOW_EVENT_FILTER_H_
#define UI_AURA_TOPLEVEL_WINDOW_EVENT_FILTER_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/event_filter.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

namespace aura {

class Window;
class MouseEvent;

class AURA_EXPORT ToplevelWindowEventFilter : public EventFilter {
 public:
  explicit ToplevelWindowEventFilter(Window* owner);
  virtual ~ToplevelWindowEventFilter();

  // Overridden from EventFilter:
  virtual bool OnMouseEvent(Window* target, MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus OnTouchEvent(Window* target,
                                       TouchEvent* event) OVERRIDE;

 protected:
  // Returns the |window_component_|. See the variable definition below for
  // more details.
  int window_component() const { return window_component_; }

 private:
  // Moves the target window and all of its parents to the front of their
  // respective z-orders.
  // NOTE: this does NOT activate the window.
  void MoveWindowToFront(Window* target);

  // Called during a drag to resize/position the window.
  // The return value is returned by OnMouseEvent() above.
  bool HandleDrag(Window* target, MouseEvent* event);

  // Updates the |window_component_| using the |event|'s location.
  void UpdateWindowComponentForEvent(Window* window, MouseEvent* event);

  // Calculates the new origin of the window during a drag.
  gfx::Point GetOriginForDrag(int bounds_change,
                              Window* target,
                              MouseEvent* event) const;

  // Calculates the new size of the window during a drag.
  gfx::Size GetSizeForDrag(int bounds_change,
                           Window* target,
                           MouseEvent* event) const;

  // The mouse position in the target window when the mouse was pressed, in
  // target window coordinates.
  gfx::Point mouse_down_offset_in_target_;

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

#endif  // UI_AURA_TOPLEVEL_WINDOW_EVENT_FILTER_H_
