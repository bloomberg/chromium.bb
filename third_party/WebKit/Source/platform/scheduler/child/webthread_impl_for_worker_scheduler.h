// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WEBTHREAD_IMPL_FOR_WORKER_SCHEDULER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WEBTHREAD_IMPL_FOR_WORKER_SCHEDULER_H_

#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "public/platform/WebPrivatePtr.h"
#include "public/platform/scheduler/child/webthread_base.h"

namespace base {
class WaitableEvent;
}

namespace blink {
class WebScheduler;
}

namespace blink {
namespace scheduler {
class SchedulerTqmDelegate;
class SingleThreadIdleTaskRunner;
class TaskQueue;
class WebSchedulerImpl;
class WebTaskRunnerImpl;
class WorkerScheduler;

class PLATFORM_EXPORT WebThreadImplForWorkerScheduler
    : public WebThreadBase,
      public base::MessageLoop::DestructionObserver {
 public:
  explicit WebThreadImplForWorkerScheduler(const char* name);
  WebThreadImplForWorkerScheduler(const char* name,
                                  base::Thread::Options options);
  ~WebThreadImplForWorkerScheduler() override;

  // WebThread implementation.
  WebScheduler* Scheduler() const override;
  PlatformThreadId ThreadId() const override;
  WebTaskRunner* GetWebTaskRunner() override;

  // WebThreadBase implementation.
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() const override;
  scheduler::SingleThreadIdleTaskRunner* GetIdleTaskRunner() const override;
  void Init() override;

  // base::MessageLoop::DestructionObserver implementation.
  void WillDestroyCurrentMessageLoop() override;

  scheduler::WorkerScheduler* GetWorkerScheduler() {
    return worker_scheduler_.get();
  }

 protected:
  base::Thread* GetThread() const { return thread_.get(); }
  SchedulerTqmDelegate* task_runner_delegate() const {
    return task_runner_delegate_.get();
  }

 private:
  virtual std::unique_ptr<scheduler::WorkerScheduler> CreateWorkerScheduler();

  void AddTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer) override;
  void RemoveTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer) override;

  void InitOnThread(base::WaitableEvent* completion);
  void RestoreTaskRunnerOnThread(base::WaitableEvent* completion);

  std::unique_ptr<base::Thread> thread_;
  std::unique_ptr<scheduler::WorkerScheduler> worker_scheduler_;
  std::unique_ptr<scheduler::WebSchedulerImpl> web_scheduler_;
  scoped_refptr<base::SingleThreadTaskRunner> thread_task_runner_;
  scoped_refptr<TaskQueue> task_queue_;
  scoped_refptr<scheduler::SingleThreadIdleTaskRunner> idle_task_runner_;
  scoped_refptr<SchedulerTqmDelegate> task_runner_delegate_;
  WebPrivatePtr<WebTaskRunnerImpl> web_task_runner_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WEBTHREAD_IMPL_FOR_WORKER_SCHEDULER_H_
