// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_MAIN_THREAD_TASK_RUNNER_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_MAIN_THREAD_TASK_RUNNER_H_

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace pp {
class Core;
}  // namespace pp

namespace remoting {

// SingleThreadTaskRunner implementation for the main plugin thread.
class PepperMainThreadTaskRunner : public base::SingleThreadTaskRunner {
 public:
  PepperMainThreadTaskRunner();

  // base::SingleThreadTaskRunner interface.
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override;
  bool RunsTasksOnCurrentThread() const override;

 protected:
  ~PepperMainThreadTaskRunner() override;

 private:
  // Helper that allows a base::Closure to be used as a pp::CompletionCallback,
  // by ignoring the completion result.
  void RunTask(int32_t result, const base::Closure& task);

  pp::Core* core_;
  pp::CompletionCallbackFactory<PepperMainThreadTaskRunner> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperMainThreadTaskRunner);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_MAIN_THREAD_TASK_RUNNER_H_
