// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/scoped_target_handler.h"

#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_target.h"
#include "ui/views/view.h"

namespace views {

ScopedTargetHandler::ScopedTargetHandler(View* view,
                                         ui::EventHandler* handler)
    : destroyed_flag_(NULL), view_(view), new_handler_(handler){
  original_handler_ = view_->SetTargetHandler(this);
}

ScopedTargetHandler::~ScopedTargetHandler() {
  EventHandler* handler = view_->SetTargetHandler(original_handler_);
  DCHECK_EQ(this, handler);
  if (destroyed_flag_)
    *destroyed_flag_ = true;
}

void ScopedTargetHandler::OnEvent(ui::Event* event) {
  bool destroyed = false;
  destroyed_flag_ = &destroyed;

  if (original_handler_)
    original_handler_->OnEvent(event);
  else
    EventHandler::OnEvent(event);

  if (destroyed)
    return;
  destroyed_flag_ = NULL;

  new_handler_->OnEvent(event);
}

void ScopedTargetHandler::OnKeyEvent(ui::KeyEvent* event) {
  static_cast<EventHandler*>(view_)->OnKeyEvent(event);
}

void ScopedTargetHandler::OnMouseEvent(ui::MouseEvent* event) {
  static_cast<EventHandler*>(view_)->OnMouseEvent(event);
}

void ScopedTargetHandler::OnScrollEvent(ui::ScrollEvent* event) {
  static_cast<EventHandler*>(view_)->OnScrollEvent(event);
}

void ScopedTargetHandler::OnTouchEvent(ui::TouchEvent* event) {
  static_cast<EventHandler*>(view_)->OnTouchEvent(event);
}

void ScopedTargetHandler::OnGestureEvent(ui::GestureEvent* event) {
  static_cast<EventHandler*>(view_)->OnGestureEvent(event);
}

void ScopedTargetHandler::OnCancelMode(ui::CancelModeEvent* event) {
  static_cast<EventHandler*>(view_)->OnCancelMode(event);
}

}  // namespace views
