/*
 * Copyright (C) 2012 Victor Carbune (victor@rosedu.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/dom/events/GenericEventQueue.h"

#include "core/dom/events/Event.h"
#include "core/probe/CoreProbes.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

GenericEventQueue* GenericEventQueue::Create(EventTarget* owner) {
  return new GenericEventQueue(owner);
}

GenericEventQueue::GenericEventQueue(EventTarget* owner)
    : owner_(owner),
      timer_(this, &GenericEventQueue::TimerFired),
      is_closed_(false) {}

GenericEventQueue::~GenericEventQueue() {}

DEFINE_TRACE(GenericEventQueue) {
  visitor->Trace(owner_);
  visitor->Trace(pending_events_);
  EventQueue::Trace(visitor);
}

bool GenericEventQueue::EnqueueEvent(const WebTraceLocation& from_here,
                                     Event* event) {
  if (is_closed_)
    return false;

  if (event->target() == owner_)
    event->SetTarget(nullptr);

  TRACE_EVENT_ASYNC_BEGIN1("event", "GenericEventQueue:enqueueEvent", event,
                           "type", event->type().Ascii());
  EventTarget* target = event->target() ? event->target() : owner_.Get();
  probe::AsyncTaskScheduled(target->GetExecutionContext(), event->type(),
                            event);
  pending_events_.push_back(event);

  if (!timer_.IsActive())
    timer_.StartOneShot(0, from_here);

  return true;
}

bool GenericEventQueue::CancelEvent(Event* event) {
  bool found = pending_events_.Contains(event);

  if (found) {
    EventTarget* target = event->target() ? event->target() : owner_.Get();
    probe::AsyncTaskCanceled(target->GetExecutionContext(), event);
    pending_events_.erase(pending_events_.Find(event));
    TRACE_EVENT_ASYNC_END2("event", "GenericEventQueue:enqueueEvent", event,
                           "type", event->type().Ascii(), "status",
                           "cancelled");
  }

  if (pending_events_.IsEmpty())
    timer_.Stop();

  return found;
}

void GenericEventQueue::TimerFired(TimerBase*) {
  DCHECK(!timer_.IsActive());
  DCHECK(!pending_events_.IsEmpty());

  HeapVector<Member<Event>> pending_events;
  pending_events_.swap(pending_events);

  for (const auto& pending_event : pending_events) {
    Event* event = pending_event.Get();
    EventTarget* target = event->target() ? event->target() : owner_.Get();
    CString type(event->type().Ascii());
    probe::AsyncTask async_task(target->GetExecutionContext(), event);
    TRACE_EVENT_ASYNC_STEP_INTO1("event", "GenericEventQueue:enqueueEvent",
                                 event, "dispatch", "type", type);
    target->DispatchEvent(pending_event);
    TRACE_EVENT_ASYNC_END1("event", "GenericEventQueue:enqueueEvent", event,
                           "type", type);
  }
}

void GenericEventQueue::Close() {
  is_closed_ = true;
  CancelAllEvents();
}

void GenericEventQueue::CancelAllEvents() {
  timer_.Stop();

  for (const auto& pending_event : pending_events_) {
    Event* event = pending_event.Get();
    TRACE_EVENT_ASYNC_END2("event", "GenericEventQueue:enqueueEvent", event,
                           "type", event->type().Ascii(), "status",
                           "cancelled");
    EventTarget* target = event->target() ? event->target() : owner_.Get();
    probe::AsyncTaskCanceled(target->GetExecutionContext(), event);
  }
  pending_events_.clear();
}

bool GenericEventQueue::HasPendingEvents() const {
  return pending_events_.size();
}

}  // namespace blink
