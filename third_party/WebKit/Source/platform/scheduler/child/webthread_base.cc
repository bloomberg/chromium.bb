// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An implementation of WebThread in terms of base::MessageLoop and
// base::Thread

#include "public/platform/scheduler/child/webthread_base.h"

#include <memory>
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/pending_task.h"
#include "base/threading/platform_thread.h"
#include "platform/scheduler/child/compositor_worker_scheduler.h"
#include "platform/scheduler/child/scheduler_tqm_delegate_impl.h"
#include "platform/scheduler/child/webthread_impl_for_worker_scheduler.h"
#include "platform/scheduler/utility/webthread_impl_for_utility_thread.h"
#include "public/platform/WebTraceLocation.h"
#include "public/platform/scheduler/child/single_thread_idle_task_runner.h"

namespace blink {
namespace scheduler {

class WebThreadBase::TaskObserverAdapter
    : public base::MessageLoop::TaskObserver {
 public:
  explicit TaskObserverAdapter(WebThread::TaskObserver* observer)
      : observer_(observer) {}

  void WillProcessTask(const base::PendingTask& pending_task) override {
    observer_->WillProcessTask();
  }

  void DidProcessTask(const base::PendingTask& pending_task) override {
    observer_->DidProcessTask();
  }

 private:
  WebThread::TaskObserver* observer_;
};

WebThreadBase::WebThreadBase() {}

WebThreadBase::~WebThreadBase() {
  for (auto& observer_entry : task_observer_map_) {
    delete observer_entry.second;
  }
}

void WebThreadBase::AddTaskObserver(TaskObserver* observer) {
  CHECK(IsCurrentThread());
  std::pair<TaskObserverMap::iterator, bool> result =
      task_observer_map_.insert(std::make_pair(observer, nullptr));
  if (result.second)
    result.first->second = new TaskObserverAdapter(observer);
  AddTaskObserverInternal(result.first->second);
}

void WebThreadBase::RemoveTaskObserver(TaskObserver* observer) {
  CHECK(IsCurrentThread());
  TaskObserverMap::iterator iter = task_observer_map_.find(observer);
  if (iter == task_observer_map_.end())
    return;
  RemoveTaskObserverInternal(iter->second);
  delete iter->second;
  task_observer_map_.erase(iter);
}

void WebThreadBase::AddTaskTimeObserver(TaskTimeObserver* task_time_observer) {
  AddTaskTimeObserverInternal(task_time_observer);
}

void WebThreadBase::RemoveTaskTimeObserver(
    TaskTimeObserver* task_time_observer) {
  RemoveTaskTimeObserverInternal(task_time_observer);
}

void WebThreadBase::AddTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  base::MessageLoop::current()->AddTaskObserver(observer);
}

void WebThreadBase::RemoveTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  base::MessageLoop::current()->RemoveTaskObserver(observer);
}

// static
void WebThreadBase::RunWebThreadIdleTask(
    std::unique_ptr<blink::WebThread::IdleTask> idle_task,
    base::TimeTicks deadline) {
  idle_task->Run((deadline - base::TimeTicks()).InSecondsF());
}

void WebThreadBase::PostIdleTask(const blink::WebTraceLocation& location,
                                 IdleTask* idle_task) {
  GetIdleTaskRunner()->PostIdleTask(
      location, base::Bind(&WebThreadBase::RunWebThreadIdleTask,
                           base::Passed(base::WrapUnique(idle_task))));
}

bool WebThreadBase::IsCurrentThread() const {
  return GetTaskRunner()->BelongsToCurrentThread();
}

namespace {

class WebThreadForCompositor : public WebThreadImplForWorkerScheduler {
 public:
  explicit WebThreadForCompositor(base::Thread::Options options)
      : WebThreadImplForWorkerScheduler("Compositor", options) {
    Init();
  }
  ~WebThreadForCompositor() override {}

 private:
  // WebThreadImplForWorkerScheduler:
  std::unique_ptr<blink::scheduler::WorkerScheduler> CreateWorkerScheduler()
      override {
    return std::make_unique<CompositorWorkerScheduler>(GetThread(),
                                                       task_runner_delegate());
  }

  DISALLOW_COPY_AND_ASSIGN(WebThreadForCompositor);
};

}  // namespace

std::unique_ptr<WebThreadBase> WebThreadBase::CreateWorkerThread(
    const char* name,
    base::Thread::Options options) {
  return std::make_unique<WebThreadImplForWorkerScheduler>(name, options);
}

std::unique_ptr<WebThreadBase> WebThreadBase::CreateCompositorThread(
    base::Thread::Options options) {
  return std::make_unique<WebThreadForCompositor>(options);
}

std::unique_ptr<WebThreadBase> WebThreadBase::InitializeUtilityThread() {
  return std::make_unique<WebThreadImplForUtilityThread>();
}

}  // namespace scheduler
}  // namespace blink
