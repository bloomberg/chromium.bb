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

// This is similar to base::AutoReset<>. The difference is that this takes a
// conditional variable, and resets the variable only if the value of the
// conditional variable hasn't changed. This gets by the restriction of
// base::AutoReset<> that the AutoReset instance needs to have a shorter
// lifetime than the scoped_variable (which does not hold for EventDispatcher
// when the dispatcher gets destroyed during event dispatch).
template<class T>
class ConditionalAutoReset {
 public:
  ConditionalAutoReset(T* scoped_variable, T new_value, bool* condition)
      : scoped_variable_(scoped_variable),
        original_value_(*scoped_variable),
        conditional_(condition),
        initial_condition_(*condition) {
    *scoped_variable_ = new_value;
  }

  ~ConditionalAutoReset() {
    if (*conditional_ == initial_condition_)
      *scoped_variable_ = original_value_;
  }

 private:
  T* scoped_variable_;
  T original_value_;
  bool* conditional_;
  bool initial_condition_;

  DISALLOW_COPY_AND_ASSIGN(ConditionalAutoReset);
};

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
    if (!target || !target->CanAcceptEvent(*event))
      return;

    ScopedDispatchHelper dispatch_helper(event);
    dispatch_helper.set_target(target);

    bool destroyed = false;
    EventHandlerList list;
    ConditionalAutoReset<EventHandlerList*>
        reset(&handler_list_, &list, &destroyed);
    target->GetPreTargetHandlers(&list);

    dispatch_helper.set_phase(EP_PRETARGET);
    DispatchEventToEventHandlers(list, event, &destroyed);
    if (event->stopped_propagation())
      return;

    // If the event hasn't been consumed, trigger the default handler. Note that
    // even if the event has already been handled (i.e. return result has
    // ER_HANDLED set), that means that the event should still be processed at
    // this layer, however it should not be processed in the next layer of
    // abstraction.
    if (CanDispatchToTarget(target)) {
      dispatch_helper.set_phase(EP_TARGET);
      DispatchEvent(target, event, &destroyed);
      if (event->stopped_propagation())
        return;
    }

    if (!CanDispatchToTarget(target))
      return;

    list.clear();
    target->GetPostTargetHandlers(&list);
    dispatch_helper.set_phase(EP_POSTTARGET);
    DispatchEventToEventHandlers(list, event, &destroyed);
  }

  const Event* current_event() const { return current_event_; }
  Event* current_event() { return current_event_; }

  void OnHandlerDestroyed(EventHandler* handler);

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
  void DispatchEventToEventHandlers(EventHandlerList& list,
                                    T* event,
                                    bool* destroyed) {
    for (EventHandlerList::const_iterator it = list.begin(),
            end = list.end(); it != end; ++it) {
      (*it)->dispatcher_ = this;
    }

    while (!list.empty()) {
      EventHandler* handler = (*list.begin());
      if (!event->stopped_propagation())
        DispatchEvent(handler, event, destroyed);

      if (!list.empty() && *list.begin() == handler) {
        // The handler has not been destroyed (because if it were, then it would
        // have been removed from the list).
        handler->dispatcher_ = NULL;
        list.erase(list.begin());
      }
    }
  }

  // Dispatches an event, and makes sure it sets ER_CONSUMED on the
  // event-handling result if the dispatcher itself has been destroyed during
  // dispatching the event to the event handler.
  template<class T>
  void DispatchEvent(EventHandler* handler, T* event, bool* destroyed) {
    // If the target has been invalidated or deleted, don't dispatch the event.
    if (!CanDispatchToTarget(event->target())) {
      event->StopPropagation();
      return;
    }

    ConditionalAutoReset<Event*> event_reset(&current_event_, event, destroyed);
    ConditionalAutoReset<bool*> reset(&set_on_destroy_, destroyed, destroyed);

    DispatchEventToSingleHandler(handler, event);
    if (*destroyed)
      event->StopPropagation();
  }

  void DispatchEventToSingleHandler(EventHandler* handler, Event* event);

  // This is used to track whether the dispatcher has been destroyed in the
  // middle of dispatching an event.
  bool* set_on_destroy_;

  Event* current_event_;

  EventHandlerList* handler_list_;

  DISALLOW_COPY_AND_ASSIGN(EventDispatcher);
};

}  // namespace ui

#endif  // UI_BASE_EVENTS_EVENT_DISPATCHER_H_
