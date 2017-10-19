/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#include "core/frame/DOMTimer.h"

#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/probe/CoreProbes.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/CurrentTime.h"

namespace blink {

static const int kMaxIntervalForUserGestureForwarding =
    1000;  // One second matches Gecko.
static const int kMaxTimerNestingLevel = 5;
static const double kOneMillisecond = 0.001;
// Chromium uses a minimum timer interval of 4ms. We'd like to go
// lower; however, there are poorly coded websites out there which do
// create CPU-spinning loops.  Using 4ms prevents the CPU from
// spinning too busily and provides a balance between CPU spinning and
// the smallest possible interval timer.
static const double kMinimumInterval = 0.004;

static inline bool ShouldForwardUserGesture(int interval, int nesting_level) {
  return UserGestureIndicator::ProcessingUserGestureThreadSafe() &&
         interval <= kMaxIntervalForUserGestureForwarding &&
         nesting_level ==
             1;  // Gestures should not be forwarded to nested timers.
}

int DOMTimer::Install(ExecutionContext* context,
                      ScheduledAction* action,
                      int timeout,
                      bool single_shot) {
  int timeout_id = context->Timers()->InstallNewTimeout(context, action,
                                                        timeout, single_shot);
  return timeout_id;
}

void DOMTimer::RemoveByID(ExecutionContext* context, int timeout_id) {
  DOMTimer* timer = context->Timers()->RemoveTimeoutByID(timeout_id);
  TRACE_EVENT_INSTANT1("devtools.timeline", "TimerRemove",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorTimerRemoveEvent::Data(context, timeout_id));
  // Eagerly unregister as ExecutionContext observer.
  if (timer)
    timer->ClearContext();
}

DOMTimer::DOMTimer(ExecutionContext* context,
                   ScheduledAction* action,
                   int interval,
                   bool single_shot,
                   int timeout_id)
    : SuspendableTimer(context, TaskType::kJavascriptTimer),
      timeout_id_(timeout_id),
      nesting_level_(context->Timers()->TimerNestingLevel() + 1),
      action_(action) {
  DCHECK_GT(timeout_id, 0);
  if (ShouldForwardUserGesture(interval, nesting_level_)) {
    // Thread safe because shouldForwardUserGesture will only return true if
    // execution is on the the main thread.
    user_gesture_token_ = UserGestureIndicator::CurrentToken();
  }

  double interval_milliseconds =
      std::max(kOneMillisecond, interval * kOneMillisecond);
  if (interval_milliseconds < kMinimumInterval &&
      nesting_level_ >= kMaxTimerNestingLevel)
    interval_milliseconds = kMinimumInterval;
  if (single_shot)
    StartOneShot(interval_milliseconds, BLINK_FROM_HERE);
  else
    StartRepeating(interval_milliseconds, BLINK_FROM_HERE);

  SuspendIfNeeded();
  TRACE_EVENT_INSTANT1("devtools.timeline", "TimerInstall",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorTimerInstallEvent::Data(context, timeout_id,
                                                        interval, single_shot));
  probe::AsyncTaskScheduledBreakable(
      context, single_shot ? "setTimeout" : "setInterval", this);
}

DOMTimer::~DOMTimer() {
  if (action_)
    action_->Dispose();
}

void DOMTimer::Stop() {
  probe::AsyncTaskCanceledBreakable(
      GetExecutionContext(),
      RepeatInterval() ? "clearInterval" : "clearTimeout", this);

  user_gesture_token_ = nullptr;
  // Need to release JS objects potentially protected by ScheduledAction
  // because they can form circular references back to the ExecutionContext
  // which will cause a memory leak.
  if (action_)
    action_->Dispose();
  action_ = nullptr;
  SuspendableTimer::Stop();
}

void DOMTimer::ContextDestroyed(ExecutionContext*) {
  Stop();
}

void DOMTimer::Fired() {
  ExecutionContext* context = GetExecutionContext();
  DCHECK(context);
  context->Timers()->SetTimerNestingLevel(nesting_level_);
  DCHECK(!context->IsContextSuspended());
  // Only the first execution of a multi-shot timer should get an affirmative
  // user gesture indicator.
  UserGestureIndicator gesture_indicator(std::move(user_gesture_token_));

  TRACE_EVENT1("devtools.timeline", "TimerFire", "data",
               InspectorTimerFireEvent::Data(context, timeout_id_));
  probe::UserCallback probe(context,
                            RepeatInterval() ? "setInterval" : "setTimeout",
                            AtomicString(), true);
  probe::AsyncTask async_task(context, this,
                              RepeatInterval() ? "fired" : nullptr);

  // Simple case for non-one-shot timers.
  if (IsActive()) {
    if (RepeatInterval() && RepeatInterval() < kMinimumInterval) {
      nesting_level_++;
      if (nesting_level_ >= kMaxTimerNestingLevel)
        AugmentRepeatInterval(kMinimumInterval - RepeatInterval());
    }

    // No access to member variables after this point, it can delete the timer.
    action_->Execute(context);

    context->Timers()->SetTimerNestingLevel(0);

    return;
  }

  // Unregister the timer from ExecutionContext before executing the action
  // for one-shot timers.
  ScheduledAction* action = action_.Release();
  context->Timers()->RemoveTimeoutByID(timeout_id_);

  action->Execute(context);

  // ExecutionContext might be already gone when we executed action->execute().
  ExecutionContext* execution_context = GetExecutionContext();
  if (!execution_context)
    return;

  execution_context->Timers()->SetTimerNestingLevel(0);
  // Eagerly unregister as ExecutionContext observer.
  ClearContext();
  // Eagerly clear out |action|'s resources.
  action->Dispose();
}

RefPtr<WebTaskRunner> DOMTimer::TimerTaskRunner() const {
  return GetExecutionContext()->Timers()->TimerTaskRunner();
}

void DOMTimer::Trace(blink::Visitor* visitor) {
  visitor->Trace(action_);
  SuspendableTimer::Trace(visitor);
}

}  // namespace blink
