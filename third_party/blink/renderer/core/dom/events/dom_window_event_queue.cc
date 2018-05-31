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

#include "third_party/blink/renderer/core/dom/events/dom_window_event_queue.h"

#include "base/macros.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"

namespace blink {

DOMWindowEventQueue* DOMWindowEventQueue::Create(ExecutionContext* context) {
  return new DOMWindowEventQueue(context);
}

DOMWindowEventQueue::DOMWindowEventQueue(ExecutionContext* context)
    : context_(context), is_closed_(false) {}

DOMWindowEventQueue::~DOMWindowEventQueue() = default;

void DOMWindowEventQueue::Trace(blink::Visitor* visitor) {
  visitor->Trace(context_);
  visitor->Trace(queued_events_);
  EventQueue::Trace(visitor);
}

bool DOMWindowEventQueue::EnqueueEvent(const base::Location& from_here,
                                       Event* event) {
  if (is_closed_)
    return false;

  DCHECK(event->target());
  probe::AsyncTaskScheduled(event->target()->GetExecutionContext(),
                            event->type(), event);

  bool was_added = queued_events_.insert(event).is_new_entry;
  DCHECK(was_added);  // It should not have already been in the list.

  // This queue is unthrottled because throttling IndexedDB events may break
  // scenarios where several tabs, some of which are backgrounded, access
  // the same database concurrently.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      context_->GetTaskRunner(TaskType::kUnthrottled);

  // Pass the event as a weak persistent so that GC can collect an event-related
  // object like IDBTransaction as soon as possible.
  task_runner->PostTask(
      FROM_HERE, WTF::Bind(&DOMWindowEventQueue::DispatchEvent,
                           WrapPersistent(this), WrapWeakPersistent(event)));

  return true;
}

bool DOMWindowEventQueue::CancelEvent(Event* event) {
  if (!RemoveEvent(event))
    return false;
  probe::AsyncTaskCanceled(event->target()->GetExecutionContext(), event);
  return true;
}

void DOMWindowEventQueue::Close() {
  is_closed_ = true;
  for (const auto& queued_event : queued_events_) {
    if (queued_event) {
      probe::AsyncTaskCanceled(queued_event->target()->GetExecutionContext(),
                               queued_event);
    }
  }
  queued_events_.clear();
}

bool DOMWindowEventQueue::RemoveEvent(Event* event) {
  auto found = queued_events_.find(event);
  if (found == queued_events_.end())
    return false;
  queued_events_.erase(found);
  return true;
}

void DOMWindowEventQueue::DispatchEvent(Event* event) {
  if (!event || !RemoveEvent(event))
    return;

  EventTarget* event_target = event->target();
  probe::AsyncTask async_task(event_target->GetExecutionContext(), event);
  if (LocalDOMWindow* window = event_target->ToLocalDOMWindow())
    window->DispatchEvent(event, nullptr);
  else
    event_target->DispatchEvent(event);
}

}  // namespace blink
