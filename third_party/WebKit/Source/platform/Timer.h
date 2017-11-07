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

#ifndef Timer_h
#define Timer_h

#include "base/time/time.h"
#include "platform/PlatformExport.h"
#include "platform/WebTaskRunner.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/AddressSanitizer.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/Threading.h"
#include "platform/wtf/WeakPtr.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

// Time intervals are all in seconds.

class PLATFORM_EXPORT TimerBase {
  WTF_MAKE_NONCOPYABLE(TimerBase);

 public:
  explicit TimerBase(scoped_refptr<WebTaskRunner>);
  virtual ~TimerBase();

  void Start(double next_fire_interval,
             double repeat_interval,
             const WebTraceLocation&);

  void StartRepeating(double repeat_interval, const WebTraceLocation& caller) {
    Start(repeat_interval, repeat_interval, caller);
  }
  void StartRepeating(base::TimeDelta repeat_interval,
                      const WebTraceLocation& caller) {
    StartRepeating(repeat_interval.InSecondsF(), caller);
  }
  void StartOneShot(double interval, const WebTraceLocation& caller) {
    Start(interval, 0, caller);
  }
  void StartOneShot(base::TimeDelta interval, const WebTraceLocation& caller) {
    StartOneShot(interval.InSecondsF(), caller);
  }

  // Timer cancellation is fast enough that you shouldn't have to worry
  // about it unless you're canceling tens of thousands of tasks.
  virtual void Stop();
  bool IsActive() const;
  const WebTraceLocation& GetLocation() const { return location_; }

  double NextFireInterval() const;
  double RepeatInterval() const { return repeat_interval_; }

  void AugmentRepeatInterval(double delta) {
    double now = TimerMonotonicallyIncreasingTime();
    SetNextFireTime(now, std::max(next_fire_time_ - now + delta, 0.0));
    repeat_interval_ += delta;
  }

  void MoveToNewTaskRunner(scoped_refptr<WebTaskRunner>);

  struct PLATFORM_EXPORT Comparator {
    bool operator()(const TimerBase* a, const TimerBase* b) const;
  };

 protected:
  static scoped_refptr<WebTaskRunner> GetTimerTaskRunner();

 private:
  virtual void Fired() = 0;

  virtual scoped_refptr<WebTaskRunner> TimerTaskRunner() const;

  NO_SANITIZE_ADDRESS
  virtual bool CanFire() const { return true; }

  double TimerMonotonicallyIncreasingTime() const;

  void SetNextFireTime(double now, double delay);

  void RunInternal();

  double next_fire_time_;   // 0 if inactive
  double repeat_interval_;  // 0 if not repeating
  WebTraceLocation location_;
  scoped_refptr<WebTaskRunner> web_task_runner_;

#if DCHECK_IS_ON()
  ThreadIdentifier thread_;
#endif
  WTF::WeakPtrFactory<TimerBase> weak_ptr_factory_;

  friend class ThreadTimers;
  friend class TimerHeapLessThanFunction;
  friend class TimerHeapReference;
};

template <typename T, bool = IsGarbageCollectedType<T>::value>
class TimerIsObjectAliveTrait {
 public:
  static bool IsHeapObjectAlive(T*) { return true; }
};

template <typename T>
class TimerIsObjectAliveTrait<T, true> {
 public:
  static bool IsHeapObjectAlive(T* object_pointer) {
    return !ThreadHeap::WillObjectBeLazilySwept(object_pointer);
  }
};

template <typename TimerFiredClass>
class TaskRunnerTimer : public TimerBase {
 public:
  using TimerFiredFunction = void (TimerFiredClass::*)(TimerBase*);

  TaskRunnerTimer(scoped_refptr<WebTaskRunner> web_task_runner,
                  TimerFiredClass* o,
                  TimerFiredFunction f)
      : TimerBase(std::move(web_task_runner)), object_(o), function_(f) {}

  ~TaskRunnerTimer() override {}

 protected:
  void Fired() override { (object_->*function_)(this); }

  NO_SANITIZE_ADDRESS
  bool CanFire() const override {
    // Oilpan: if a timer fires while Oilpan heaps are being lazily
    // swept, it is not safe to proceed if the object is about to
    // be swept (and this timer will be stopped while doing so.)
    return TimerIsObjectAliveTrait<TimerFiredClass>::IsHeapObjectAlive(object_);
  }

 private:
  // FIXME: Oilpan: TimerBase should be moved to the heap and m_object should be
  // traced.  This raw pointer is safe as long as Timer<X> is held by the X
  // itself (That's the case
  // in the current code base).
  GC_PLUGIN_IGNORE("363031")
  TimerFiredClass* object_;
  TimerFiredFunction function_;
};

// TODO(dcheng): Consider removing this overload once all timers are using the
// appropriate task runner. https://crbug.com/624694
template <typename TimerFiredClass>
class Timer : public TaskRunnerTimer<TimerFiredClass> {
 public:
  using TimerFiredFunction =
      typename TaskRunnerTimer<TimerFiredClass>::TimerFiredFunction;

  ~Timer() override {}

  Timer(TimerFiredClass* timer_fired_class,
        TimerFiredFunction timer_fired_function)
      : TaskRunnerTimer<TimerFiredClass>(TimerBase::GetTimerTaskRunner(),
                                         timer_fired_class,
                                         timer_fired_function) {}
};

NO_SANITIZE_ADDRESS
inline bool TimerBase::IsActive() const {
#if DCHECK_IS_ON()
  DCHECK_EQ(thread_, CurrentThread());
#endif
  return weak_ptr_factory_.HasWeakPtrs();
}

}  // namespace blink

#endif  // Timer_h
