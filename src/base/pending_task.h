// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PENDING_TASK_H_
#define BASE_PENDING_TASK_H_

#include <array>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/time/time.h"

namespace base {

enum class Nestable : uint8_t {
  kNonNestable,
  kNestable,
};

// Contains data about a pending task. Stored in TaskQueue and DelayedTaskQueue
// for use by classes that queue and execute tasks.
struct BASE_EXPORT PendingTask {
  PendingTask();
  PendingTask(const Location& posted_from, OnceClosure task);
  PendingTask(const Location& posted_from,
              OnceClosure task,
              TimeTicks queue_time,
              TimeTicks delayed_run_time);
  PendingTask(PendingTask&& other);
  ~PendingTask();

  PendingTask& operator=(PendingTask&& other);

  // Used for a min-heap.
  bool operator>(const PendingTask& other) const;

  // Returns the time at which this task should run. This is |delayed_run_time|
  // for a delayed task, |queue_time| otherwise.
  base::TimeTicks GetDesiredExecutionTime() const;

  // The task to run.
  OnceClosure task;

  // The site this PendingTask was posted from.
  Location posted_from;

  // The time at which the task was queued, which happens at post time. For
  // deferred non-nestable tasks, this is reset when the nested loop exits and
  // the deferred tasks are pushed back at the front of the queue. This is not
  // set for immediate SequenceManager tasks unless SetAddQueueTimeToTasks(true)
  // was called. This defaults to a null TimeTicks if the task hasn't been
  // inserted in a sequence yet.
  TimeTicks queue_time;

  // The time when the task should be run. This is null for an immediate task.
  base::TimeTicks delayed_run_time;

  // Chain of symbols of the parent tasks which led to this one being posted.
  static constexpr size_t kTaskBacktraceLength = 4;
  std::array<const void*, kTaskBacktraceLength> task_backtrace = {};

  // The context of the IPC message that was being handled when this task was
  // posted. This is a hash of the IPC message name that is set within the scope
  // of an IPC handler and when symbolized uniquely identifies the message being
  // processed. This property is not propagated from one PendingTask to the
  // next. For example, if pending task A was posted while handling an IPC,
  // and pending task B was posted from within pending task A, then pending task
  // B will not inherit the |ipc_hash| of pending task A.
  uint32_t ipc_hash = 0;
  const char* ipc_interface_name = nullptr;

  // Secondary sort key for run time.
  int sequence_num = 0;

  bool task_backtrace_overflow = false;
};

}  // namespace base

#endif  // BASE_PENDING_TASK_H_
