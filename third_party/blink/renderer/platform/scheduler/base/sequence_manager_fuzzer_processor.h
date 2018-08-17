// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_SEQUENCE_MANAGER_FUZZER_PROCESSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_SEQUENCE_MANAGER_FUZZER_PROCESSOR_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/base/proto/sequence_manager_test_description.pb.h"

namespace base {
namespace sequence_manager {

class ThreadData;
class ThreadPoolManager;

// Provides functionality to parse the fuzzer's test description and run the
// relevant APIs.
//
// Warning: For unit testing purposes, the thread data of the threads managed by
// the |thread_pool_manager_| should live for the scope of the main thread
// entry function i.e RunTest.
class PLATFORM_EXPORT SequenceManagerFuzzerProcessor {
 public:
  // Public interface used to parse the fuzzer's test description and
  // run the relevant APIs.
  static void ParseAndRun(const SequenceManagerTestDescription& description);

 protected:
  struct TaskForTest {
    TaskForTest(uint64_t id, uint64_t start_time_ms, uint64_t end_time_ms);
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
      kRemoveFence,
      kCreateThread,
    };

    ActionForTest(uint64_t id, ActionType type, uint64_t start_time_ms);

    bool operator==(const ActionForTest& rhs) const;

    uint64_t action_id;
    ActionType action_type;
    uint64_t start_time_ms;
  };

  class Task {
   public:
    Task(ThreadData* thread_data, SequenceManagerFuzzerProcessor* processor);
    ~Task() = default;

    void Execute(const SequenceManagerTestDescription::Task& task);

    bool is_running;
    SequenceManagerFuzzerProcessor* processor_;
    ThreadData* thread_data_;
    base::WeakPtrFactory<Task> weak_ptr_factory_;
  };

  SequenceManagerFuzzerProcessor();

  explicit SequenceManagerFuzzerProcessor(bool log_for_testing);

  ~SequenceManagerFuzzerProcessor();

  void RunTest(const SequenceManagerTestDescription& description);

  void ExecuteThread(
      ThreadData* thread_data,
      const google::protobuf::RepeatedPtrField<
          SequenceManagerTestDescription::Action>& initial_thread_actions);

  // Returns an ordered list of tasks executed on each thread. Note that the
  // ordering of the threads isn't deterministic since it follows the order in
  // which the threads were constructed. Furthermore, given that
  // ThreadPoolManager::CreateThread is used to construct these threads and
  // given that it can be called from multiple threads, the order of
  // construction isn't deterministic.
  const std::vector<std::vector<TaskForTest>>& ordered_tasks() const;

  // Returns an ordered list of actions executed on each thread. Note that the
  // ordering of the threads isn't deterministic. For more details, check the
  // comment above on ordered_tasks().
  const std::vector<std::vector<ActionForTest>>& ordered_actions() const;

 private:
  friend class ThreadData;
  friend class ThreadPoolManager;

  void RunAction(ThreadData* thread_data,
                 const SequenceManagerTestDescription::Action& action);

  void ExecuteCreateThreadAction(
      ThreadData* thread_data,
      uint64_t action_id,
      const SequenceManagerTestDescription::CreateThreadAction& action);
  void ExecuteCreateTaskQueueAction(
      ThreadData* thread_data,
      uint64_t action_id,
      const SequenceManagerTestDescription::CreateTaskQueueAction& action);
  void ExecutePostDelayedTaskAction(
      ThreadData* thread_data,
      uint64_t action_id,
      const SequenceManagerTestDescription::PostDelayedTaskAction& action);
  void ExecuteSetQueuePriorityAction(
      ThreadData* thread_data,
      uint64_t action_id,
      const SequenceManagerTestDescription::SetQueuePriorityAction& action);
  void ExecuteSetQueueEnabledAction(
      ThreadData* thread_data,
      uint64_t action_id,
      const SequenceManagerTestDescription::SetQueueEnabledAction& action);
  void ExecuteCreateQueueVoterAction(
      ThreadData* thread_data,
      uint64_t action_id,
      const SequenceManagerTestDescription::CreateQueueVoterAction& action);
  void ExecuteShutdownTaskQueueAction(
      ThreadData* thread_data,
      uint64_t action_id,
      const SequenceManagerTestDescription::ShutdownTaskQueueAction& action);
  void ExecuteCancelTaskAction(
      ThreadData* thread_data,
      uint64_t action_id,
      const SequenceManagerTestDescription::CancelTaskAction& action);
  void ExecuteInsertFenceAction(
      ThreadData* thread_data,
      uint64_t action_id,
      const SequenceManagerTestDescription::InsertFenceAction& action);
  void ExecuteRemoveFenceAction(
      ThreadData* thread_data,
      uint64_t action_id,
      const SequenceManagerTestDescription::RemoveFenceAction& action);

  void ExecuteTask(ThreadData* thread_data,
                   const SequenceManagerTestDescription::Task& task);

  void DeleteTask(Task* task);

  void LogTaskForTesting(ThreadData* thread_data,
                         uint64_t task_id,
                         TimeTicks start_time,
                         TimeTicks end_time);

  void LogActionForTesting(ThreadData* thread_data,
                           uint64_t action_id,
                           ActionForTest::ActionType type,
                           TimeTicks start_time);

  const bool log_for_testing_;

  TimeTicks initial_time_;

  std::unique_ptr<ThreadPoolManager> thread_pool_manager_;

  // The clock of the main thread data task runner is initialized to
  // |initial_time_| and never advanced, since it can only execute actions at
  // the start of the program.
  std::unique_ptr<ThreadData> main_thread_data_;

  // For Testing. Each entry contains the ordered list of tasks for one of the
  // created threads. The first entry is reserved for the main thread (which is
  // always empty since no tasks are executed on the main thread).
  std::vector<std::vector<TaskForTest>> ordered_tasks_;

  // For Testing. Each entry contains the ordered list of actions for one of the
  // created threads. The first entry is reserved for the main thread (which
  // can only contain ActionType::kCreateThread actions).
  std::vector<std::vector<ActionForTest>> ordered_actions_;
};

}  // namespace sequence_manager
}  // namespace base

#endif
