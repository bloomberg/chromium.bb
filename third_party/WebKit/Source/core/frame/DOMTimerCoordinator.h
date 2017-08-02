// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMTimerCoordinator_h
#define DOMTimerCoordinator_h

#include <memory>
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class DOMTimer;
class ExecutionContext;
class ScheduledAction;
class WebTaskRunner;

// Maintains a set of DOMTimers for a given page or
// worker. DOMTimerCoordinator assigns IDs to timers; these IDs are
// the ones returned to web authors from setTimeout or setInterval. It
// also tracks recursive creation or iterative scheduling of timers,
// which is used as a signal for throttling repetitive timers.
class DOMTimerCoordinator {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(DOMTimerCoordinator);

 public:
  explicit DOMTimerCoordinator(RefPtr<WebTaskRunner>);

  // Creates and installs a new timer. Returns the assigned ID.
  int InstallNewTimeout(ExecutionContext*,
                        ScheduledAction*,
                        int timeout,
                        bool single_shot);

  // Removes and disposes the timer with the specified ID, if any. This may
  // destroy the timer.
  DOMTimer* RemoveTimeoutByID(int id);

  // Timers created during the execution of other timers, and
  // repeating timers, are throttled. Timer nesting level tracks the
  // number of linked timers or repetitions of a timer. See
  // https://html.spec.whatwg.org/#timers
  int TimerNestingLevel() { return timer_nesting_level_; }

  // Sets the timer nesting level. Set when a timer executes so that
  // any timers created while the timer is executing will incur a
  // deeper timer nesting level, see DOMTimer::DOMTimer.
  void SetTimerNestingLevel(int level) { timer_nesting_level_ = level; }

  void SetTimerTaskRunner(RefPtr<WebTaskRunner>);

  RefPtr<WebTaskRunner> TimerTaskRunner() const { return timer_task_runner_; }

  DECLARE_TRACE();  // Oilpan.

 private:
  int NextID();

  using TimeoutMap = HeapHashMap<int, Member<DOMTimer>>;
  TimeoutMap timers_;

  int circular_sequential_id_;
  int timer_nesting_level_;
  RefPtr<WebTaskRunner> timer_task_runner_;
};

}  // namespace blink

#endif  // DOMTimerCoordinator_h
