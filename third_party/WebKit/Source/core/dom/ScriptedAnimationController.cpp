/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
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
 *  THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "core/dom/ScriptedAnimationController.h"

#include "core/css/MediaQueryListListener.h"
#include "core/dom/Document.h"
#include "core/dom/FrameRequestCallback.h"
#include "core/dom/events/Event.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrameView.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/loader/DocumentLoader.h"
#include "core/probe/CoreProbes.h"

namespace blink {

std::pair<EventTarget*, StringImpl*> EventTargetKey(const Event* event) {
  return std::make_pair(event->target(), event->type().Impl());
}

ScriptedAnimationController::ScriptedAnimationController(Document* document)
    : document_(document), callback_collection_(document), suspend_count_(0) {}

DEFINE_TRACE(ScriptedAnimationController) {
  visitor->Trace(document_);
  visitor->Trace(callback_collection_);
  visitor->Trace(event_queue_);
  visitor->Trace(media_query_list_listeners_);
  visitor->Trace(per_frame_events_);
}

void ScriptedAnimationController::Suspend() {
  ++suspend_count_;
}

void ScriptedAnimationController::Resume() {
  // It would be nice to put an DCHECK(m_suspendCount > 0) here, but in WK1
  // resume() can be called even when suspend hasn't (if a tab was created in
  // the background).
  if (suspend_count_ > 0)
    --suspend_count_;
  ScheduleAnimationIfNeeded();
}

void ScriptedAnimationController::DispatchEventsAndCallbacksForPrinting() {
  DispatchEvents(EventNames::MediaQueryListEvent);
  CallMediaQueryListListeners();
}

ScriptedAnimationController::CallbackId
ScriptedAnimationController::RegisterCallback(FrameRequestCallback* callback) {
  CallbackId id = callback_collection_.RegisterCallback(callback);
  ScheduleAnimationIfNeeded();
  return id;
}

void ScriptedAnimationController::CancelCallback(CallbackId id) {
  callback_collection_.CancelCallback(id);
}

void ScriptedAnimationController::RunTasks() {
  Vector<WTF::Closure> tasks;
  tasks.swap(task_queue_);
  for (auto& task : tasks)
    task();
}

void ScriptedAnimationController::DispatchEvents(
    const AtomicString& event_interface_filter) {
  HeapVector<Member<Event>> events;
  if (event_interface_filter.IsEmpty()) {
    events.swap(event_queue_);
    per_frame_events_.clear();
  } else {
    HeapVector<Member<Event>> remaining;
    for (auto& event : event_queue_) {
      if (event && event->InterfaceName() == event_interface_filter) {
        per_frame_events_.erase(EventTargetKey(event.Get()));
        events.push_back(event.Release());
      } else {
        remaining.push_back(event.Release());
      }
    }
    remaining.swap(event_queue_);
  }

  for (const auto& event : events) {
    EventTarget* event_target = event->target();
    // FIXME: we should figure out how to make dispatchEvent properly virtual to
    // avoid special casting window.
    // FIXME: We should not fire events for nodes that are no longer in the
    // tree.
    probe::AsyncTask async_task(event_target->GetExecutionContext(), event);
    if (LocalDOMWindow* window = event_target->ToLocalDOMWindow())
      window->DispatchEvent(event, nullptr);
    else
      event_target->DispatchEvent(event);
  }
}

void ScriptedAnimationController::ExecuteCallbacks(double monotonic_time_now) {
  // dispatchEvents() runs script which can cause the document to be destroyed.
  if (!document_)
    return;

  double high_res_now_ms =
      1000.0 *
      document_->Loader()->GetTiming().MonotonicTimeToZeroBasedDocumentTime(
          monotonic_time_now);
  double legacy_high_res_now_ms =
      1000.0 * document_->Loader()->GetTiming().MonotonicTimeToPseudoWallTime(
                   monotonic_time_now);
  callback_collection_.ExecuteCallbacks(high_res_now_ms,
                                        legacy_high_res_now_ms);
}

void ScriptedAnimationController::CallMediaQueryListListeners() {
  MediaQueryListListeners listeners;
  listeners.Swap(media_query_list_listeners_);

  for (const auto& listener : listeners) {
    listener->NotifyMediaQueryChanged();
  }
}

bool ScriptedAnimationController::HasScheduledItems() const {
  if (suspend_count_)
    return false;

  return !callback_collection_.IsEmpty() || !task_queue_.IsEmpty() ||
         !event_queue_.IsEmpty() || !media_query_list_listeners_.IsEmpty();
}

void ScriptedAnimationController::ServiceScriptedAnimations(
    double monotonic_time_now) {
  if (!HasScheduledItems())
    return;

  CallMediaQueryListListeners();
  DispatchEvents();
  RunTasks();
  ExecuteCallbacks(monotonic_time_now);

  ScheduleAnimationIfNeeded();
}

void ScriptedAnimationController::EnqueueTask(WTF::Closure task) {
  task_queue_.push_back(std::move(task));
  ScheduleAnimationIfNeeded();
}

void ScriptedAnimationController::EnqueueEvent(Event* event) {
  probe::AsyncTaskScheduled(event->target()->GetExecutionContext(),
                            event->type(), event);
  event_queue_.push_back(event);
  ScheduleAnimationIfNeeded();
}

void ScriptedAnimationController::EnqueuePerFrameEvent(Event* event) {
  if (!per_frame_events_.insert(EventTargetKey(event)).is_new_entry)
    return;
  EnqueueEvent(event);
}

void ScriptedAnimationController::EnqueueMediaQueryChangeListeners(
    HeapVector<Member<MediaQueryListListener>>& listeners) {
  for (const auto& listener : listeners) {
    media_query_list_listeners_.insert(listener);
  }
  ScheduleAnimationIfNeeded();
}

void ScriptedAnimationController::ScheduleAnimationIfNeeded() {
  if (!HasScheduledItems())
    return;

  if (!document_)
    return;

  if (LocalFrameView* frame_view = document_->View())
    frame_view->ScheduleAnimation();
}

}  // namespace blink
