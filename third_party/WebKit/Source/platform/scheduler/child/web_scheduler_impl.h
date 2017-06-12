// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WEB_SCHEDULER_IMPL_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WEB_SCHEDULER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "platform/PlatformExport.h"
#include "platform/scheduler/base/task_queue.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "public/platform/WebThread.h"

namespace blink {
namespace scheduler {

class ChildScheduler;
class SingleThreadIdleTaskRunner;
class WebTaskRunnerImpl;

class PLATFORM_EXPORT WebSchedulerImpl : public WebScheduler {
 public:
  WebSchedulerImpl(ChildScheduler* child_scheduler,
                   scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner,
                   scoped_refptr<TaskQueue> loading_task_runner,
                   scoped_refptr<TaskQueue> timer_task_runner);
  ~WebSchedulerImpl() override;

  // WebScheduler implementation:
  void Shutdown() override;
  bool ShouldYieldForHighPriorityWork() override;
  bool CanExceedIdleDeadlineIfRequired() override;
  void PostIdleTask(const WebTraceLocation& location,
                    WebThread::IdleTask* task) override;
  void PostNonNestableIdleTask(const WebTraceLocation& location,
                               WebThread::IdleTask* task) override;
  WebTaskRunner* LoadingTaskRunner() override;
  WebTaskRunner* TimerTaskRunner() override;
  WebTaskRunner* CompositorTaskRunner() override;
  std::unique_ptr<WebViewScheduler> CreateWebViewScheduler(
      InterventionReporter*);
  void SuspendTimerQueue() override {}
  void ResumeTimerQueue() override {}
  void AddPendingNavigation(
      scheduler::RendererScheduler::NavigatingFrameType type) override {}
  void RemovePendingNavigation(
      scheduler::RendererScheduler::NavigatingFrameType type) override {}

 private:
  static void RunIdleTask(std::unique_ptr<WebThread::IdleTask> task,
                          base::TimeTicks deadline);

  ChildScheduler* child_scheduler_;  // NOT OWNED
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;
  RefPtr<WebTaskRunnerImpl> loading_web_task_runner_;
  RefPtr<WebTaskRunnerImpl> timer_web_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebSchedulerImpl);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WEB_SCHEDULER_IMPL_H_
