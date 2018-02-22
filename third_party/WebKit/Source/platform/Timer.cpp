/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "platform/Timer.h"

#include <limits.h>
#include <math.h>
#include <algorithm>
#include <limits>
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/wtf/AddressSanitizer.h"
#include "platform/wtf/Atomics.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Time.h"
#include "public/platform/Platform.h"

namespace blink {

TimerBase::TimerBase(
    scoped_refptr<base::SingleThreadTaskRunner> web_task_runner)
    : web_task_runner_(std::move(web_task_runner)),
#if DCHECK_IS_ON()
      thread_(CurrentThread()),
#endif
      weak_ptr_factory_(this) {
}

TimerBase::~TimerBase() {
  Stop();
}

void TimerBase::Start(TimeDelta next_fire_interval,
                      TimeDelta repeat_interval,
                      const base::Location& caller) {
#if DCHECK_IS_ON()
  DCHECK_EQ(thread_, CurrentThread());
#endif

  location_ = caller;
  repeat_interval_ = repeat_interval;
  SetNextFireTime(TimerCurrentTimeTicks(), next_fire_interval);
}

void TimerBase::Stop() {
#if DCHECK_IS_ON()
  DCHECK_EQ(thread_, CurrentThread());
#endif

  repeat_interval_ = TimeDelta();
  next_fire_time_ = TimeTicks();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

TimeDelta TimerBase::NextFireIntervalDelta() const {
  DCHECK(IsActive());
  TimeTicks current = TimerCurrentTimeTicks();
  if (next_fire_time_ < current)
    return TimeDelta();
  return next_fire_time_ - current;
}

void TimerBase::MoveToNewTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
#if DCHECK_IS_ON()
  DCHECK_EQ(thread_, CurrentThread());
  DCHECK(task_runner->RunsTasksInCurrentSequence());
#endif
  // If the underlying task runner stays the same, ignore it.
  if (web_task_runner_ == task_runner) {
    return;
  }

  bool active = IsActive();
  weak_ptr_factory_.InvalidateWeakPtrs();
  web_task_runner_ = std::move(task_runner);

  if (!active)
    return;

  TimeTicks now = TimerCurrentTimeTicks();
  TimeTicks next_fire_time = std::max(next_fire_time_, now);
  next_fire_time_ = TimeTicks();

  SetNextFireTime(now, next_fire_time - now);
}

scoped_refptr<base::SingleThreadTaskRunner> TimerBase::TimerTaskRunner() const {
  return web_task_runner_;
}

void TimerBase::SetNextFireTime(TimeTicks now, TimeDelta delay) {
#if DCHECK_IS_ON()
  DCHECK_EQ(thread_, CurrentThread());
#endif

  TimeTicks new_time = now + delay;

  if (next_fire_time_ != new_time) {
    next_fire_time_ = new_time;

    // Cancel any previously posted task.
    weak_ptr_factory_.InvalidateWeakPtrs();

    TimerTaskRunner()->PostDelayedTask(
        location_,
        WTF::Bind(&TimerBase::RunInternal, weak_ptr_factory_.GetWeakPtr()),
        delay);
  }
}

NO_SANITIZE_ADDRESS
void TimerBase::RunInternal() {
  if (!CanFire())
    return;

  weak_ptr_factory_.InvalidateWeakPtrs();

  TRACE_EVENT0("blink", "TimerBase::run");
#if DCHECK_IS_ON()
  DCHECK_EQ(thread_, CurrentThread())
      << "Timer posted by " << location_.function_name() << " "
      << location_.file_name() << " was run on a different thread";
#endif

  if (!repeat_interval_.is_zero()) {
    TimeTicks now = TimerCurrentTimeTicks();
    // This computation should be drift free, and it will cope if we miss a
    // beat, which can easily happen if the thread is busy.  It will also cope
    // if we get called slightly before m_unalignedNextFireTime, which can
    // happen due to lack of timer precision.
    TimeDelta interval_to_next_fire_time =
        repeat_interval_ - (now - next_fire_time_) % repeat_interval_;
    SetNextFireTime(now, interval_to_next_fire_time);
  } else {
    next_fire_time_ = TimeTicks();
  }
  Fired();
}

bool TimerBase::Comparator::operator()(const TimerBase* a,
                                       const TimerBase* b) const {
  return a->next_fire_time_ < b->next_fire_time_;
}

// static
TimeTicks TimerBase::TimerCurrentTimeTicks() const {
  return WTF::TimeTicks(Platform::Current()
                            ->CurrentThread()
                            ->Scheduler()
                            ->MonotonicallyIncreasingVirtualTime());
}

}  // namespace blink
