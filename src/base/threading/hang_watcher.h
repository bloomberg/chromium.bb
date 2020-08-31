// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_HANG_WATCHER_H_
#define BASE_THREADING_HANG_WATCHER_H_

#include <atomic>
#include <memory>
#include <vector>

#include "base/atomicops.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_local.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"

namespace base {
class HangWatchScope;
namespace internal {
class HangWatchState;
}  // namespace internal
}  // namespace base

namespace base {

// Instantiate a HangWatchScope in a scope to register to be
// watched for hangs of more than |timeout| by the HangWatcher.
//
// Example usage:
//
//  void FooBar(){
//    HangWatchScope scope(base::TimeDelta::FromSeconds(5));
//    DoWork();
//  }
//
// If DoWork() takes more than 5s to run and the HangWatcher
// inspects the thread state before Foobar returns a hang will be
// reported.
//
// HangWatchScopes are typically meant to live on the stack. In some cases it's
// necessary to keep a HangWatchScope instance as a class member but special
// care is required when doing so as a HangWatchScope that stays alive longer
// than intended will generate non-actionable hang reports.
class BASE_EXPORT HangWatchScope {
 public:
  // A good default value needs to be large enough to represent a significant
  // hang and avoid noise while being small enough to not exclude too many
  // hangs. The nature of the work that gets executed on the thread is also
  // important. We can be much stricter when monitoring a UI thread compared tp
  // a ThreadPool thread for example.
  static const base::TimeDelta kDefaultHangWatchTime;

  // Constructing/destructing thread must be the same thread.
  explicit HangWatchScope(TimeDelta timeout);
  ~HangWatchScope();

  HangWatchScope(const HangWatchScope&) = delete;
  HangWatchScope& operator=(const HangWatchScope&) = delete;

 private:
  // This object should always be constructed and destructed on the same thread.
  THREAD_CHECKER(thread_checker_);

  // The deadline set by the previous HangWatchScope created on this thread.
  // Stored so it can be restored when this HangWatchScope is destroyed.
  TimeTicks previous_deadline_;

#if DCHECK_IS_ON()
  // The previous HangWatchScope created on this thread.
  HangWatchScope* previous_scope_;
#endif
};

// Monitors registered threads for hangs by inspecting their associated
// HangWatchStates for deadline overruns. This happens at a regular interval on
// a separate thread. Only one instance of HangWatcher can exist at a time
// within a single process. This instance must outlive all monitored threads.
class BASE_EXPORT HangWatcher : public DelegateSimpleThread::Delegate {
 public:
  static const base::Feature kEnableHangWatcher;

  // The first invocation of the constructor will set the global instance
  // accessible through GetInstance(). This means that only one instance can
  // exist at a time.
  HangWatcher();

  // Clears the global instance for the class.
  ~HangWatcher() override;

  HangWatcher(const HangWatcher&) = delete;
  HangWatcher& operator=(const HangWatcher&) = delete;

  // Returns a non-owning pointer to the global HangWatcher instance.
  static HangWatcher* GetInstance();

  // Sets up the calling thread to be monitored for threads. Returns a
  // ScopedClosureRunner that unregisters the thread. This closure has to be
  // called from the registered thread before it's joined.
  ScopedClosureRunner RegisterThread()
      LOCKS_EXCLUDED(watch_state_lock_) WARN_UNUSED_RESULT;

  // Choose a closure to be run at the end of each call to Monitor(). Use only
  // for testing. Reentering the HangWatcher in the closure must be done with
  // care. It should only be done through certain testing functions because
  // deadlocks are possible.
  void SetAfterMonitorClosureForTesting(base::RepeatingClosure closure);

  // Choose a closure to be run instead of recording the hang. Used to test
  // that certain conditions hold true at the time of recording. Use only
  // for testing. Reentering the HangWatcher in the closure must be done with
  // care. It should only be done through certain testing functions because
  // deadlocks are possible.
  void SetOnHangClosureForTesting(base::RepeatingClosure closure);

  // Set a monitoring period other than the default. Use only for
  // testing.
  void SetMonitoringPeriodForTesting(base::TimeDelta period);

  // Choose a callback to invoke right after waiting to monitor in Wait(). Use
  // only for testing.
  void SetAfterWaitCallbackForTesting(
      RepeatingCallback<void(TimeTicks)> callback);

  // Force the monitoring loop to resume and evaluate whether to continue.
  // This can trigger a call to Monitor() or not depending on why the
  // HangWatcher thread is sleeping. Use only for testing.
  void SignalMonitorEventForTesting();

  // Call to make sure no more monitoring takes place. The
  // function is thread-safe and can be called at anytime but won't stop
  // monitoring that is currently taking place. Use only for testing.
  void StopMonitoringForTesting();

  // Replace the clock used when calculating time spent
  // sleeping. Use only for testing.
  void SetTickClockForTesting(const base::TickClock* tick_clock);

  // Use to block until the hang is recorded. Allows the caller to halt
  // execution so it does not overshoot the hang watch target and result in a
  // non-actionable stack trace in the crash recorded.
  void BlockIfCaptureInProgress();

 private:
  // Use to assert that functions are called on the monitoring thread.
  THREAD_CHECKER(hang_watcher_thread_checker_);

  // Use to assert that functions are called on the constructing thread.
  THREAD_CHECKER(constructing_thread_checker_);

  // Invoke base::debug::DumpWithoutCrashing() insuring that the stack frame
  // right under it in the trace belongs to HangWatcher for easier attribution.
  NOINLINE static void RecordHang();

  using HangWatchStates =
      std::vector<std::unique_ptr<internal::HangWatchState>>;

  // Used to save a snapshots of the state of hang watching during capture.
  // Only the state of hung threads is retained.
  class BASE_EXPORT WatchStateSnapShot {
   public:
    struct WatchStateCopy {
      base::TimeTicks deadline;
      base::PlatformThreadId thread_id;
    };

    // Construct the snapshot from provided data. |snapshot_time| can be
    // different than now() to be coherent with other operations recently done
    // on |watch_states|. If any deadline in |watch_states| is before
    // |deadline_ignore_threshold|, the snapshot is empty.
    WatchStateSnapShot(const HangWatchStates& watch_states,
                       base::TimeTicks snapshot_time,
                       base::TimeTicks deadline_ignore_threshold);
    WatchStateSnapShot(const WatchStateSnapShot& other);
    ~WatchStateSnapShot();

    // Returns a string that contains the ids of the hung threads separated by a
    // '|'. The size of the string is capped at debug::CrashKeySize::Size256. If
    // no threads are hung returns an empty string.
    std::string PrepareHungThreadListCrashKey() const;

    // Return the highest deadline included in this snapshot.
    base::TimeTicks GetHighestDeadline() const;

   private:
    base::TimeTicks snapshot_time_;
    std::vector<WatchStateCopy> hung_watch_state_copies_;
  };

  // Return a watch state snapshot taken Now() to be inspected in tests.
  // NO_THREAD_SAFETY_ANALYSIS is needed because the analyzer can't figure out
  // that calls to this function done from |on_hang_closure_| are properly
  // locked.
  WatchStateSnapShot GrabWatchStateSnapshotForTesting() const
      NO_THREAD_SAFETY_ANALYSIS;

  // Inspects the state of all registered threads to check if they are hung and
  // invokes the appropriate closure if so.
  void Monitor() LOCKS_EXCLUDED(watch_state_lock_);

  // Record the hang and perform the necessary housekeeping before and after.
  void CaptureHang(base::TimeTicks capture_time)
      EXCLUSIVE_LOCKS_REQUIRED(watch_state_lock_) LOCKS_EXCLUDED(capture_lock_);

  // Call Run() on the HangWatcher thread.
  void Start();

  // Stop all monitoring and join the HangWatcher thread.
  void Stop();

  // Wait until it's time to monitor.
  void Wait();

  // Run the loop that periodically monitors the registered thread at a
  // set time interval.
  void Run() override;

  base::TimeDelta monitor_period_;

  // Indicates whether Run() should return after the next monitoring.
  std::atomic<bool> keep_monitoring_{true};

  // Use to make the HangWatcher thread wake or sleep to schedule the
  // appropriate monitoring frequency.
  WaitableEvent should_monitor_;

  bool IsWatchListEmpty() LOCKS_EXCLUDED(watch_state_lock_);

  // Stops hang watching on the calling thread by removing the entry from the
  // watch list.
  void UnregisterThread() LOCKS_EXCLUDED(watch_state_lock_);

  Lock watch_state_lock_;

  std::vector<std::unique_ptr<internal::HangWatchState>> watch_states_
      GUARDED_BY(watch_state_lock_);

  base::DelegateSimpleThread thread_;

  RepeatingClosure after_monitor_closure_for_testing_;
  RepeatingClosure on_hang_closure_for_testing_;
  RepeatingCallback<void(TimeTicks)> after_wait_callback_;

  base::Lock capture_lock_ ACQUIRED_AFTER(watch_state_lock_);
  std::atomic<bool> capture_in_progress{false};

  const base::TickClock* tick_clock_;

  // The time after which all deadlines in |watch_states_| need to be for a hang
  // to be reported.
  base::TimeTicks deadline_ignore_threshold_;

  FRIEND_TEST_ALL_PREFIXES(HangWatcherTest, NestedScopes);
  FRIEND_TEST_ALL_PREFIXES(HangWatcherSnapshotTest, HungThreadIDs);
};

// Classes here are exposed in the header only for testing. They are not
// intended to be used outside of base.
namespace internal {

// Contains the information necessary for hang watching a specific
// thread. Instances of this class are accessed concurrently by the associated
// thread and the HangWatcher. The HangWatcher owns instances of this
// class and outside of it they are accessed through
// GetHangWatchStateForCurrentThread().
class BASE_EXPORT HangWatchState {
 public:
  HangWatchState();
  ~HangWatchState();

  HangWatchState(const HangWatchState&) = delete;
  HangWatchState& operator=(const HangWatchState&) = delete;

  // Allocates a new state object bound to the calling thread and returns an
  // owning pointer to it.
  static std::unique_ptr<HangWatchState> CreateHangWatchStateForCurrentThread();

  // Retrieves the hang watch state associated with the calling thread.
  // Returns nullptr if no HangWatchState exists for the current thread (see
  // CreateHangWatchStateForCurrentThread()).
  static ThreadLocalPointer<HangWatchState>*
  GetHangWatchStateForCurrentThread();

  // Returns the value of the current deadline. Use this function if you need to
  // store the value. To test if the deadline has expired use IsOverDeadline().
  TimeTicks GetDeadline() const;

  // Atomically sets the deadline to a new value.
  void SetDeadline(TimeTicks deadline);

  // Tests whether the associated thread's execution has gone over the deadline.
  bool IsOverDeadline() const;

#if DCHECK_IS_ON()
  // Saves the supplied HangWatchScope as the currently active scope.
  void SetCurrentHangWatchScope(HangWatchScope* scope);

  // Retrieve the currently active scope.
  HangWatchScope* GetCurrentHangWatchScope();
#endif

  PlatformThreadId GetThreadID() const;

 private:
  // The thread that creates the instance should be the class that updates
  // the deadline.
  THREAD_CHECKER(thread_checker_);

  // If the deadline fails to be updated before TimeTicks::Now() ever
  // reaches the value contained in it this constistutes a hang.
  std::atomic<TimeTicks> deadline_{base::TimeTicks::Max()};

  const PlatformThreadId thread_id_;

#if DCHECK_IS_ON()
  // Used to keep track of the current HangWatchScope and detect improper usage.
  // Scopes should always be destructed in reverse order from the one they were
  // constructed in. Example of improper use:
  //
  // {
  //   std::unique_ptr<Scope> scope = std::make_unique<Scope>(...);
  //   Scope other_scope;
  //   |scope| gets deallocated first, violating reverse destruction order.
  //   scope.reset();
  // }
  HangWatchScope* current_hang_watch_scope_{nullptr};
#endif
};

}  // namespace internal
}  // namespace base

#endif  // BASE_THREADING_HANG_WATCHER_H_
