// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_EVENTS_EVENT_DISPATCHER_H_
#define UI_BASE_EVENTS_EVENT_DISPATCHER_H_

#include "ui/base/events/event.h"
#include "ui/base/events/event_constants.h"
#include "ui/base/events/event_target.h"
#include "ui/base/ui_export.h"

namespace ui {

// Dispatches events to appropriate targets.
class UI_EXPORT EventDispatcher {
 public:
  EventDispatcher();
  virtual ~EventDispatcher();

  // Returns whether an event can still be dispatched to a target. (e.g. during
  // event dispatch, one of the handlers may have destroyed the target, in which
  // case the event can no longer be dispatched to the target).
  virtual bool CanDispatchToTarget(EventTarget* target) = 0;

  template<class T>
  int ProcessEvent(EventTarget* target, T* event) {
    if (!target || !target->CanAcceptEvents())
      return ER_UNHANDLED;

    ScopedDispatchHelper dispatch_helper(event);
    dispatch_helper.set_target(target);

    EventHandlerList list;
    target->GetPreTargetHandlers(&list);
    dispatch_helper.set_phase(EP_PRETARGET);
    int result = DispatchEventToEventHandlers(list, event);
    if (result & ER_CONSUMED)
      return result;

    // If the event hasn't been consumed, trigger the default handler. Note that
    // even if the event has already been handled (i.e. return result has
    // ER_HANDLED set), that means that the event should still be processed at
    // this layer, however it should not be processed in the next layer of
    // abstraction.
    if (CanDispatchToTarget(target)) {
      dispatch_helper.set_phase(EP_TARGET);
      result |= DispatchEvent(target, event);
      dispatch_helper.set_result(event->result() | result);
      if (result & ER_CONSUMED)
        return result;
    }

    if (!CanDispatchToTarget(target))
      return result;

    list.clear();
    target->GetPostTargetHandlers(&list);
    dispatch_helper.set_phase(EP_POSTTARGET);
    result |= DispatchEventToEventHandlers(list, event);
    return result;
  }

  const Event* current_event() const { return current_event_; }
  Event* current_event() { return current_event_; }

 private:
  class UI_EXPORT ScopedDispatchHelper : public NON_EXPORTED_BASE(
      Event::DispatcherApi) {
   public:
    explicit ScopedDispatchHelper(Event* event);
    virtual ~ScopedDispatchHelper();
   private:
    DISALLOW_COPY_AND_ASSIGN(ScopedDispatchHelper);
  };

  template<class T>
  int DispatchEventToEventHandlers(EventHandlerList& list, T* event) {
    int result = ER_UNHANDLED;
    Event::DispatcherApi dispatch_helper(event);
    for (EventHandlerList::const_iterator it = list.begin(),
            end = list.end(); it != end; ++it) {
      result |= DispatchEvent((*it), event);
      dispatch_helper.set_result(event->result() | result);
      if (result & ER_CONSUMED)
        return result;
    }
    return result;
  }

  // Dispatches an event, and makes sure it sets ER_CONSUMED on the
  // event-handling result if the dispatcher itself has been destroyed during
  // dispatching the event to the event handler.
  template<class T>
  int DispatchEvent(EventHandler* handler, T* event) {
    // If the target has been invalidated or deleted, don't dispatch the event.
    if (!CanDispatchToTarget(event->target()))
      return ui::ER_CONSUMED;
    bool destroyed = false;
    set_on_destroy_ = &destroyed;

    // Do not use base::AutoReset for |current_event_|. The EventDispatcher can
    // be destroyed by the event-handler during the event-dispatch. That would
    // cause invalid memory-write when AutoReset tries to restore the value.
    Event* old_event = current_event_;
    current_event_ = event;
    int result = DispatchEventToSingleHandler(handler, event);
    if (destroyed) {
      result |= ui::ER_CONSUMED;
    } else {
      current_event_ = old_event;
      set_on_destroy_ = NULL;
    }
    return result;
  }

  EventResult DispatchEventToSingleHandler(EventHandler* handler, Event* event);
  EventResult DispatchEventToSingleHandler(EventHandler* handler,
                                           KeyEvent* event);
  EventResult DispatchEventToSingleHandler(EventHandler* handler,
                                           MouseEvent* event);
  EventResult DispatchEventToSingleHandler(EventHandler* handler,
                                           ScrollEvent* event);
  EventResult DispatchEventToSingleHandler(EventHandler* handler,
                                           TouchEvent* event);
  EventResult DispatchEventToSingleHandler(EventHandler* handler,
                                           GestureEvent* event);

  // This is used to track whether the dispatcher has been destroyed in the
  // middle of dispatching an event.
  bool* set_on_destroy_;

  Event* current_event_;

  DISALLOW_COPY_AND_ASSIGN(EventDispatcher);
};

}  // namespace ui

#endif  // UI_BASE_EVENTS_EVENT_DISPATCHER_H_
