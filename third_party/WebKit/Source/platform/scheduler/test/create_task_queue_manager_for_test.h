// Copyright 2017 The Chromium Authors. All rights reserved.  // Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_CREATE_TASK_QUEUE_MANAGER_FOR_TEST_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_CREATE_TASK_QUEUE_MANAGER_FOR_TEST_H_

#include "base/single_thread_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "platform/scheduler/base/task_queue_manager.h"

namespace base {
class MessageLoop;
}

namespace blink {
namespace scheduler {

std::unique_ptr<TaskQueueManager> CreateTaskQueueManagerWithUnownedClockForTest(
    base::MessageLoop* message_loop,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::SimpleTestTickClock* clock);

std::unique_ptr<TaskQueueManager> CreateTaskQueueManagerForTest(
    base::MessageLoop* message_loop,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<base::TickClock> clock);

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_CREATE_TASK_QUEUE_MANAGER_FOR_TEST_H_
