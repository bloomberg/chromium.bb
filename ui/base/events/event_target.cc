// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/events/event_target.h"

#include <algorithm>

#include "base/logging.h"

namespace ui {

EventTarget::EventTarget()
    : target_handler_(NULL) {
}

EventTarget::~EventTarget() {
}

void EventTarget::AddPreTargetHandler(EventHandler* handler) {
  pre_target_list_.push_back(handler);
}

void EventTarget::RemovePreTargetHandler(EventHandler* handler) {
  pre_target_list_.erase(std::remove(pre_target_list_.begin(),
      pre_target_list_.end(), handler));
}

void EventTarget::AddPostTargetHandler(EventHandler* handler) {
  post_target_list_.push_back(handler);
}

void EventTarget::RemovePostTargetHandler(EventHandler* handler) {
  post_target_list_.erase(std::remove(post_target_list_.begin(),
      post_target_list_.end(), handler));
}

void EventTarget::GetPreTargetHandlers(EventHandlerList* list) {
  EventTarget* target = this->GetParentTarget() ? this->GetParentTarget()
                                                : this;
  while (target) {
    EventHandlerList::reverse_iterator it, rend;
    for (it = target->pre_target_list().rbegin(),
            rend = target->pre_target_list().rend();
        it != rend;
        ++it) {
      list->insert(list->begin(), *it);
    }
    target = target->GetParentTarget();
  }
}

void EventTarget::GetPostTargetHandlers(EventHandlerList* list) {
  EventTarget* target = this;
  while (target) {
    for (EventHandlerList::iterator it = target->post_target_list().begin(),
        end = target->post_target_list().end(); it != end; ++it) {
      list->push_back(*it);
    }
    target = target->GetParentTarget();
  }
}

EventResult EventTarget::OnKeyEvent(EventTarget* target,
                                    KeyEvent* event) {
  CHECK_EQ(this, target);
  return target_handler_ ? target_handler_->OnKeyEvent(target, event) :
                           ER_UNHANDLED;
}

EventResult EventTarget::OnMouseEvent(EventTarget* target,
                                      MouseEvent* event) {
  CHECK_EQ(this, target);
  return target_handler_ ? target_handler_->OnMouseEvent(target, event) :
                           ER_UNHANDLED;
}

EventResult EventTarget::OnScrollEvent(EventTarget* target,
                                       ScrollEvent* event) {
  CHECK_EQ(this, target);
  return target_handler_ ? target_handler_->OnScrollEvent(target, event) :
                           ER_UNHANDLED;
}

TouchStatus EventTarget::OnTouchEvent(EventTarget* target,
                                      TouchEvent* event) {
  CHECK_EQ(this, target);
  return target_handler_ ? target_handler_->OnTouchEvent(target, event) :
                           TOUCH_STATUS_UNKNOWN;
}

EventResult EventTarget::OnGestureEvent(EventTarget* target,
                                        GestureEvent* event) {
  CHECK_EQ(this, target);
  return target_handler_ ? target_handler_->OnGestureEvent(target, event) :
                           ER_UNHANDLED;
}

}  // namespace ui
