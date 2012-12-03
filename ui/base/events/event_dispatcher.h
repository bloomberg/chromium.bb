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
  void ProcessEvent(EventTarget* target, T* event) {
    if (!target || !target->CanAcceptEvents())
      return;

    ScopedDispatchHelper dispatch_helper(event);
    dispatch_helper.set_target(target);

    EventHandlerList list;
    target->GetPreTargetHandlers(&list);
    dispatch_helper.set_phase(EP_PRETARGET);
    DispatchEventToEventHandlers(list, event);
    if (event->stopped_propagation())
      return;

    // If the event hasn't been consumed, trigger the default handler. Note that
    // even if the event has already been handled (i.e. return result has
    // ER_HANDLED set), that means that the event should still be processed at
    // this layer, however it should not be processed in the next layer of
    // abstraction.
    if (CanDispatchToTarget(target)) {
      dispatch_helper.set_phase(EP_TARGET);
      DispatchEvent(target, event);
      if (event->stopped_propagation())
        return;
    }

    if (!CanDispatchToTarget(target))
      return;

    list.clear();
    target->GetPostTargetHandlers(&list);
    dispatch_helper.set_phase(EP_POSTTARGET);
    DispatchEventToEventHandlers(list, event);
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
  void DispatchEventToEventHandlers(EventHandlerList& list, T* event) {
    for (EventHandlerList::const_iterator it = list.begin(),
            end = list.end(); it != end; ++it) {
      DispatchEvent((*it), event);
      if (event->stopped_propagation())
        return;
    }
  }

  // Dispatches an event, and makes sure it sets ER_CONSUMED on the
  // event-handling result if the dispatcher itself has been destroyed during
  // dispatching the event to the event handler.
  template<class T>
  void DispatchEvent(EventHandler* handler, T* event) {
    // If the target has been invalidated or deleted, don't dispatch the event.
    if (!CanDispatchToTarget(event->target())) {
      event->StopPropagation();
      return;
    }
    bool destroyed = false;
    set_on_destroy_ = &destroyed;

    // Do not use base::AutoReset for |current_event_|. The EventDispatcher can
    // be destroyed by the event-handler during the event-dispatch. That would
    // cause invalid memory-write when AutoReset tries to restore the value.
    Event* old_event = current_event_;
    current_event_ = event;
    DispatchEventToSingleHandler(handler, event);
    if (destroyed) {
      event->StopPropagation();
    } else {
      current_event_ = old_event;
      set_on_destroy_ = NULL;
    }
  }

  void DispatchEventToSingleHandler(EventHandler* handler, Event* event);

  // This is used to track whether the dispatcher has been destroyed in the
  // middle of dispatching an event.
  bool* set_on_destroy_;

  Event* current_event_;

  DISALLOW_COPY_AND_ASSIGN(EventDispatcher);
};

}  // namespace ui

#endif  // UI_BASE_EVENTS_EVENT_DISPATCHER_H_
