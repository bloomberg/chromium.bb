/*
 * Copyright (C) 2010 Julien Chaffraix <jchaffraix@webkit.org>  All right
 * reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/xmlhttprequest/XMLHttpRequestProgressEventThrottle.h"

#include "core/event_type_names.h"
#include "core/events/ProgressEvent.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/probe/CoreProbes.h"
#include "core/xmlhttprequest/XMLHttpRequest.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/text/AtomicString.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"

namespace blink {

static const double kMinimumProgressEventDispatchingIntervalInSeconds =
    .05;  // 50 ms per specification.

XMLHttpRequestProgressEventThrottle::DeferredEvent::DeferredEvent() {
  Clear();
}

void XMLHttpRequestProgressEventThrottle::DeferredEvent::Set(
    bool length_computable,
    unsigned long long loaded,
    unsigned long long total) {
  is_set_ = true;

  length_computable_ = length_computable;
  loaded_ = loaded;
  total_ = total;
}

void XMLHttpRequestProgressEventThrottle::DeferredEvent::Clear() {
  is_set_ = false;

  length_computable_ = false;
  loaded_ = 0;
  total_ = 0;
}

Event* XMLHttpRequestProgressEventThrottle::DeferredEvent::Take() {
  DCHECK(is_set_);

  Event* event = ProgressEvent::Create(EventTypeNames::progress,
                                       length_computable_, loaded_, total_);
  Clear();
  return event;
}

XMLHttpRequestProgressEventThrottle::XMLHttpRequestProgressEventThrottle(
    XMLHttpRequest* target)
    : TimerBase(TaskRunnerHelper::Get(TaskType::kNetworking,
                                      target->GetExecutionContext())),
      target_(target),
      has_dispatched_progress_progress_event_(false) {
  DCHECK(target);
}

XMLHttpRequestProgressEventThrottle::~XMLHttpRequestProgressEventThrottle() {}

void XMLHttpRequestProgressEventThrottle::DispatchProgressEvent(
    const AtomicString& type,
    bool length_computable,
    unsigned long long loaded,
    unsigned long long total) {
  // Given that ResourceDispatcher doesn't deliver an event when suspended,
  // we don't have to worry about event dispatching while suspended.
  if (type != EventTypeNames::progress) {
    target_->DispatchEvent(
        ProgressEvent::Create(type, length_computable, loaded, total));
    return;
  }

  if (IsActive()) {
    deferred_.Set(length_computable, loaded, total);
  } else {
    DispatchProgressProgressEvent(ProgressEvent::Create(
        EventTypeNames::progress, length_computable, loaded, total));
    StartOneShot(kMinimumProgressEventDispatchingIntervalInSeconds,
                 BLINK_FROM_HERE);
  }
}

void XMLHttpRequestProgressEventThrottle::DispatchReadyStateChangeEvent(
    Event* event,
    DeferredEventAction action) {
  XMLHttpRequest::State state = target_->readyState();
  // Given that ResourceDispatcher doesn't deliver an event when suspended,
  // we don't have to worry about event dispatching while suspended.
  if (action == kFlush) {
    if (deferred_.IsSet())
      DispatchProgressProgressEvent(deferred_.Take());

    Stop();
  } else if (action == kClear) {
    deferred_.Clear();
    Stop();
  }

  has_dispatched_progress_progress_event_ = false;
  if (state == target_->readyState()) {
    // We don't dispatch the event when an event handler associated with
    // the previously dispatched event changes the readyState (e.g. when
    // the event handler calls xhr.abort()). In such cases a
    // readystatechange should have been already dispatched if necessary.
    probe::AsyncTask async_task(target_->GetExecutionContext(), target_,
                                "progress", target_->IsAsync());
    target_->DispatchEvent(event);
  }
}

void XMLHttpRequestProgressEventThrottle::DispatchProgressProgressEvent(
    Event* progress_event) {
  XMLHttpRequest::State state = target_->readyState();
  if (target_->readyState() == XMLHttpRequest::kLoading &&
      has_dispatched_progress_progress_event_) {
    TRACE_EVENT1("devtools.timeline", "XHRReadyStateChange", "data",
                 InspectorXhrReadyStateChangeEvent::Data(
                     target_->GetExecutionContext(), target_));
    probe::AsyncTask async_task(target_->GetExecutionContext(), target_,
                                "progress", target_->IsAsync());
    target_->DispatchEvent(Event::Create(EventTypeNames::readystatechange));
  }

  if (target_->readyState() != state)
    return;

  has_dispatched_progress_progress_event_ = true;
  probe::AsyncTask async_task(target_->GetExecutionContext(), target_,
                              "progress", target_->IsAsync());
  target_->DispatchEvent(progress_event);
}

void XMLHttpRequestProgressEventThrottle::Fired() {
  if (!deferred_.IsSet()) {
    // No "progress" event was queued since the previous dispatch, we can
    // safely stop the timer.
    return;
  }

  DispatchProgressProgressEvent(deferred_.Take());

  // Watch if another "progress" ProgressEvent arrives in the next 50ms.
  StartOneShot(kMinimumProgressEventDispatchingIntervalInSeconds,
               BLINK_FROM_HERE);
}

void XMLHttpRequestProgressEventThrottle::Suspend() {
  Stop();
}

void XMLHttpRequestProgressEventThrottle::Resume() {
  if (!deferred_.IsSet())
    return;

  // Do not dispatch events inline here, since ExecutionContext is iterating
  // over the list of SuspendableObjects to resume them, and any activated JS
  // event-handler could insert new SuspendableObjects to the list.
  StartOneShot(0, BLINK_FROM_HERE);
}

DEFINE_TRACE(XMLHttpRequestProgressEventThrottle) {
  visitor->Trace(target_);
}

}  // namespace blink
