// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_ROOT_VIEW_H_
#define UI_VIEWS_WIDGET_ROOT_VIEW_H_
#pragma once

#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/view.h"

namespace ui {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// RootView class
//
//  A View subclass that owns a View hierarchy. Used by the Widget to perform
//  View-specific event tracking.
//
class RootView : public View,
                 public FocusTraversable {
 public:
  RootView(Widget* widget, View* contents_view);
  virtual ~RootView();

  // Overridden from View:
  virtual void OnViewRemoved(View* parent, View* child) OVERRIDE;
  virtual bool OnKeyPressed(const KeyEvent& event) OVERRIDE;
  virtual bool OnKeyReleased(const KeyEvent& event) OVERRIDE;
  virtual bool OnMouseWheel(const MouseWheelEvent& event) OVERRIDE;
  virtual bool OnMousePressed(const MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual void OnMouseMoved(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const MouseEvent& event) OVERRIDE;
  virtual void Paint(gfx::Canvas* canvas) OVERRIDE;
  virtual void InvalidateRect(const gfx::Rect& invalid_rect) OVERRIDE;
  virtual Widget* GetWidget() const OVERRIDE;

 private:
  // Overridden from FocusTraversable:
  virtual const FocusSearch* GetFocusSearch() const OVERRIDE;
  virtual FocusTraversable* GetFocusTraversableParent() const OVERRIDE;
  virtual View* GetFocusTraversableParentView() const OVERRIDE;

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

  //
  FocusSearch focus_search_;

  FocusTraversable* focus_traversable_parent_;
  View* focus_traversable_parent_view_;

  DISALLOW_COPY_AND_ASSIGN(RootView);
};

}  // namespace internal
}  // namespace ui

#endif  // UI_VIEWS_WIDGET_ROOT_VIEW_H_
