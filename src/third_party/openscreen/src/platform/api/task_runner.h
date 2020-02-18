// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TASK_RUNNER_H_
#define PLATFORM_API_TASK_RUNNER_H_

#include <future>

#include "absl/types/optional.h"
#include "platform/api/time.h"

namespace openscreen {
namespace platform {

// A thread-safe API surface that allows for posting tasks. The underlying
// implementation may be single or multi-threaded, and all complication should
// be handled by the implementation class. It is the expectation of this API
// that the underlying impl gives the following guarantees:
// (1) Tasks shall not overlap in time/CPU.
// (2) Tasks shall run sequentially, e.g. posting task A then B implies
//     that A shall run before B.
class TaskRunner {
 public:
  using Task = std::packaged_task<void() noexcept>;

  virtual ~TaskRunner() = default;

  // Takes any callable target (function, lambda-expression, std::bind result,
  // etc.) that should be run at the first convenient time.
  template <typename Functor>
  inline void PostTask(Functor f) {
    PostPackagedTask(Task(std::move(f)));
  }

  // Takes any callable target (function, lambda-expression, std::bind result,
  // etc.) that should be run no sooner than |delay| time from now. Note that
  // the Task might run after an additional delay, especially under heavier
  // system load. There is no deadline concept.
  template <typename Functor>
  inline void PostTaskWithDelay(Functor f, Clock::duration delay) {
    PostPackagedTaskWithDelay(Task(std::move(f)), delay);
  }

  // Implementations should provide the behavior explained in the comments above
  // for PostTask[WithDelay](). Client code may also call these directly when
  // passing an existing Task object.
  virtual void PostPackagedTask(Task task) = 0;
  virtual void PostPackagedTaskWithDelay(Task task, Clock::duration delay) = 0;

  // Return true if the calling thread is the thread that task runner is using
  // to run tasks, false otherwise.
  virtual bool IsRunningOnTaskRunner() { return true; }
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_TASK_RUNNER_H_
