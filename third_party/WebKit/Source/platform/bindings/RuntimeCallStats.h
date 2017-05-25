// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RuntimeCallStats_h
#define RuntimeCallStats_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Time.h"

namespace blink {

// A simple counter used to track total execution count & time for a particular
// function/scope.
class PLATFORM_EXPORT RuntimeCallCounter {
 public:
  RuntimeCallCounter(const char* name) : count_(0), name_(name) {}

  void IncrementAndAddTime(TimeDelta time) {
    count_++;
    time_ += time;
  }

  uint64_t GetCount() const { return count_; }
  TimeDelta GetTime() const { return time_; }
  const char* GetName() const { return name_; }

 private:
  uint64_t count_;
  TimeDelta time_;
  const char* name_;
};

// Used to track elapsed time for a counter.
// NOTE: Do not use this class directly to track execution times, instead use it
// with RuntimeCallStats::Enter/Leave.
class PLATFORM_EXPORT RuntimeCallTimer {
 public:
  RuntimeCallTimer() = default;
  ~RuntimeCallTimer() { DCHECK(!IsRunning()); };

  // Starts recording time for <counter>, and pauses <parent> (if non-null).
  void Start(RuntimeCallCounter*, RuntimeCallTimer* parent);

  // Stops recording time for the counter passed in Start(), and also updates
  // elapsed time and increments the count stored by the counter. It also
  // resumes the parent timer passed in Start() (if any).
  RuntimeCallTimer* Stop();

  // Resets the timer. Call this before reusing a timer.
  void Reset() {
    start_ticks_ = TimeTicks();
    elapsed_time_ = TimeDelta();
  }

 private:
  void Pause(TimeTicks now) {
    DCHECK(IsRunning());
    elapsed_time_ += (now - start_ticks_);
    start_ticks_ = TimeTicks();
  }

  void Resume(TimeTicks now) {
    DCHECK(!IsRunning());
    start_ticks_ = now;
  }

  bool IsRunning() { return start_ticks_ != TimeTicks(); }

  RuntimeCallCounter* counter_;
  RuntimeCallTimer* parent_;
  TimeTicks start_ticks_;
  TimeDelta elapsed_time_;
};

// Maintains a stack of timers and provides functions to manage recording scopes
// by pausing and resuming timers in the chain when entering and leaving a
// scope.
class PLATFORM_EXPORT RuntimeCallStats {
 public:
  // Enters a new recording scope by pausing the currently running timer that
  // was started by the current instance, and starting <timer>.
  void Enter(RuntimeCallTimer* timer, RuntimeCallCounter* counter) {
    timer->Start(counter, current_timer_);
    current_timer_ = timer;
  }

  // Exits the current recording scope, by stopping <timer> (and updating the
  // counter associated with <timer>) and resuming the timer that was paused
  // before entering the current scope.
  void Leave(RuntimeCallTimer* timer) {
    DCHECK_EQ(timer, current_timer_);
    current_timer_ = timer->Stop();
  }

 private:
  RuntimeCallTimer* current_timer_ = nullptr;
};

// A utility class that creates a RuntimeCallTimer and uses it with
// RuntimeCallStats to measure execution time of a C++ scope.
class PLATFORM_EXPORT RuntimeCallTimerScope {
  STATIC_ONLY(RuntimeCallTimerScope);

 public:
  RuntimeCallTimerScope(RuntimeCallStats* stats, RuntimeCallCounter* counter)
      : call_stats_(stats) {
    call_stats_->Enter(&timer_, counter);
  }
  ~RuntimeCallTimerScope() { call_stats_->Leave(&timer_); }

 private:
  RuntimeCallStats* call_stats_;
  RuntimeCallTimer timer_;
};

}  // namespace blink

#endif  // RuntimeCallStats_h
