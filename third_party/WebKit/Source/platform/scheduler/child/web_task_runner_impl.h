// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WEB_TASK_RUNNER_IMPL_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WEB_TASK_RUNNER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/WebCommon.h"

namespace blink {
namespace scheduler {
class TaskQueue;

class BLINK_PLATFORM_EXPORT WebTaskRunnerImpl : public WebTaskRunner {
 public:
  static RefPtr<WebTaskRunnerImpl> create(scoped_refptr<TaskQueue> task_queue);

  // WebTaskRunner implementation:
  void postDelayedTask(const WebTraceLocation&,
                       const base::Closure&,
                       double delayMs) override;
  bool runsTasksOnCurrentThread() override;
  double virtualTimeSeconds() const override;
  double monotonicallyIncreasingVirtualTimeSeconds() const override;
  base::SingleThreadTaskRunner* toSingleThreadTaskRunner() override;

 private:
  explicit WebTaskRunnerImpl(scoped_refptr<TaskQueue> task_queue);
  ~WebTaskRunnerImpl() override;

  base::TimeTicks Now() const;

  scoped_refptr<TaskQueue> task_queue_;

  DISALLOW_COPY_AND_ASSIGN(WebTaskRunnerImpl);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WEB_TASK_RUNNER_IMPL_H_
