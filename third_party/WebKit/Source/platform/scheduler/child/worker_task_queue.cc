// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/worker_task_queue.h"

#include "platform/scheduler/base/task_queue_impl.h"

namespace blink {
namespace scheduler {

WorkerTaskQueue::WorkerTaskQueue(std::unique_ptr<internal::TaskQueueImpl> impl)
    : TaskQueue(std::move(impl)) {}

WorkerTaskQueue::~WorkerTaskQueue() {}

}  // namespace scheduler
}  // namespace blink
