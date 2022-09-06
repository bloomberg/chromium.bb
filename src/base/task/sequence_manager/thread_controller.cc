// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/thread_controller.h"

#include "base/check.h"
#include "base/trace_event/base_tracing.h"

namespace base {
namespace sequence_manager {
namespace internal {

ThreadController::ThreadController()
    : associated_thread_(AssociatedThreadId::CreateUnbound()) {}

ThreadController::~ThreadController() = default;

ThreadController::RunLevelTracker::RunLevelTracker(
    const ThreadController& outer)
    : outer_(outer) {}
ThreadController::RunLevelTracker::~RunLevelTracker() {
  DCHECK_CALLED_ON_VALID_THREAD(outer_.associated_thread_->thread_checker);

  // There shouldn't be any remaining |run_levels_| by the time this unwinds.
  DCHECK_EQ(run_levels_.size(), 0u);
}

void ThreadController::RunLevelTracker::OnRunLoopStarted(State initial_state) {
  DCHECK_CALLED_ON_VALID_THREAD(outer_.associated_thread_->thread_checker);
  run_levels_.emplace(initial_state, !run_levels_.empty());
}

void ThreadController::RunLevelTracker::OnRunLoopEnded() {
  DCHECK_CALLED_ON_VALID_THREAD(outer_.associated_thread_->thread_checker);
  // Normally this will occur while kIdle or kInBetweenTasks but it can also
  // occur while kRunningTask in rare situations where the owning
  // ThreadController is deleted from within a task. Ref.
  // SequenceManagerWithTaskRunnerTest.DeleteSequenceManagerInsideATask. Thus we
  // can't assert anything about the current state other than that it must be
  // exiting an existing RunLevel.
  DCHECK(!run_levels_.empty());
  run_levels_.pop();
}

void ThreadController::RunLevelTracker::OnTaskStarted() {
  DCHECK_CALLED_ON_VALID_THREAD(outer_.associated_thread_->thread_checker);
  // Ignore tasks outside the main run loop.
  // The only practical case where this would happen is if a native loop is spun
  // outside the main runloop (e.g. system dialog during startup). We cannot
  // support this because we are not guaranteed to be able to observe its exit
  // (like we would inside an application task which is at least guaranteed to
  // itself notify us when it ends).
  if (run_levels_.empty())
    return;

  // Already running a task?
  if (run_levels_.top().state() == kRunningTask) {
    // #task-in-task-implies-nested
    run_levels_.emplace(kRunningTask, true);
  } else {
    // Going from kIdle or kInBetweenTasks to kRunningTask.
    run_levels_.top().UpdateState(kRunningTask);
  }
}

void ThreadController::RunLevelTracker::OnTaskEnded() {
  DCHECK_CALLED_ON_VALID_THREAD(outer_.associated_thread_->thread_checker);
  if (run_levels_.empty())
    return;

  // #done-task-while-not-running-implies-done-nested
  if (run_levels_.top().state() != kRunningTask)
    run_levels_.pop();

  // Whether we exited a nested run-level or not: the current run-level is now
  // transitioning from kRunningTask to kInBetweenTasks.
  DCHECK_EQ(run_levels_.top().state(), kRunningTask);
  run_levels_.top().UpdateState(kInBetweenTasks);
}

void ThreadController::RunLevelTracker::OnIdle() {
  DCHECK_CALLED_ON_VALID_THREAD(outer_.associated_thread_->thread_checker);
  if (run_levels_.empty())
    return;

  // This is similar to the logic in OnTaskStarted().
  if (run_levels_.top().state() == kRunningTask) {
    // #task-in-task-implies-nested
    // While OnIdle() isn't typically thought of as a "task" it is a way to "do
    // work" and, on platforms like Mac which uses an |idle_work_source_|,
    // DoIdleWork() can be invoked without DoWork() being first invoked at this
    // run-level. We need to create a nested kIdle RunLevel or we break
    // #done-task-while-not-running-implies-done-nested.
    run_levels_.emplace(kIdle, true);
  } else {
    // Simply going kIdle at the current run-level.
    run_levels_.top().UpdateState(kIdle);
  }
}

// static
void ThreadController::RunLevelTracker::SetTraceObserverForTesting(
    TraceObserverForTesting* trace_observer_for_testing) {
  DCHECK_NE(!!trace_observer_for_testing_, !!trace_observer_for_testing);
  trace_observer_for_testing_ = trace_observer_for_testing;
}

// static
ThreadController::RunLevelTracker::TraceObserverForTesting*
    ThreadController::RunLevelTracker::trace_observer_for_testing_ = nullptr;

ThreadController::RunLevelTracker::RunLevel::RunLevel(State initial_state,
                                                      bool is_nested)
    : is_nested_(is_nested),
      thread_controller_sample_metadata_("ThreadController active",
                                         base::SampleMetadataScope::kThread) {
  UpdateState(initial_state);
}

ThreadController::RunLevelTracker::RunLevel::~RunLevel() {
  UpdateState(kIdle);
  // Intentionally ordered after UpdateState(kIdle), reinstantiates
  // thread_controller_sample_metadata_ when yielding back to a parent RunLevel
  // (which is active by definition as it is currently running this one).
  if (is_nested_) {
    thread_controller_sample_metadata_.Set(
        static_cast<int64_t>(++thread_controller_active_id_));
  }
}

ThreadController::RunLevelTracker::RunLevel::RunLevel(RunLevel&& other)
    : state_(std::exchange(other.state_, kIdle)),
      is_nested_(std::exchange(other.is_nested_, false)),
      thread_controller_sample_metadata_(
          other.thread_controller_sample_metadata_),
      thread_controller_active_id_(other.thread_controller_active_id_) {}
ThreadController::RunLevelTracker::RunLevel&
ThreadController::RunLevelTracker::RunLevel::operator=(RunLevel&& other) {
  state_ = std::exchange(other.state_, kIdle);
  return *this;
}

void ThreadController::RunLevelTracker::RunLevel::UpdateState(State new_state) {
  // The only state that can be redeclared is idle, anything else should be a
  // transition.
  DCHECK(state_ != new_state || new_state == kIdle)
      << state_ << "," << new_state;

  const bool was_active = state_ != kIdle;
  const bool is_active = new_state != kIdle;

  state_ = new_state;
  if (was_active == is_active)
    return;

  // Change of state.
  if (is_active) {
    TRACE_EVENT_BEGIN0("base", "ThreadController active");
    // Overriding the annotation from the previous RunLevel is intentional. Only
    // the top RunLevel is ever updated, which holds the relevant state.
    thread_controller_sample_metadata_.Set(
        static_cast<int64_t>(++thread_controller_active_id_));
  } else {
    thread_controller_sample_metadata_.Remove();
    TRACE_EVENT_END0("base", "ThreadController active");
    // TODO(crbug.com/1021571): Remove this once fixed.
    PERFETTO_INTERNAL_ADD_EMPTY_EVENT();
  }

  if (trace_observer_for_testing_) {
    if (is_active)
      trace_observer_for_testing_->OnThreadControllerActiveBegin();
    else
      trace_observer_for_testing_->OnThreadControllerActiveEnd();
  }
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
