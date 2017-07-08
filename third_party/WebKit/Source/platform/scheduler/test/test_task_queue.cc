// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/test/test_task_queue.h"

#include "platform/scheduler/base/task_queue_impl.h"

namespace blink {
namespace scheduler {

TestTaskQueue::TestTaskQueue(std::unique_ptr<internal::TaskQueueImpl> impl)
    : TaskQueue(std::move(impl)) {}

TestTaskQueue::~TestTaskQueue() {}

}  // namespace scheduler
}  // namespace blink
