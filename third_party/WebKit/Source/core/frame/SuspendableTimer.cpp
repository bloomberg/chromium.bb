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

#include "core/frame/SuspendableTimer.h"

#include "core/dom/TaskRunnerHelper.h"

namespace blink {

namespace {
// The lowest value returned by TimerBase::nextUnalignedFireInterval is 0.0
const double kNextFireIntervalInvalid = -1.0;
}

SuspendableTimer::SuspendableTimer(ExecutionContext* context,
                                   TaskType task_type)
    : TimerBase(TaskRunnerHelper::Get(task_type, context)),
      SuspendableObject(context),
      next_fire_interval_(kNextFireIntervalInvalid),
      repeat_interval_(0) {
  DCHECK(context);
}

SuspendableTimer::~SuspendableTimer() {}

void SuspendableTimer::Stop() {
  next_fire_interval_ = kNextFireIntervalInvalid;
  TimerBase::Stop();
}

void SuspendableTimer::ContextDestroyed(ExecutionContext*) {
  Stop();
}

void SuspendableTimer::Suspend() {
#if DCHECK_IS_ON()
  DCHECK(!suspended_);
  suspended_ = true;
#endif
  if (IsActive()) {
    next_fire_interval_ = NextFireInterval();
    ASSERT(next_fire_interval_ >= 0.0);
    repeat_interval_ = RepeatInterval();
    TimerBase::Stop();
  }
}

void SuspendableTimer::Resume() {
#if DCHECK_IS_ON()
  DCHECK(suspended_);
  suspended_ = false;
#endif
  if (next_fire_interval_ >= 0.0) {
    // start() was called before, therefore location() is already set.
    // m_nextFireInterval is only set in suspend() if the Timer was active.
    Start(next_fire_interval_, repeat_interval_, GetLocation());
    next_fire_interval_ = kNextFireIntervalInvalid;
  }
}

}  // namespace blink
