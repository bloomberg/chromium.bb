// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/webthread_impl_for_worker_scheduler.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/default_tick_clock.h"
#include "platform/scheduler/base/task_queue.h"
#include "platform/scheduler/child/scheduler_tqm_delegate_impl.h"
#include "platform/scheduler/child/web_scheduler_impl.h"
#include "platform/scheduler/child/web_task_runner_impl.h"
#include "platform/scheduler/child/worker_scheduler_impl.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {
namespace scheduler {

WebThreadImplForWorkerScheduler::WebThreadImplForWorkerScheduler(
    const char* name)
    : WebThreadImplForWorkerScheduler(name, base::Thread::Options()) {}

WebThreadImplForWorkerScheduler::WebThreadImplForWorkerScheduler(
    const char* name,
    base::Thread::Options options)
    : thread_(new base::Thread(name ? name : std::string())) {
  bool started = thread_->StartWithOptions(options);
  CHECK(started);
  thread_task_runner_ = thread_->task_runner();
}

void WebThreadImplForWorkerScheduler::Init() {
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&WebThreadImplForWorkerScheduler::InitOnThread,
                            base::Unretained(this), &completion));
  completion.Wait();
}

WebThreadImplForWorkerScheduler::~WebThreadImplForWorkerScheduler() {
  if (task_runner_delegate_) {
    base::WaitableEvent completion(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    // Restore the original task runner so that the thread can tear itself down.
    thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&WebThreadImplForWorkerScheduler::RestoreTaskRunnerOnThread,
                   base::Unretained(this), &completion));
    completion.Wait();
  }
  thread_->Stop();
}

void WebThreadImplForWorkerScheduler::InitOnThread(
    base::WaitableEvent* completion) {
  // TODO(alexclarke): Do we need to unify virtual time for workers and the
  // main thread?
  task_runner_delegate_ = SchedulerTqmDelegateImpl::Create(
      thread_->message_loop(), base::MakeUnique<base::DefaultTickClock>());
  worker_scheduler_ = CreateWorkerScheduler();
  worker_scheduler_->Init();
  task_queue_ = worker_scheduler_->DefaultTaskQueue();
  idle_task_runner_ = worker_scheduler_->IdleTaskRunner();
  web_scheduler_.reset(new WebSchedulerImpl(
      worker_scheduler_.get(), worker_scheduler_->IdleTaskRunner(),
      worker_scheduler_->DefaultTaskQueue(),
      worker_scheduler_->DefaultTaskQueue()));
  base::MessageLoop::current()->AddDestructionObserver(this);
  web_task_runner_ = WebTaskRunnerImpl::Create(task_queue_);
  completion->Signal();
}

void WebThreadImplForWorkerScheduler::RestoreTaskRunnerOnThread(
    base::WaitableEvent* completion) {
  task_runner_delegate_->RestoreDefaultTaskRunner();
  completion->Signal();
}

void WebThreadImplForWorkerScheduler::WillDestroyCurrentMessageLoop() {
  task_queue_ = nullptr;
  idle_task_runner_ = nullptr;
  web_scheduler_.reset();
  worker_scheduler_.reset();
  web_task_runner_.Reset();
}

std::unique_ptr<scheduler::WorkerScheduler>
WebThreadImplForWorkerScheduler::CreateWorkerScheduler() {
  return WorkerScheduler::Create(task_runner_delegate_);
}

blink::PlatformThreadId WebThreadImplForWorkerScheduler::ThreadId() const {
  return thread_->GetThreadId();
}

blink::WebScheduler* WebThreadImplForWorkerScheduler::Scheduler() const {
  return web_scheduler_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
WebThreadImplForWorkerScheduler::GetTaskRunner() const {
  return task_queue_;
}

SingleThreadIdleTaskRunner* WebThreadImplForWorkerScheduler::GetIdleTaskRunner()
    const {
  return idle_task_runner_.get();
}

blink::WebTaskRunner* WebThreadImplForWorkerScheduler::GetWebTaskRunner() {
  return web_task_runner_.Get();
}

void WebThreadImplForWorkerScheduler::AddTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  worker_scheduler_->AddTaskObserver(observer);
}

void WebThreadImplForWorkerScheduler::RemoveTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  worker_scheduler_->RemoveTaskObserver(observer);
}

}  // namespace scheduler
}  // namespace blink
