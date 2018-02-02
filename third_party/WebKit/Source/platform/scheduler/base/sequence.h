// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_SEQUENCE_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_SEQUENCE_H_

#include "base/optional.h"
#include "base/pending_task.h"

namespace blink {
namespace scheduler {
class LazyNow;

namespace internal {

// This is temporary interface for ThreadController to be able to run tasks
// from TaskQueueManager.
// TODO(alexclarke): Rename to SequencedTaskSource.
class Sequence {
 public:
  // TODO(alexclarke): Move this enum elsewhere.
  enum class WorkType { kImmediate, kDelayed };

  // Take a next task to run from a sequence.
  // TODO(altimin): Do not pass |work_type| here.
  virtual base::Optional<base::PendingTask> TakeTask() = 0;

  // Notify a sequence that a taken task has been completed.
  virtual void DidRunTask() = 0;

  // Returns the delay till the next task, or base::TimeDelta::Max() if there
  // isn't one.
  virtual base::TimeDelta DelayTillNextTask(LazyNow* lazy_now) = 0;
};

}  // namespace internal
}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_SEQUENCE_H_
