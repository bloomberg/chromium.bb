// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the Blink version of RuntimeCallStats which is implemented
// by V8 in //v8/src/counters.h

#ifndef RuntimeCallStats_h
#define RuntimeCallStats_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Time.h"
#include "platform/wtf/text/WTFString.h"
#include "v8/include/v8.h"

namespace blink {

// A simple counter used to track total execution count & time for a particular
// function/scope.
class PLATFORM_EXPORT RuntimeCallCounter {
 public:
  explicit RuntimeCallCounter(const char* name) : count_(0), name_(name) {}

  void IncrementAndAddTime(TimeDelta time) {
    count_++;
    time_ += time;
  }

  uint64_t GetCount() const { return count_; }
  TimeDelta GetTime() const { return time_; }
  const char* GetName() const { return name_; }

  void Reset() {
    time_ = TimeDelta();
    count_ = 0;
  }

 private:
  RuntimeCallCounter() {}

  uint64_t count_;
  TimeDelta time_;
  const char* name_;

  friend class RuntimeCallStats;
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
  RuntimeCallStats();
  // Get RuntimeCallStats object associated with the given isolate.
  static RuntimeCallStats* From(v8::Isolate*);

// The following 3 macros are used to define counters that are used in the
// bindings layer to measure call stats for IDL interface methods and
// attributes. Also see documentation for [RuntimeCallStatsCounter] in
// bindings/IDLExtendedAttributes.md.

// Use this to define a counter for IDL interface methods.
// [RuntimeCallStatsCounter=MethodCounter] void method() =>
// BINDINGS_METHOD(V, MethodCounter)
#define BINDINGS_METHOD(V, counter) V(counter)

// Use this to define a counter for IDL readonly attributes.
// [RuntimeCallStatsCounter=AttributeCounter] readonly attribute boolean attr =>
// BINDINGS_READ_ONLY_ATTRIBUTE(V, AttributeCounter)
#define BINDINGS_READ_ONLY_ATTRIBUTE(V, counter) V(counter##_Getter)

// Use this to define counters for IDL attributes (defines a counter each for
// getter and setter).
// [RuntimeCallStats=AttributeCounter] attribute long attr
// => BINDINGS_ATTRIBUTE(V, AttributeCounter)
#define BINDINGS_ATTRIBUTE(V, counter) \
  V(counter##_Getter)                  \
  V(counter##_Setter)

// Counters
#define FOR_EACH_COUNTER(V)                                             \
  V(UpdateStyle)                                                        \
  V(UpdateLayout)                                                       \
  V(CollectGarbage)                                                     \
  V(PerformIdleLazySweep)                                               \
  V(UpdateLayerPositionsAfterLayout)                                    \
  V(PaintContents)                                                      \
  V(ProcessStyleSheet)                                                  \
  V(TestCounter1)                                                       \
  V(TestCounter2)                                                       \
  BINDINGS_METHOD(V, BindingsMethodTestCounter)                         \
  BINDINGS_READ_ONLY_ATTRIBUTE(V, BindingsReadOnlyAttributeTestCounter) \
  BINDINGS_ATTRIBUTE(V, BindingsAttributeTestCounter)

  enum class CounterId : uint16_t {
#define ADD_ENUM_VALUE(counter) k##counter,
    FOR_EACH_COUNTER(ADD_ENUM_VALUE)
#undef ADD_ENUM_VALUE
        kNumberOfCounters
  };

  // Enters a new recording scope by pausing the currently running timer that
  // was started by the current instance, and starting <timer>.
  virtual void Enter(RuntimeCallTimer*, CounterId);

  // Exits the current recording scope, by stopping <timer> (and updating the
  // counter associated with <timer>) and resuming the timer that was paused
  // before entering the current scope.
  virtual void Leave(RuntimeCallTimer*);

  // Reset all the counters.
  void Reset();

  RuntimeCallCounter* GetCounter(CounterId id) {
    return &(counters_[static_cast<uint16_t>(id)]);
  }

  String ToString() const;

  // Use these two functions to stub out RuntimeCallStats in tests.
  static void SetRuntimeCallStatsForTesting();
  static void ClearRuntimeCallStatsForTesting();

 private:
  RuntimeCallTimer* current_timer_ = nullptr;
  RuntimeCallCounter counters_[static_cast<int>(CounterId::kNumberOfCounters)];
  static const int number_of_counters_ =
      static_cast<int>(CounterId::kNumberOfCounters);
};

// A utility class that creates a RuntimeCallTimer and uses it with
// RuntimeCallStats to measure execution time of a C++ scope.
class PLATFORM_EXPORT RuntimeCallTimerScope {
 public:
  RuntimeCallTimerScope(RuntimeCallStats* stats,
                        RuntimeCallStats::CounterId counter)
      : call_stats_(stats) {
    call_stats_->Enter(&timer_, counter);
  }
  ~RuntimeCallTimerScope() { call_stats_->Leave(&timer_); }

 private:
  RuntimeCallStats* call_stats_;
  RuntimeCallTimer timer_;
};

// Used to stub out RuntimeCallStats for testing.
class RuntimeCallStatsForTesting : public RuntimeCallStats {
  void Enter(RuntimeCallTimer*, RuntimeCallStats::CounterId) override {}
  void Leave(RuntimeCallTimer*) override {}
};

}  // namespace blink

#endif  // RuntimeCallStats_h
