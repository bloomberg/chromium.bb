// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_SEQUENCE_MANAGER_FUZZER_PROCESSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_SEQUENCE_MANAGER_FUZZER_PROCESSOR_H_

#include <memory>
#include <vector>

#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/base/proto/sequence_manager_test_description.pb.h"
#include "third_party/blink/renderer/platform/scheduler/base/test/sequence_manager_for_test.h"
#include "third_party/blink/renderer/platform/scheduler/base/test/test_task_queue.h"

namespace base {
namespace sequence_manager {

// Provides functionality to parse the fuzzer's test description and run the
// relevant APIs.
class PLATFORM_EXPORT SequenceManagerFuzzerProcessor {
 public:
  // Public interface used to parse the fuzzer's test description and
  // run the relevant APIs.
  static void ParseAndRun(const SequenceManagerTestDescription& description);

 protected:
  struct TaskForTest {
    uint64_t task_id;
    uint64_t start_time_ms;
    uint64_t end_time_ms;

    TaskForTest(uint64_t id, uint64_t start, uint64_t end);
    bool operator==(const TaskForTest& rhs) const;
  };

  explicit SequenceManagerFuzzerProcessor(
      const SequenceManagerTestDescription& description);

  SequenceManagerFuzzerProcessor(
      const SequenceManagerTestDescription& description,
      bool log_tasks_for_testing);

  void RunTest();

  void RunAction(const SequenceManagerTestDescription::Action& action);

  const std::vector<TaskForTest>& ordered_tasks() const;

 private:
  void CreateTaskQueueFromAction(
      const SequenceManagerTestDescription::CreateTaskQueueAction& action);

  void PostDelayedTaskFromAction(
      const SequenceManagerTestDescription::PostDelayedTaskAction& action);

  void ExecuteTask(const SequenceManagerTestDescription::Task& task);

  std::unique_ptr<SequenceManagerForTest> manager_;

  // Bound to current thread. Used to control the clock of the task queue
  // manager.
  scoped_refptr<TestMockTimeTaskRunner> test_task_runner_;

  std::vector<scoped_refptr<TestTaskQueue>> task_queues_;

  const bool log_tasks_for_testing_;

  TimeTicks initial_time_;

  // For Testing. Used to log tasks in their order of execution.
  std::vector<TaskForTest> ordered_tasks_;
};

}  // namespace sequence_manager
}  // namespace base

#endif
