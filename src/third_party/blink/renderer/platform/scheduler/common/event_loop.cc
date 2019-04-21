// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "v8/include/v8.h"

namespace blink {
namespace scheduler {

// static
scoped_refptr<EventLoop> EventLoop::CreateForWorkerOrWorklet(
    v8::Isolate* isolate) {
  return base::AdoptRef(new EventLoop(isolate));
}

EventLoop::EventLoop(v8::Isolate* isolate)
    : isolate_(isolate), microtask_queue_(v8::MicrotaskQueue::New(isolate)) {
  DCHECK(isolate_);
}

EventLoop::~EventLoop() {
  microtask_queue_ = nullptr;

  // TODO(tzik): Remove the instance from associated EventLoopGroup.
}

void EventLoop::EnqueueMicrotask(base::OnceClosure task) {
  pending_microtasks_.push_back(std::move(task));
  microtask_queue_->EnqueueMicrotask(isolate_, &EventLoop::RunPendingMicrotask,
                                     this);
}

void EventLoop::PerformMicrotaskCheckpoint() {
  microtask_queue_->PerformCheckpoint(isolate_);
}

// static
void EventLoop::PerformIsolateGlobalMicrotasksCheckpoint(v8::Isolate* isolate) {
  v8::MicrotasksScope::PerformCheckpoint(isolate);
}

void EventLoop::Disable() {
  loop_enabled_ = false;
  // TODO(tzik): Disable associated Frames.
}

void EventLoop::Enable() {
  loop_enabled_ = true;
  // TODO(tzik): Enable associated Frames.
}

// static
void EventLoop::RunPendingMicrotask(void* data) {
  TRACE_EVENT0("renderer.scheduler", "RunPendingMicrotask");
  auto* self = static_cast<EventLoop*>(data);
  base::OnceClosure task = std::move(self->pending_microtasks_.front());
  self->pending_microtasks_.pop_front();
  std::move(task).Run();
}

}  // namespace scheduler
}  // namespace blink
