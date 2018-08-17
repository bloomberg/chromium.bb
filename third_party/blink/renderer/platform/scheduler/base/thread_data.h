// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_THREAD_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_THREAD_DATA_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/task/sequence_manager/test/test_task_queue.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/base/sequence_manager_fuzzer_processor.h"

namespace base {
namespace sequence_manager {

class PLATFORM_EXPORT ThreadData {
 public:
  struct TaskQueueWithVoters {
    explicit TaskQueueWithVoters(scoped_refptr<TestTaskQueue> task_queue)
        : queue(std::move(task_queue)){};

    scoped_refptr<TestTaskQueue> queue;
    std::vector<std::unique_ptr<TaskQueue::QueueEnabledVoter>> voters;
  };

  // |initial_time| is the time in which |this| was instantiated.
  explicit ThreadData(TimeTicks initial_time);

  ~ThreadData();

  // Bound to the thread in which this object was instantiated. Used to
  // control the clock of the sequence manager.
  scoped_refptr<TestMockTimeTaskRunner> test_task_runner_;

  std::unique_ptr<SequenceManagerForTest> manager_;

  // For testing purposes, this should follow the order in which queues
  // were created on the thread in which |this| was instantiated.
  std::vector<TaskQueueWithVoters> task_queues_;

  // Used to be able to cancel pending tasks from the sequence manager. For
  // testing purposes, this should follow the order in which the tasks were
  // posted to the thread in which |this| was instantiated.
  std::vector<std::unique_ptr<SequenceManagerFuzzerProcessor::Task>>
      pending_tasks_;

  // For Testing. Used to log tasks in their order of execution on the
  // thread in which |this| was instantiated.
  std::vector<SequenceManagerFuzzerProcessor::TaskForTest> ordered_tasks_;

  // For Testing. Used to log actions in their order of execution on the
  // thread in which |this| was instantiated.
  std::vector<SequenceManagerFuzzerProcessor::ActionForTest> ordered_actions_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace sequence_manager
}  // namespace base

#endif
