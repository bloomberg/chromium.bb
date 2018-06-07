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

#include "third_party/blink/renderer/core/dom/events/media_element_event_queue.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

MediaElementEventQueue* MediaElementEventQueue::Create(EventTarget* owner) {
  return new MediaElementEventQueue(owner);
}

MediaElementEventQueue::MediaElementEventQueue(EventTarget* owner)
    : ContextLifecycleObserver(owner->GetExecutionContext()),
      owner_(owner),
      is_closed_(false) {
  if (!GetExecutionContext())
    Close(nullptr);
}

MediaElementEventQueue::~MediaElementEventQueue() = default;

void MediaElementEventQueue::Trace(blink::Visitor* visitor) {
  visitor->Trace(owner_);
  visitor->Trace(pending_events_);
  ContextLifecycleObserver::Trace(visitor);
}

bool MediaElementEventQueue::EnqueueEvent(const base::Location& from_here,
                                          Event* event) {
  if (is_closed_)
    return false;

  DCHECK(GetExecutionContext());

  TRACE_EVENT_ASYNC_BEGIN1("event", "MediaElementEventQueue:enqueueEvent",
                           event, "type", event->type().Ascii());
  probe::AsyncTaskScheduled(GetExecutionContext(), event->type(), event);

  pending_events_.insert(event);
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kMediaElementEvent)
      ->PostTask(FROM_HERE,
                 WTF::Bind(&MediaElementEventQueue::DispatchEvent,
                           WrapPersistent(this), WrapPersistent(event)));
  return true;
}

bool MediaElementEventQueue::RemoveEvent(Event* event) {
  auto found = pending_events_.find(event);
  if (found == pending_events_.end())
    return false;
  pending_events_.erase(found);
  return true;
}

void MediaElementEventQueue::DispatchEvent(Event* event) {
  if (!RemoveEvent(event))
    return;

  DCHECK(GetExecutionContext());

  CString type(event->type().Ascii());
  probe::AsyncTask async_task(GetExecutionContext(), event);
  TRACE_EVENT_ASYNC_STEP_INTO1("event", "MediaElementEventQueue:enqueueEvent",
                               event, "dispatch", "type", type);
  EventTarget* target = event->target() ? event->target() : owner_.Get();
  target->DispatchEvent(event);
  TRACE_EVENT_ASYNC_END1("event", "MediaElementEventQueue:enqueueEvent", event,
                         "type", type);
}

void MediaElementEventQueue::CancelAllEvents() {
  if (!GetExecutionContext()) {
    DCHECK(!pending_events_.size());
    return;
  }
  DoCancelAllEvents(GetExecutionContext());
}

bool MediaElementEventQueue::HasPendingEvents() const {
  return pending_events_.size() > 0;
}

void MediaElementEventQueue::ContextDestroyed(ExecutionContext* context) {
  Close(context);
}

void MediaElementEventQueue::Close(ExecutionContext* context) {
  is_closed_ = true;
  DoCancelAllEvents(context);
  owner_.Clear();
}

void MediaElementEventQueue::DoCancelAllEvents(ExecutionContext* context) {
  DCHECK(!pending_events_.size() || context);
  for (auto& event : pending_events_) {
    TRACE_EVENT_ASYNC_END2("event", "MediaElementEventQueue:enqueueEvent",
                           event, "type", event->type().Ascii(), "status",
                           "cancelled");
    probe::AsyncTaskCanceled(context, event);
  }
  pending_events_.clear();
}

}  // namespace blink
