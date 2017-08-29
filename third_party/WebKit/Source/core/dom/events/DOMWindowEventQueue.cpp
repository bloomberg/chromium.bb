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

#include "core/dom/events/DOMWindowEventQueue.h"

#include "core/dom/TaskRunnerHelper.h"
#include "core/dom/events/Event.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/SuspendableTimer.h"
#include "core/probe/CoreProbes.h"

namespace blink {

class DOMWindowEventQueueTimer final
    : public GarbageCollectedFinalized<DOMWindowEventQueueTimer>,
      public SuspendableTimer {
  USING_GARBAGE_COLLECTED_MIXIN(DOMWindowEventQueueTimer);
  WTF_MAKE_NONCOPYABLE(DOMWindowEventQueueTimer);

 public:
  DOMWindowEventQueueTimer(DOMWindowEventQueue* event_queue,
                           ExecutionContext* context)
      // This queue is unthrottled because throttling IndexedDB events may break
      // scenarios where several tabs, some of which are backgrounded, access
      // the same database concurrently.
      : SuspendableTimer(context, TaskType::kUnthrottled),
        event_queue_(event_queue) {}

  // Eager finalization is needed to promptly stop this timer object.
  // (see DOMTimer comment for more.)
  EAGERLY_FINALIZE();
  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(event_queue_);
    SuspendableTimer::Trace(visitor);
  }

 private:
  virtual void Fired() { event_queue_->PendingEventTimerFired(); }

  Member<DOMWindowEventQueue> event_queue_;
};

DOMWindowEventQueue* DOMWindowEventQueue::Create(ExecutionContext* context) {
  return new DOMWindowEventQueue(context);
}

DOMWindowEventQueue::DOMWindowEventQueue(ExecutionContext* context)
    : pending_event_timer_(new DOMWindowEventQueueTimer(this, context)),
      is_closed_(false) {
  pending_event_timer_->SuspendIfNeeded();
}

DOMWindowEventQueue::~DOMWindowEventQueue() {}

DEFINE_TRACE(DOMWindowEventQueue) {
  visitor->Trace(pending_event_timer_);
  visitor->Trace(queued_events_);
  EventQueue::Trace(visitor);
}

bool DOMWindowEventQueue::EnqueueEvent(const WebTraceLocation& from_here,
                                       Event* event) {
  if (is_closed_)
    return false;

  DCHECK(event->target());
  probe::AsyncTaskScheduled(event->target()->GetExecutionContext(),
                            event->type(), event);

  bool was_added = queued_events_.insert(event).is_new_entry;
  DCHECK(was_added);  // It should not have already been in the list.

  if (!pending_event_timer_->IsActive())
    pending_event_timer_->StartOneShot(0, from_here);

  return true;
}

bool DOMWindowEventQueue::CancelEvent(Event* event) {
  HeapListHashSet<Member<Event>, 16>::iterator it = queued_events_.find(event);
  bool found = it != queued_events_.end();
  if (found) {
    probe::AsyncTaskCanceled(event->target()->GetExecutionContext(), event);
    queued_events_.erase(it);
  }
  if (queued_events_.IsEmpty())
    pending_event_timer_->Stop();
  return found;
}

void DOMWindowEventQueue::Close() {
  is_closed_ = true;
  pending_event_timer_->Stop();
  for (const auto& queued_event : queued_events_) {
    if (queued_event) {
      probe::AsyncTaskCanceled(queued_event->target()->GetExecutionContext(),
                               queued_event);
    }
  }
  queued_events_.clear();
}

void DOMWindowEventQueue::PendingEventTimerFired() {
  DCHECK(!pending_event_timer_->IsActive());
  DCHECK(!queued_events_.IsEmpty());

  // Insert a marker for where we should stop.
  DCHECK(!queued_events_.Contains(nullptr));
  bool was_added = queued_events_.insert(nullptr).is_new_entry;
  DCHECK(was_added);  // It should not have already been in the list.

  while (!queued_events_.IsEmpty()) {
    HeapListHashSet<Member<Event>, 16>::iterator iter = queued_events_.begin();
    Event* event = *iter;
    queued_events_.erase(iter);
    if (!event)
      break;
    DispatchEvent(event);
  }
}

void DOMWindowEventQueue::DispatchEvent(Event* event) {
  EventTarget* event_target = event->target();
  probe::AsyncTask async_task(event_target->GetExecutionContext(), event);
  if (LocalDOMWindow* window = event_target->ToLocalDOMWindow())
    window->DispatchEvent(event, nullptr);
  else
    event_target->DispatchEvent(event);
}

}  // namespace blink
