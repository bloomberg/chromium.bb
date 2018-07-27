// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_SEQUENCE_MANAGER_FUZZER_PROCESSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_SEQUENCE_MANAGER_FUZZER_PROCESSOR_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/task/sequence_manager/test/test_task_queue.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/base/proto/sequence_manager_test_description.pb.h"

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
    TaskForTest(uint64_t id, uint64_t start, uint64_t end);
    bool operator==(const TaskForTest& rhs) const;

    uint64_t task_id;
    uint64_t start_time_ms;
    uint64_t end_time_ms;
  };

  struct ActionForTest {
    enum class ActionType {
      kCreateTaskQueue,
      kPostDelayedTask,
      kSetQueuePriority,
      kSetQueueEnabled,
      kCreateQueueVoter,
      kCancelTask,
      kShutdownTaskQueue,
      kInsertFence,
      kRemoveFence
    };

    ActionForTest(uint64_t id, ActionType type, uint64_t start);
    bool operator==(const ActionForTest& rhs) const;

    uint64_t action_id;
    ActionType action_type;
    uint64_t start_time_ms;
  };

  SequenceManagerFuzzerProcessor();

  explicit SequenceManagerFuzzerProcessor(bool log_for_testing);

  void RunTest(const SequenceManagerTestDescription& description);

  void RunAction(const SequenceManagerTestDescription::Action& action);

  const std::vector<TaskForTest>& ordered_tasks() const;
  const std::vector<ActionForTest>& ordered_actions() const;

 private:
  class Task {
   public:
    Task(SequenceManagerFuzzerProcessor* processor);
    ~Task() = default;

    void Execute(const SequenceManagerTestDescription::Task& task);

    bool is_running;
    SequenceManagerFuzzerProcessor* processor_;
    base::WeakPtrFactory<Task> weak_ptr_factory_;
  };

  struct TaskQueueWithVoters {
    TaskQueueWithVoters(scoped_refptr<TestTaskQueue> task_queue)
        : queue(std::move(task_queue)){};

    scoped_refptr<TestTaskQueue> queue;
    std::vector<std::unique_ptr<TaskQueue::QueueEnabledVoter>> voters;
  };

  void ExecuteCreateTaskQueueAction(
      uint64_t action_id,
      const SequenceManagerTestDescription::CreateTaskQueueAction& action);
  void ExecutePostDelayedTaskAction(
      uint64_t action_id,
      const SequenceManagerTestDescription::PostDelayedTaskAction& action);
  void ExecuteSetQueuePriorityAction(
      uint64_t action_id,
      const SequenceManagerTestDescription::SetQueuePriorityAction& action);
  void ExecuteSetQueueEnabledAction(
      uint64_t action_id,
      const SequenceManagerTestDescription::SetQueueEnabledAction& action);
  void ExecuteCreateQueueVoterAction(
      uint64_t action_id,
      const SequenceManagerTestDescription::CreateQueueVoterAction& action);
  void ExecuteShutdownTaskQueueAction(
      uint64_t action_id,
      const SequenceManagerTestDescription::ShutdownTaskQueueAction& action);
  void ExecuteCancelTaskAction(
      uint64_t action_id,
      const SequenceManagerTestDescription::CancelTaskAction& action);
  void ExecuteInsertFenceAction(
      uint64_t action_id,
      const SequenceManagerTestDescription::InsertFenceAction& action);
  void ExecuteRemoveFenceAction(
      uint64_t action_id,
      const SequenceManagerTestDescription::RemoveFenceAction& action);

  void ExecuteTask(const SequenceManagerTestDescription::Task& task);

  void DeleteTask(Task* task);

  void LogTaskForTesting(uint64_t task_id,
                         TimeTicks start_time,
                         TimeTicks end_time);

  void LogActionForTesting(uint64_t action_id,
                           ActionForTest::ActionType type,
                           TimeTicks start_time);

  // Bound to current thread. Used to control the clock of the task queue
  // manager.
  scoped_refptr<TestMockTimeTaskRunner> test_task_runner_;

  std::unique_ptr<SequenceManagerForTest> manager_;

  // For testing purposes, this should follow the order in which the queues were
  // created.
  std::vector<TaskQueueWithVoters> task_queues_;

  // Used to be able to cancel pending tasks from the sequence manager. For
  // testing purposes, this should follow the order in which the tasks were
  // posted.
  std::vector<std::unique_ptr<Task>> pending_tasks_;

  const bool log_for_testing_;

  TimeTicks initial_time_;

  // For Testing. Used to log tasks in their order of execution.
  std::vector<TaskForTest> ordered_tasks_;

  // For Testing. Used to log actions in their order of execution.
  std::vector<ActionForTest> ordered_actions_;
};

}  // namespace sequence_manager
}  // namespace base

#endif
