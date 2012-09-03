// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_EVENTS_EVENT_TARGET_H_
#define UI_BASE_EVENTS_EVENT_TARGET_H_

#include "base/compiler_specific.h"
#include "ui/base/events/event_handler.h"
#include "ui/base/ui_export.h"

namespace ui {

class EventDispatcher;

class UI_EXPORT EventTarget : public EventHandler {
 public:
  typedef std::vector<EventTarget*> EventTargets;

  EventTarget();
  virtual ~EventTarget();

  virtual bool CanAcceptEvents() = 0;
  virtual EventTarget* GetParentTarget() = 0;

  // Adds a handler to receive events before the target. The handler must be
  // explicitly removed from the target before the handler is destroyed. The
  // EventTarget does not take ownership of the handler.
  void AddPreTargetHandler(EventHandler* handler);
  void RemovePreTargetHandler(EventHandler* handler);

  // Adds a handler to receive events after the target. The hanler must be
  // explicitly removed from the target before the handler is destroyed. The
  // EventTarget does not take ownership of the handler.
  void AddPostTargetHandler(EventHandler* handler);
  void RemovePostTargetHandler(EventHandler* handler);

 protected:
  void set_target_handler(EventHandler* handler) {
    target_handler_ = handler;
  }

  EventHandlerList& pre_target_list() { return pre_target_list_; }
  EventHandlerList& post_target_list() { return post_target_list_; }

 private:
  friend class EventDispatcher;

  // Returns the list of handlers that should receive the event before the
  // target. The handlers from the outermost target are first in the list, and
  // the handlers on |this| are the last in the list.
  void GetPreTargetHandlers(EventHandlerList* list);

  // Returns the list of handlers that should receive the event after the
  // target. The handlers from the outermost target are last in the list, and
  // the handlers on |this| are the first in the list.
  void GetPostTargetHandlers(EventHandlerList* list);

  // Overridden from EventHandler:
  virtual EventResult OnKeyEvent(EventTarget* target,
                                 KeyEvent* event) OVERRIDE;
  virtual EventResult OnMouseEvent(EventTarget* target,
                                   MouseEvent* event) OVERRIDE;
  virtual EventResult OnScrollEvent(EventTarget* target,
                                    ScrollEvent* event) OVERRIDE;
  virtual TouchStatus OnTouchEvent(EventTarget* target,
                                   TouchEvent* event) OVERRIDE;
  virtual EventResult OnGestureEvent(EventTarget* target,
                                     GestureEvent* event) OVERRIDE;

  EventHandlerList pre_target_list_;
  EventHandlerList post_target_list_;
  EventHandler* target_handler_;

  DISALLOW_COPY_AND_ASSIGN(EventTarget);
};

}  // namespace ui

#endif  // UI_BASE_EVENTS_EVENT_TARGET_H_
