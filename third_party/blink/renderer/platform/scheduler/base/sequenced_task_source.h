// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_SEQUENCED_TASK_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_SEQUENCED_TASK_SOURCE_H_

#include "base/optional.h"
#include "base/pending_task.h"

namespace base {
namespace sequence_manager {
class LazyNow;

namespace internal {

// This is temporary interface for ThreadController to be able to run tasks
// from TaskQueueManager.
class SequencedTaskSource {
 public:
  // Take a next task to run from a sequence.
  virtual Optional<PendingTask> TakeTask() = 0;

  // Notify a sequence that a taken task has been completed.
  virtual void DidRunTask() = 0;

  // Returns the delay till the next task, or TimeDelta::Max() if there
  // isn't one.
  virtual TimeDelta DelayTillNextTask(LazyNow* lazy_now) = 0;
};

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_SEQUENCED_TASK_SOURCE_H_
