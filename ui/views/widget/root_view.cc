// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/root_view.h"

#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ui {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// RootView, public:

RootView::RootView(Widget* widget, View* contents_view)
    : widget_(widget),
      mouse_pressed_handler_(NULL),
      mouse_move_handler_(NULL) {
  SetLayoutManager(new FillLayout);
  AddChildView(contents_view);
}

RootView::~RootView() {
}

////////////////////////////////////////////////////////////////////////////////
// RootView, View overrides:

void RootView::OnViewRemoved(View* parent, View* child) {
  if (child == mouse_pressed_handler_)
    mouse_pressed_handler_ = NULL;
}

bool RootView::OnKeyPressed(const KeyEvent& event) {
  return true;
}

bool RootView::OnKeyReleased(const KeyEvent& event) {
  return true;
}

bool RootView::OnMouseWheel(const MouseWheelEvent& event) {
  return true;
}

bool RootView::OnMousePressed(const MouseEvent& event) {
  bool handled = false;

  // Find the most View most tightly enclosing the event location that wants to
  // handle events.
  mouse_pressed_handler_ = GetEventHandlerForPoint(event.location());

  // Walk up the tree from that View until we find one that handles it.
  while (mouse_pressed_handler_ && mouse_pressed_handler_ != this) {
    if (!mouse_pressed_handler_->enabled())
      break;

    MouseEvent target_event(event, this, mouse_pressed_handler_);
    drag_info_.Reset();
    bool handled = mouse_pressed_handler_->MousePressed(target_event,
                                                        &drag_info_);
    // MousePressed() may have resulted in the handler removing itself from the
    // hierarchy, which will NULL-out |mouse_pressed_handler_|.
    if (!mouse_pressed_handler_)
      break;

    if (handled)
      return true;

    mouse_pressed_handler_ = mouse_pressed_handler_->parent();
  }
  return false;
}

bool RootView::OnMouseDragged(const MouseEvent& event) {
  // TODO(beng): Update cursor.
  if (mouse_pressed_handler_)
    return mouse_pressed_handler_->MouseDragged(event, &drag_info_);
  return false;
}

void RootView::OnMouseReleased(const MouseEvent& event) {
  // TODO(beng): Update cursor.
  if (mouse_pressed_handler_) {
    MouseEvent released_event(event, this, mouse_pressed_handler_);
    View* mouse_pressed_handler = mouse_pressed_handler_;
    mouse_pressed_handler_ = NULL;
    mouse_pressed_handler->MouseReleased(released_event);
  }
}

void RootView::OnMouseCaptureLost() {
  if (mouse_pressed_handler_) {
    View* mouse_pressed_handler = mouse_pressed_handler_;
    mouse_pressed_handler_ = NULL;
    mouse_pressed_handler->OnMouseCaptureLost();
  }
}

void RootView::OnMouseMoved(const MouseEvent& event) {
  // TODO(beng): Update cursor.
  View* v = GetEventHandlerForPoint(event.location());
  while (v && !v->enabled() && (v != mouse_move_handler_))
    v = v->parent();
  if (v && v != this) {
    if (v != mouse_move_handler_) {
      OnMouseExited(event);
      mouse_move_handler_ = v;
      MouseEvent entered_event(event, this, mouse_move_handler_);
      mouse_move_handler_->OnMouseEntered(entered_event);
    }
    MouseEvent moved_event(event, this, mouse_move_handler_);
    mouse_move_handler_->OnMouseMoved(moved_event);
  } else {
    OnMouseExited(event);
  }
}

void RootView::OnMouseExited(const MouseEvent& event) {
  if (mouse_move_handler_) {
    MouseEvent exited_event(event, this, mouse_move_handler_);
    mouse_move_handler_->OnMouseExited(exited_event);
    mouse_move_handler_ = NULL;
  }
}

void RootView::Paint(gfx::Canvas* canvas) {
  // Public pass-thru to protected base class method.
  View::Paint(canvas);
}

void RootView::InvalidateRect(const gfx::Rect& invalid_rect) {
  widget_->InvalidateRect(invalid_rect);
}

Widget* RootView::GetWidget() const {
  return widget_;
}

////////////////////////////////////////////////////////////////////////////////
// RootView, private:

}  // namespace internal
}  // namespace ui
