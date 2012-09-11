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

  // Allows the subclass to add additional event handlers to the dispatch
  // sequence.
  virtual void ProcessPreTargetList(EventHandlerList* list) = 0;
  virtual void ProcessPostTargetList(EventHandlerList* list) = 0;

  template<class T>
  int ProcessEvent(EventTarget* target, T* event) {
    if (!target || !target->CanAcceptEvents())
      return ER_UNHANDLED;

    ScopedDispatchHelper dispatch_helper(event);
    dispatch_helper.set_target(target);

    EventHandlerList list;
    target->GetPreTargetHandlers(&list);
    ProcessPreTargetList(&list);
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
      result |= DispatchEventToSingleHandler(target, event);
      dispatch_helper.set_result(event->result() | result);
      if (result & ER_CONSUMED)
        return result;
    }

    if (!CanDispatchToTarget(target))
      return result;

    list.clear();
    target->GetPostTargetHandlers(&list);
    ProcessPostTargetList(&list);
    dispatch_helper.set_phase(EP_POSTTARGET);
    result |= DispatchEventToEventHandlers(list, event);
    return result;
  }

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
      result |= DispatchEventToSingleHandler((*it), event);
      dispatch_helper.set_result(event->result() | result);
      if (result & ER_CONSUMED)
        return result;
    }
    return result;
  }

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

  DISALLOW_COPY_AND_ASSIGN(EventDispatcher);
};

}  // namespace ui

#endif  // UI_BASE_EVENTS_EVENT_DISPATCHER_H_
