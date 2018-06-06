/*
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
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
 *
 */

#include "third_party/blink/renderer/core/dom/events/event_queue_impl.h"

#include "base/macros.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"

namespace blink {

EventQueueImpl* EventQueueImpl::Create(ExecutionContext* context,
                                       TaskType task_type) {
  return new EventQueueImpl(context, task_type);
}

EventQueueImpl::EventQueueImpl(ExecutionContext* context, TaskType task_type)
    : ContextLifecycleObserver(context),
      task_type_(task_type),
      is_closed_(false) {}

EventQueueImpl::~EventQueueImpl() = default;

void EventQueueImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(queued_events_);
  EventQueue::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

bool EventQueueImpl::EnqueueEvent(const base::Location& from_here,
                                  Event* event) {
  if (is_closed_)
    return false;

  DCHECK(GetExecutionContext());

  DCHECK(event->target());
  probe::AsyncTaskScheduled(event->target()->GetExecutionContext(),
                            event->type(), event);

  bool was_added = queued_events_.insert(event).is_new_entry;
  DCHECK(was_added);  // It should not have already been in the list.

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetExecutionContext()->GetTaskRunner(task_type_);

  // Pass the event as a weak persistent so that GC can collect an event-related
  // object like IDBTransaction as soon as possible.
  task_runner->PostTask(
      FROM_HERE, WTF::Bind(&EventQueueImpl::DispatchEvent, WrapPersistent(this),
                           WrapWeakPersistent(event)));

  return true;
}

void EventQueueImpl::CancelAllEvents() {
  for (const auto& queued_event : queued_events_) {
    probe::AsyncTaskCanceled(queued_event->target()->GetExecutionContext(),
                             queued_event);
  }
  queued_events_.clear();
}

void EventQueueImpl::Close() {
  is_closed_ = true;
  CancelAllEvents();
}

bool EventQueueImpl::RemoveEvent(Event* event) {
  auto found = queued_events_.find(event);
  if (found == queued_events_.end())
    return false;
  queued_events_.erase(found);
  return true;
}

void EventQueueImpl::DispatchEvent(Event* event) {
  if (!event || !RemoveEvent(event))
    return;

  EventTarget* event_target = event->target();
  probe::AsyncTask async_task(event_target->GetExecutionContext(), event);
  if (LocalDOMWindow* window = event_target->ToLocalDOMWindow())
    window->DispatchEvent(event, nullptr);
  else
    event_target->DispatchEvent(event);
}

void EventQueueImpl::ContextDestroyed(ExecutionContext*) {
  Close();
}

}  // namespace blink
