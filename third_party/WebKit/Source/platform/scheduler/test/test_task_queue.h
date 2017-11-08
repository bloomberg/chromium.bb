// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_TEST_TASK_QUEUE_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_TEST_TASK_QUEUE_H_

#include "platform/scheduler/base/task_queue.h"

namespace blink {
namespace scheduler {

class TestTaskQueue : public TaskQueue {
 public:
  explicit TestTaskQueue(std::unique_ptr<internal::TaskQueueImpl> impl,
                         const TaskQueue::Spec& spec);
  ~TestTaskQueue() override;

  using TaskQueue::GetTaskQueueImpl;

  base::WeakPtr<TestTaskQueue> GetWeakPtr();

 private:
  // Used to ensure that task queue is deleted in tests.
  base::WeakPtrFactory<TestTaskQueue> weak_factory_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_TEST_TASK_QUEUE_H_
