// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_EVENTS_EVENT_CONSTANTS_EVENTS_EVENT_DISPATCHER_H_
#define UI_BASE_EVENTS_EVENT_CONSTANTS_EVENTS_EVENT_DISPATCHER_H_

#include "ui/base/events/event.h"
#include "ui/base/events/event_constants.h"
#include "ui/base/events/event_target.h"
#include "ui/base/ui_export.h"

namespace ui {

// Dispatches events to appropriate targets.
class UI_EXPORT EventDispatcher {
 public:
  EventDispatcher() {}
  virtual ~EventDispatcher() {}

  // Returns whether an event can still be dispatched to a target. (e.g. during
  // event dispatch, one of the handlers may have destroyed the target, in which
  // case the event can no longer be dispatched to the target).
  virtual bool CanDispatchToTarget(EventTarget* target) = 0;

  // Allows the subclass to add additional event handlers to the dispatch
  // sequence.
  virtual void ProcessPreTargetList(EventHandlerList* list) = 0;
  virtual void ProcessPostTargetList(EventHandlerList* list) = 0;

  template<class T>
  int ProcessEvent(EventTarget* target, T* event) {
    if (!target || !target->CanAcceptEvents())
      return ER_UNHANDLED;

    Event::DispatcherApi dispatcher(event);
    dispatcher.set_target(target);

    EventHandlerList list;
    target->GetPreTargetHandlers(&list);
    ProcessPreTargetList(&list);
    int result = DispatchEventToEventHandlers(list, event);
    if (result & ER_CONSUMED)
      return result;

    // If the event hasn't been consumed, trigger the default handler. Note that
    // even if the event has already been handled (i.e. return result has
    // ER_HANDLED set), that means that the event should still be processed at
    // this layer, however it should not be processed in the next layer of
    // abstraction.
    if (CanDispatchToTarget(target)) {
      result |= DispatchEventToSingleHandler(target, event);
      if (result & ER_CONSUMED)
        return result;
    }

    if (!CanDispatchToTarget(target))
      return result;

    list.clear();
    target->GetPostTargetHandlers(&list);
    ProcessPostTargetList(&list);
    result |= DispatchEventToEventHandlers(list, event);
    return result;
  }

 private:
  template<class T>
  int DispatchEventToEventHandlers(EventHandlerList& list, T* event) {
    int result = ER_UNHANDLED;
    for (EventHandlerList::const_iterator it = list.begin(),
            end = list.end(); it != end; ++it) {
      result |= DispatchEventToSingleHandler((*it), event);
      if (result & ER_CONSUMED)
        return result;
    }
    return result;
  }

  EventResult DispatchEventToSingleHandler(EventHandler* handler,
                                           ui::KeyEvent* event) {
    return handler->OnKeyEvent(event);
  }

  EventResult DispatchEventToSingleHandler(EventHandler* handler,
                                           ui::MouseEvent* event) {
    return handler->OnMouseEvent(event);
  }

  EventResult DispatchEventToSingleHandler(EventHandler* handler,
                                           ui::ScrollEvent* event) {
    return handler->OnScrollEvent(event);
  }

  EventResult DispatchEventToSingleHandler(EventHandler* handler,
                                           ui::TouchEvent* event) {
    // TODO(sad): This needs fixing (especially for the QUEUED_ status).
    TouchStatus status = handler->OnTouchEvent(event);
    return status == ui::TOUCH_STATUS_UNKNOWN ? ER_UNHANDLED :
           status == ui::TOUCH_STATUS_QUEUED_END ? ER_CONSUMED :
                                                   ER_HANDLED;
  }

  EventResult DispatchEventToSingleHandler(EventHandler* handler,
                                           ui::GestureEvent* event) {
    return handler->OnGestureEvent(event);
  }

  DISALLOW_COPY_AND_ASSIGN(EventDispatcher);
};

}  // namespace ui

#endif  // UI_BASE_EVENTS_EVENT_CONSTANTS_EVENTS_EVENT_DISPATCHER_H_
