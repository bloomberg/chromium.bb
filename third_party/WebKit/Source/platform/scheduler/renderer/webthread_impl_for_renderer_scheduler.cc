// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/webthread_impl_for_renderer_scheduler.h"

#include "base/location.h"
#include "platform/scheduler/base/task_queue.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/renderer_web_scheduler_impl.h"

namespace blink {
namespace scheduler {

WebThreadImplForRendererScheduler::WebThreadImplForRendererScheduler(
    RendererSchedulerImpl* scheduler)
    : web_scheduler_(new RendererWebSchedulerImpl(scheduler)),
      task_runner_(scheduler->DefaultTaskQueue()),
      idle_task_runner_(scheduler->IdleTaskRunner()),
      scheduler_(scheduler),
      thread_id_(base::PlatformThread::CurrentId()) {}

WebThreadImplForRendererScheduler::~WebThreadImplForRendererScheduler() =
    default;

blink::PlatformThreadId WebThreadImplForRendererScheduler::ThreadId() const {
  return thread_id_;
}

blink::WebScheduler* WebThreadImplForRendererScheduler::Scheduler() const {
  return web_scheduler_.get();
}

SingleThreadIdleTaskRunner*
WebThreadImplForRendererScheduler::GetIdleTaskRunner() const {
  return idle_task_runner_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
WebThreadImplForRendererScheduler::GetTaskRunner() const {
  return task_runner_;
}

void WebThreadImplForRendererScheduler::AddTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  scheduler_->AddTaskObserver(observer);
}

void WebThreadImplForRendererScheduler::RemoveTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  scheduler_->RemoveTaskObserver(observer);
}

void WebThreadImplForRendererScheduler::AddTaskTimeObserverInternal(
    TaskTimeObserver* task_time_observer) {
  scheduler_->AddTaskTimeObserver(task_time_observer);
}

void WebThreadImplForRendererScheduler::RemoveTaskTimeObserverInternal(
    TaskTimeObserver* task_time_observer) {
  scheduler_->RemoveTaskTimeObserver(task_time_observer);
}

void WebThreadImplForRendererScheduler::Init() {}

}  // namespace scheduler
}  // namespace blink
