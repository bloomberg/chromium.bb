// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/root_window.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/events.h"

namespace aura {
namespace internal {

RootWindow::RootWindow()
    : Window(NULL),
      mouse_pressed_handler_(NULL),
      mouse_moved_handler_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(focus_manager_(new FocusManager(this))),
      capture_window_(NULL) {
  set_name("RootWindow");
}

RootWindow::~RootWindow() {
}

bool RootWindow::HandleMouseEvent(const MouseEvent& event) {
  Window* target =
      mouse_pressed_handler_ ? mouse_pressed_handler_ : capture_window_;
  if (!target)
    target = GetEventHandlerForPoint(event.location());
  switch (event.type()) {
    case ui::ET_MOUSE_MOVED:
      HandleMouseMoved(event, target);
      break;
    case ui::ET_MOUSE_PRESSED:
      if (!mouse_pressed_handler_)
        mouse_pressed_handler_ = target;
      break;
    case ui::ET_MOUSE_RELEASED:
      mouse_pressed_handler_ = NULL;
      break;
    default:
      break;
  }
  if (target && target->delegate()) {
    MouseEvent translated_event(event, this, target);
    return target->OnMouseEvent(&translated_event);
  }
  return false;
}

bool RootWindow::HandleKeyEvent(const KeyEvent& event) {
  Window* focused_window = GetFocusManager()->focused_window();
  if (focused_window) {
    KeyEvent translated_event(event);
    return GetFocusManager()->focused_window()->OnKeyEvent(&translated_event);
  }
  return false;
}

void RootWindow::SetCapture(Window* window) {
  if (capture_window_ == window)
    return;

  if (capture_window_ && capture_window_->delegate())
    capture_window_->delegate()->OnCaptureLost();
  capture_window_ = window;

  if (capture_window_ && mouse_pressed_handler_) {
    // Make all subsequent mouse events go to the capture window. We shouldn't
    // need to send an event here as OnCaptureLost should take care of that.
    mouse_pressed_handler_ = capture_window_;
  }
}

void RootWindow::ReleaseCapture(Window* window) {
  if (capture_window_ != window)
    return;

  if (capture_window_ && capture_window_->delegate())
    capture_window_->delegate()->OnCaptureLost();
  capture_window_ = NULL;
}

void RootWindow::WindowDestroying(Window* window) {
  // Update the FocusManager if the window was focused.
  internal::FocusManager* focus_manager = GetFocusManager();
  if (focus_manager && focus_manager->focused_window() == window)
    focus_manager->SetFocusedWindow(NULL);

  Desktop::GetInstance()->WindowDestroying(window);

  // When a window is being destroyed it's likely that the WindowDelegate won't
  // want events, so we reset the mouse_pressed_handler_ and capture_window_ and
  // don't sent it release/capture lost events.
  if (mouse_pressed_handler_ == window)
    mouse_pressed_handler_ = NULL;
  if (mouse_moved_handler_ == window)
    mouse_moved_handler_ = NULL;
  if (capture_window_ == window)
    capture_window_ = NULL;
}

FocusManager* RootWindow::GetFocusManager() {
  return focus_manager_.get();
}

internal::RootWindow* RootWindow::GetRoot() {
  return this;
}

void RootWindow::HandleMouseMoved(const MouseEvent& event, Window* target) {
  if (target == mouse_moved_handler_)
    return;

  // Send an exited event.
  if (mouse_moved_handler_ && mouse_moved_handler_->delegate()) {
    MouseEvent translated_event(event, this, mouse_moved_handler_,
                                ui::ET_MOUSE_EXITED);
    mouse_moved_handler_->OnMouseEvent(&translated_event);
  }
  mouse_moved_handler_ = target;
  // Send an entered event.
  if (mouse_moved_handler_ && mouse_moved_handler_->delegate()) {
    MouseEvent translated_event(event, this, mouse_moved_handler_,
                                ui::ET_MOUSE_ENTERED);
    mouse_moved_handler_->OnMouseEvent(&translated_event);
  }
}

}  // namespace internal
}  // namespace aura
