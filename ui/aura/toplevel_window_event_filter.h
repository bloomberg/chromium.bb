// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TOPLEVEL_WINDOW_EVENT_FILTER_H_
#define UI_AURA_TOPLEVEL_WINDOW_EVENT_FILTER_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/aura/event_filter.h"
#include "ui/gfx/point.h"

namespace aura {

class Window;
class MouseEvent;

class ToplevelWindowEventFilter : public EventFilter {
 public:
  explicit ToplevelWindowEventFilter(Window* owner);
  ~ToplevelWindowEventFilter();

  // Overridden from EventFilter:
  virtual bool OnMouseEvent(Window* target, MouseEvent* event) OVERRIDE;

 private:
  // Moves the target window and all of its parents to the front of their
  // respective z-orders.
  // NOTE: this does NOT activate the window.
  void MoveWindowToFront(Window* target);

  void UpdateCursorForWindowComponent();

  gfx::Point mouse_down_offset_;
  int window_component_;

  DISALLOW_COPY_AND_ASSIGN(ToplevelWindowEventFilter);
};

}  // namespace aura

#endif  // UI_AURA_TOPLEVEL_WINDOW_EVENT_FILTER_H_
