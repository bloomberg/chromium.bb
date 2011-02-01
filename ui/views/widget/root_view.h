// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_ROOT_VIEW_H_
#define UI_VIEWS_WIDGET_ROOT_VIEW_H_
#pragma once

#include "ui/views/view.h"

namespace ui {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// RootView class
//
//  A View subclass that owns a View hierarchy. Used by the Widget to perform
//  View-specific event tracking.
//
class RootView : public View {
 public:
  RootView(Widget* widget, View* contents_view);
  virtual ~RootView();

  // Overridden from View:
  virtual void OnViewRemoved(View* parent, View* child);
  virtual bool OnKeyPressed(const KeyEvent& event);
  virtual bool OnKeyReleased(const KeyEvent& event);
  virtual bool OnMouseWheel(const MouseWheelEvent& event);
  virtual bool OnMousePressed(const MouseEvent& event);
  virtual bool OnMouseDragged(const MouseEvent& event);
  virtual void OnMouseReleased(const MouseEvent& event);
  virtual void OnMouseCaptureLost();
  virtual void OnMouseMoved(const MouseEvent& event);
  virtual void OnMouseExited(const MouseEvent& event);
  virtual void Paint(gfx::Canvas* canvas);
  virtual void InvalidateRect(const gfx::Rect& invalid_rect);
  virtual Widget* GetWidget() const;

 private:
  Widget* widget_;

  // The View that the mouse was pressed down on. Used to track drag operations
  // and to target mouse-released events at the correct view.
  View* mouse_pressed_handler_;

  // The View that the mouse is currently over. Used to track mouse-exited
  // events as the mouse moves from view to view, and when the mouse leaves the
  // bounds of the containing Widget.
  View* mouse_move_handler_;

  // State captured on mouse press that would be useful for a potential drag
  // operation.
  DragInfo drag_info_;

  DISALLOW_COPY_AND_ASSIGN(RootView);
};

}  // namespace internal
}  // namespace ui

#endif  // UI_VIEWS_WIDGET_ROOT_VIEW_H_
