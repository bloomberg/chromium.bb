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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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

#include "third_party/blink/renderer/core/frame/pausable_timer.h"

#include "third_party/blink/public/platform/task_type.h"

namespace blink {

namespace {
// The lowest value returned by TimerBase::nextUnalignedFireInterval is 0.0
const double kNextFireIntervalInvalid = -1.0;
}  // namespace

PausableTimer::PausableTimer(ExecutionContext* context, TaskType task_type)
    : TimerBase(context->GetTaskRunner(task_type)),
      PausableObject(context),
      next_fire_interval_(kNextFireIntervalInvalid),
      repeat_interval_(0) {
  DCHECK(context);
}

PausableTimer::~PausableTimer() = default;

void PausableTimer::Stop() {
  next_fire_interval_ = kNextFireIntervalInvalid;
  TimerBase::Stop();
}

void PausableTimer::ContextDestroyed(ExecutionContext*) {
  Stop();
}

void PausableTimer::Pause() {
#if DCHECK_IS_ON()
  DCHECK(!paused_);
  paused_ = true;
#endif
  if (IsActive()) {
    next_fire_interval_ = NextFireInterval();
    DCHECK_GE(next_fire_interval_, 0.0);
    repeat_interval_ = RepeatInterval();
    TimerBase::Stop();
  }
}

void PausableTimer::Unpause() {
#if DCHECK_IS_ON()
  DCHECK(paused_);
  paused_ = false;
#endif
  if (next_fire_interval_ >= 0.0) {
    // start() was called before, therefore location() is already set.
    // m_nextFireInterval is only set in suspend() if the Timer was active.
    Start(next_fire_interval_, repeat_interval_, GetLocation());
    next_fire_interval_ = kNextFireIntervalInvalid;
  }
}

}  // namespace blink
