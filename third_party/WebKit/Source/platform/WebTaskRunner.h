// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTaskRunner_h
#define WebTaskRunner_h

#include <memory>
#include "base/callback.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "platform/wtf/Compiler.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/Time.h"
#include "public/platform/WebCommon.h"

namespace blink {

// TaskHandle is associated to a task posted by PostCancellableTask() or
// PostCancellableDelayedTask() and cancels the associated task on
// TaskHandle::cancel() call or on TaskHandle destruction.
class BLINK_PLATFORM_EXPORT TaskHandle {
 public:
  // Returns true if the task will run later. Returns false if the task is
  // cancelled or the task is run already.
  // This function is not thread safe. Call this on the thread that has posted
  // the task.
  bool IsActive() const;

  // Cancels the task invocation. Do nothing if the task is cancelled or run
  // already.
  // This function is not thread safe. Call this on the thread that has posted
  // the task.
  void Cancel();

  TaskHandle();
  ~TaskHandle();

  TaskHandle(TaskHandle&&);
  TaskHandle& operator=(TaskHandle&&);

  class Runner;

 private:
  friend BLINK_PLATFORM_EXPORT WARN_UNUSED_RESULT TaskHandle
  PostCancellableTask(base::SingleThreadTaskRunner&,
                      const base::Location&,
                      base::OnceClosure);
  friend BLINK_PLATFORM_EXPORT WARN_UNUSED_RESULT TaskHandle
  PostDelayedCancellableTask(base::SingleThreadTaskRunner&,
                             const base::Location&,
                             base::OnceClosure,
                             TimeDelta delay);

  explicit TaskHandle(scoped_refptr<Runner>);
  scoped_refptr<Runner> runner_;
};

// For cross-thread posting. Can be called from any thread.
BLINK_PLATFORM_EXPORT void PostCrossThreadTask(base::SingleThreadTaskRunner&,
                                               const base::Location&,
                                               CrossThreadClosure);
BLINK_PLATFORM_EXPORT void PostDelayedCrossThreadTask(
    base::SingleThreadTaskRunner&,
    const base::Location&,
    CrossThreadClosure,
    TimeDelta delay);

// For same-thread cancellable task posting. Returns a TaskHandle object for
// cancellation.
BLINK_PLATFORM_EXPORT WARN_UNUSED_RESULT TaskHandle
PostCancellableTask(base::SingleThreadTaskRunner&,
                    const base::Location&,
                    base::OnceClosure);
BLINK_PLATFORM_EXPORT WARN_UNUSED_RESULT TaskHandle
PostDelayedCancellableTask(base::SingleThreadTaskRunner&,
                           const base::Location&,
                           base::OnceClosure,
                           TimeDelta delay);

}  // namespace blink

#endif  // WebTaskRunner_h
