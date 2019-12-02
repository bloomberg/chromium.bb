/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TIMER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TIMER_H_

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/sanitizers.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

// Time intervals are all in seconds.

class PLATFORM_EXPORT TimerBase {
 public:
  explicit TimerBase(scoped_refptr<base::SingleThreadTaskRunner>);
  virtual ~TimerBase();

  void Start(base::TimeDelta next_fire_interval,
             base::TimeDelta repeat_interval,
             const base::Location&);

  void StartRepeating(base::TimeDelta repeat_interval,
                      const base::Location& caller) {
    Start(repeat_interval, repeat_interval, caller);
  }

  void StartOneShot(base::TimeDelta interval, const base::Location& caller) {
    Start(interval, base::TimeDelta(), caller);
  }

  // Timer cancellation is fast enough that you shouldn't have to worry
  // about it unless you're canceling tens of thousands of tasks.
  virtual void Stop();
  bool IsActive() const;
  const base::Location& GetLocation() const { return location_; }

  base::TimeDelta NextFireInterval() const;
  base::TimeDelta RepeatInterval() const { return repeat_interval_; }

  void AugmentRepeatInterval(base::TimeDelta delta) {
    base::TimeTicks now = TimerCurrentTimeTicks();
    SetNextFireTime(now,
                    std::max(next_fire_time_ - now + delta, base::TimeDelta()));
    repeat_interval_ += delta;
  }

  void MoveToNewTaskRunner(scoped_refptr<base::SingleThreadTaskRunner>);

  struct PLATFORM_EXPORT Comparator {
    bool operator()(const TimerBase* a, const TimerBase* b) const;
  };

 private:
  virtual void Fired() = 0;

  NO_SANITIZE_ADDRESS
  virtual bool CanFire() const { return true; }

  base::TimeTicks TimerCurrentTimeTicks() const;

  void SetNextFireTime(base::TimeTicks now, base::TimeDelta delay);

  void RunInternal();

  base::TimeTicks next_fire_time_;   // 0 if inactive
  base::TimeDelta repeat_interval_;  // 0 if not repeating
  base::Location location_;
  scoped_refptr<base::SingleThreadTaskRunner> web_task_runner_;

#if DCHECK_IS_ON()
  base::PlatformThreadId thread_;
#endif
  base::WeakPtrFactory<TimerBase> weak_ptr_factory_{this};

  friend class ThreadTimers;
  friend class TimerHeapLessThanFunction;
  friend class TimerHeapReference;

  DISALLOW_COPY_AND_ASSIGN(TimerBase);
};

template <typename TimerFiredClass,
          bool = WTF::IsGarbageCollectedTypeInternal<TimerFiredClass>::value>
class TaskRunnerTimer;

template <typename TimerFiredClass>
class TaskRunnerTimer<TimerFiredClass, false> : public TimerBase {
 public:
  using TimerFiredFunction = void (TimerFiredClass::*)(TimerBase*);

  TaskRunnerTimer(scoped_refptr<base::SingleThreadTaskRunner> web_task_runner,
                  TimerFiredClass* o,
                  TimerFiredFunction f)
      : TimerBase(std::move(web_task_runner)), object_(o), function_(f) {}

  ~TaskRunnerTimer() override = default;

 protected:
  void Fired() override { (object_->*function_)(this); }

  NO_SANITIZE_ADDRESS
  bool CanFire() const override { return true; }

 private:
  TimerFiredClass* object_;
  TimerFiredFunction function_;
};

template <typename TimerFiredClass>
class TaskRunnerTimer<TimerFiredClass, true> : public TimerBase {
 public:
  using TimerFiredFunction = void (TimerFiredClass::*)(TimerBase*);

  TaskRunnerTimer(scoped_refptr<base::SingleThreadTaskRunner> web_task_runner,
                  TimerFiredClass* o,
                  TimerFiredFunction f)
      : TimerBase(std::move(web_task_runner)), object_(o), function_(f) {}

  ~TaskRunnerTimer() override = default;

 protected:
  void Fired() override { (object_->*function_)(this); }

  NO_SANITIZE_ADDRESS
  bool CanFire() const override { return object_.IsClearedUnsafe(); }

 private:
  GC_PLUGIN_IGNORE("363031")
  WeakPersistent<TimerFiredClass> object_;
  TimerFiredFunction function_;
};

NO_SANITIZE_ADDRESS
inline bool TimerBase::IsActive() const {
#if DCHECK_IS_ON()
  DCHECK_EQ(thread_, CurrentThread());
#endif
  return weak_ptr_factory_.HasWeakPtrs();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TIMER_H_
