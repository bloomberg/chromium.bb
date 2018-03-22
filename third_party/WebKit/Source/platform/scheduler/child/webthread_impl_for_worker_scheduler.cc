// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/webthread_impl_for_worker_scheduler.h"

#include <memory>
#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/default_tick_clock.h"
#include "platform/scheduler/base/task_queue.h"
#include "platform/scheduler/child/web_scheduler_impl.h"
#include "platform/scheduler/child/worker_scheduler_impl.h"
#include "platform/scheduler/child/worker_scheduler_proxy.h"

namespace blink {
namespace scheduler {

WebThreadImplForWorkerScheduler::WebThreadImplForWorkerScheduler(
    const WebThreadCreationParams& params)
    : thread_(new base::Thread(params.name ? params.name : std::string())),
      thread_type_(params.thread_type),
      worker_scheduler_proxy_(
          params.frame_scheduler
              ? std::make_unique<WorkerSchedulerProxy>(params.frame_scheduler)
              : nullptr) {
  bool started = thread_->StartWithOptions(params.thread_options);
  CHECK(started);
  thread_task_runner_ = thread_->task_runner();
}

void WebThreadImplForWorkerScheduler::Init() {
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebThreadImplForWorkerScheduler::InitOnThread,
                                base::Unretained(this), &completion));
  completion.Wait();
}

WebThreadImplForWorkerScheduler::~WebThreadImplForWorkerScheduler() {
  // We want to avoid blocking main thread when the thread was already
  // shut down, but calling ShutdownOnThread twice does not cause any problems.
  if (!was_shutdown_on_thread_.IsSet()) {
    base::WaitableEvent completion(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WebThreadImplForWorkerScheduler::ShutdownOnThread,
                       base::Unretained(this), &completion));
    completion.Wait();
  }
  thread_->Stop();
}

void WebThreadImplForWorkerScheduler::InitOnThread(
    base::WaitableEvent* completion) {
  // TODO(alexclarke): Do we need to unify virtual time for workers and the
  // main thread?
  worker_scheduler_ = CreateWorkerScheduler();
  worker_scheduler_->Init();
  task_queue_ = worker_scheduler_->DefaultTaskQueue();
  idle_task_runner_ = worker_scheduler_->IdleTaskRunner();
  web_scheduler_.reset(new WebSchedulerImpl(
      worker_scheduler_.get(), worker_scheduler_->IdleTaskRunner(),
      worker_scheduler_->DefaultTaskQueue()));
  base::MessageLoop::current()->AddDestructionObserver(this);
  completion->Signal();
}

void WebThreadImplForWorkerScheduler::ShutdownOnThread(
    base::WaitableEvent* completion) {
  was_shutdown_on_thread_.Set();

  task_queue_ = nullptr;
  idle_task_runner_ = nullptr;
  web_scheduler_ = nullptr;
  worker_scheduler_ = nullptr;

  if (completion)
    completion->Signal();
}

std::unique_ptr<WorkerScheduler>
WebThreadImplForWorkerScheduler::CreateWorkerScheduler() {
  return WorkerScheduler::Create(thread_type_, worker_scheduler_proxy_.get());
}

void WebThreadImplForWorkerScheduler::WillDestroyCurrentMessageLoop() {
  ShutdownOnThread(nullptr);
}

blink::PlatformThreadId WebThreadImplForWorkerScheduler::ThreadId() const {
  return thread_->GetThreadId();
}

blink::WebScheduler* WebThreadImplForWorkerScheduler::Scheduler() const {
  return web_scheduler_.get();
}

SingleThreadIdleTaskRunner* WebThreadImplForWorkerScheduler::GetIdleTaskRunner()
    const {
  return idle_task_runner_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
WebThreadImplForWorkerScheduler::GetTaskRunner() const {
  return task_queue_;
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
