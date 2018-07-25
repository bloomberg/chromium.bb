#include "third_party/blink/renderer/platform/scheduler/base/sequence_manager_fuzzer_processor.h"

#include <algorithm>
#include <string>

#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace base {
namespace sequence_manager {

namespace {

TaskQueue::QueuePriority ToTaskQueuePriority(
    SequenceManagerTestDescription::QueuePriority priority) {
  switch (priority) {
    case SequenceManagerTestDescription::BEST_EFFORT:
      return TaskQueue::kBestEffortPriority;
    case SequenceManagerTestDescription::LOW:
      return TaskQueue::kLowPriority;
    case SequenceManagerTestDescription::UNDEFINED:
    case SequenceManagerTestDescription::NORMAL:
      return TaskQueue::kNormalPriority;
    case SequenceManagerTestDescription::HIGH:
      return TaskQueue::kHighPriority;
    case SequenceManagerTestDescription::HIGHEST:
      return TaskQueue::kHighestPriority;
    case SequenceManagerTestDescription::CONTROL:
      return TaskQueue::kControlPriority;
  }
}

}  // namespace

void SequenceManagerFuzzerProcessor::ParseAndRun(
    const SequenceManagerTestDescription& description) {
  SequenceManagerFuzzerProcessor processor;
  processor.RunTest(description);
}

SequenceManagerFuzzerProcessor::SequenceManagerFuzzerProcessor()
    : SequenceManagerFuzzerProcessor(false) {}

SequenceManagerFuzzerProcessor::SequenceManagerFuzzerProcessor(
    bool log_for_testing)
    : log_for_testing_(log_for_testing) {
  test_task_runner_ = WrapRefCounted(
      new TestMockTimeTaskRunner(TestMockTimeTaskRunner::Type::kBoundToThread));

  // A zero clock triggers some assertions.
  test_task_runner_->AdvanceMockTickClock(TimeDelta::FromMilliseconds(1));

  initial_time_ = test_task_runner_->GetMockTickClock()->NowTicks();

  manager_ =
      SequenceManagerForTest::Create(nullptr, ThreadTaskRunnerHandle::Get(),
                                     test_task_runner_->GetMockTickClock());

  TaskQueue::Spec spec = TaskQueue::Spec("default_task_queue");
  task_queues_.emplace_back(manager_->CreateTaskQueue<TestTaskQueue>(spec));
}

void SequenceManagerFuzzerProcessor::RunTest(
    const SequenceManagerTestDescription& description) {
  for (const auto& initial_action : description.initial_actions()) {
    RunAction(initial_action);
  }

  test_task_runner_->FastForwardUntilNoTasksRemain();
}

void SequenceManagerFuzzerProcessor::RunAction(
    const SequenceManagerTestDescription::Action& action) {
  if (action.has_create_task_queue()) {
    ExecuteCreateTaskQueueAction(action.action_id(),
                                 action.create_task_queue());
  } else if (action.has_set_queue_priority()) {
    ExecuteSetQueuePriorityAction(action.action_id(),
                                  action.set_queue_priority());
  } else if (action.has_set_queue_enabled()) {
    ExecuteSetQueueEnabledAction(action.action_id(),
                                 action.set_queue_enabled());
  } else if (action.has_create_queue_voter()) {
    ExecuteCreateQueueVoterAction(action.action_id(),
                                  action.create_queue_voter());
  } else if (action.has_shutdown_task_queue()) {
    ExecuteShutdownTaskQueueAction(action.action_id(),
                                   action.shutdown_task_queue());
  } else if (action.has_cancel_task()) {
    ExecuteCancelTaskAction(action.action_id(), action.cancel_task());
  } else if (action.has_insert_fence()) {
    ExecuteInsertFenceAction(action.action_id(), action.insert_fence());
  } else if (action.has_remove_fence()) {
    ExecuteRemoveFenceAction(action.action_id(), action.remove_fence());
  } else {
    ExecutePostDelayedTaskAction(action.action_id(),
                                 action.post_delayed_task());
  }
}

void SequenceManagerFuzzerProcessor::ExecutePostDelayedTaskAction(
    uint64_t action_id,
    const SequenceManagerTestDescription::PostDelayedTaskAction& action) {
  DCHECK(!task_queues_.empty());

  LogActionForTesting(action_id, ActionForTest::ActionType::kPostDelayedTask,
                      test_task_runner_->GetMockTickClock()->NowTicks());

  size_t queue_index = action.task_queue_id() % task_queues_.size();
  TestTaskQueue* chosen_task_queue = task_queues_[queue_index].queue.get();

  std::unique_ptr<Task> pending_task = std::make_unique<Task>(this);

  // TODO(farahcharab) After adding non-nestable/nestable tasks, fix this to
  // PostNonNestableDelayedTask for the former and PostDelayedTask for the
  // latter.
  chosen_task_queue->PostDelayedTask(
      FROM_HERE,
      BindOnce(&Task::Execute, pending_task->weak_ptr_factory_.GetWeakPtr(),
               action.task()),
      TimeDelta::FromMilliseconds(action.delay_ms()));

  pending_tasks_.push_back(std::move(pending_task));
}

void SequenceManagerFuzzerProcessor::ExecuteCreateTaskQueueAction(
    uint64_t action_id,
    const SequenceManagerTestDescription::CreateTaskQueueAction& action) {
  LogActionForTesting(action_id, ActionForTest::ActionType::kCreateTaskQueue,
                      test_task_runner_->GetMockTickClock()->NowTicks());

  TaskQueue::Spec spec = TaskQueue::Spec("test_task_queue");
  task_queues_.emplace_back(manager_->CreateTaskQueue<TestTaskQueue>(spec));
  task_queues_.back().queue->SetQueuePriority(
      ToTaskQueuePriority(action.initial_priority()));
}

void SequenceManagerFuzzerProcessor::ExecuteSetQueuePriorityAction(
    uint64_t action_id,
    const SequenceManagerTestDescription::SetQueuePriorityAction& action) {
  DCHECK(!task_queues_.empty());

  LogActionForTesting(action_id, ActionForTest::ActionType::kSetQueuePriority,
                      test_task_runner_->GetMockTickClock()->NowTicks());

  size_t queue_index = action.task_queue_id() % task_queues_.size();
  TestTaskQueue* chosen_task_queue = task_queues_[queue_index].queue.get();

  chosen_task_queue->SetQueuePriority(ToTaskQueuePriority(action.priority()));
}

void SequenceManagerFuzzerProcessor::ExecuteSetQueueEnabledAction(
    uint64_t action_id,
    const SequenceManagerTestDescription::SetQueueEnabledAction& action) {
  DCHECK(!task_queues_.empty());

  LogActionForTesting(action_id, ActionForTest::ActionType::kSetQueueEnabled,
                      test_task_runner_->GetMockTickClock()->NowTicks());

  size_t queue_index = action.task_queue_id() % task_queues_.size();
  TestTaskQueue* chosen_task_queue = task_queues_[queue_index].queue.get();

  if (task_queues_[queue_index].voters.empty()) {
    task_queues_[queue_index].voters.push_back(
        chosen_task_queue->CreateQueueEnabledVoter());
  }

  size_t voter_index =
      action.voter_id() % task_queues_[queue_index].voters.size();
  task_queues_[queue_index].voters[voter_index]->SetQueueEnabled(
      action.enabled());
}

void SequenceManagerFuzzerProcessor::ExecuteCreateQueueVoterAction(
    uint64_t action_id,
    const SequenceManagerTestDescription::CreateQueueVoterAction& action) {
  LogActionForTesting(action_id, ActionForTest::ActionType::kCreateQueueVoter,
                      test_task_runner_->GetMockTickClock()->NowTicks());

  size_t queue_index = action.task_queue_id() % task_queues_.size();
  TestTaskQueue* chosen_task_queue = task_queues_[queue_index].queue.get();

  task_queues_[queue_index].voters.push_back(
      chosen_task_queue->CreateQueueEnabledVoter());
}

void SequenceManagerFuzzerProcessor::ExecuteShutdownTaskQueueAction(
    uint64_t action_id,
    const SequenceManagerTestDescription::ShutdownTaskQueueAction& action) {
  LogActionForTesting(action_id, ActionForTest::ActionType::kShutdownTaskQueue,
                      test_task_runner_->GetMockTickClock()->NowTicks());

  // We always want to have a default task queue.
  if (task_queues_.size() > 1) {
    size_t queue_index = action.task_queue_id() % task_queues_.size();
    task_queues_[queue_index].queue.get()->ShutdownTaskQueue();
    task_queues_.erase(task_queues_.begin() + queue_index);
  }
}

void SequenceManagerFuzzerProcessor::ExecuteCancelTaskAction(
    uint64_t action_id,
    const SequenceManagerTestDescription::CancelTaskAction& action) {
  LogActionForTesting(action_id, ActionForTest::ActionType::kCancelTask,
                      test_task_runner_->GetMockTickClock()->NowTicks());

  if (!pending_tasks_.empty()) {
    size_t task_index = action.task_id() % pending_tasks_.size();
    pending_tasks_[task_index]->weak_ptr_factory_.InvalidateWeakPtrs();

    // If it is already running, it is a parent task and will be deleted when
    // it is done.
    if (!pending_tasks_[task_index]->is_running) {
      pending_tasks_.erase(pending_tasks_.begin() + task_index);
    }
  }
}

void SequenceManagerFuzzerProcessor::ExecuteInsertFenceAction(
    uint64_t action_id,
    const SequenceManagerTestDescription::InsertFenceAction& action) {
  LogActionForTesting(action_id, ActionForTest::ActionType::kInsertFence,
                      test_task_runner_->GetMockTickClock()->NowTicks());

  size_t queue_index = action.task_queue_id() % task_queues_.size();

  if (action.position() ==
      SequenceManagerTestDescription::InsertFenceAction::NOW) {
    task_queues_[queue_index].queue.get()->InsertFence(
        TaskQueue::InsertFencePosition::kNow);
  } else {
    task_queues_[queue_index].queue.get()->InsertFence(
        TaskQueue::InsertFencePosition::kBeginningOfTime);
  }
}

void SequenceManagerFuzzerProcessor::ExecuteRemoveFenceAction(
    uint64_t action_id,
    const SequenceManagerTestDescription::RemoveFenceAction& action) {
  LogActionForTesting(action_id, ActionForTest::ActionType::kRemoveFence,
                      test_task_runner_->GetMockTickClock()->NowTicks());

  size_t queue_index = action.task_queue_id() % task_queues_.size();
  task_queues_[queue_index].queue.get()->RemoveFence();
}

void SequenceManagerFuzzerProcessor::ExecuteTask(
    const SequenceManagerTestDescription::Task& task) {
  TimeTicks start_time = test_task_runner_->GetMockTickClock()->NowTicks();

  // We can limit the depth of the nested post delayed action when processing
  // the proto.
  for (const auto& task_action : task.actions()) {
    // TODO(farahcharab) Add run loop to deal with nested tasks later. So far,
    // we are assuming tasks are non-nestable.
    RunAction(task_action);
  }

  TimeTicks end_time = test_task_runner_->GetMockTickClock()->NowTicks();

  test_task_runner_->AdvanceMockTickClock(
      TimeDelta::FromMilliseconds(task.duration_ms()) -
      (end_time - start_time));

  LogTaskForTesting(task.task_id(), start_time,
                    test_task_runner_->GetMockTickClock()->NowTicks());
}

void SequenceManagerFuzzerProcessor::DeleteTask(Task* task) {
  size_t i = 0;
  while (i < pending_tasks_.size() && task != pending_tasks_[i].get()) {
    i++;
  }
  pending_tasks_.erase(pending_tasks_.begin() + i);
}

void SequenceManagerFuzzerProcessor::LogTaskForTesting(uint64_t task_id,
                                                       TimeTicks start_time,
                                                       TimeTicks end_time) {
  if (!log_for_testing_)
    return;

  uint64_t start_time_ms = (start_time - initial_time_).InMilliseconds();
  uint64_t end_time_ms = (end_time - initial_time_).InMilliseconds();

  ordered_tasks_.emplace_back(task_id, start_time_ms, end_time_ms);
}

void SequenceManagerFuzzerProcessor::LogActionForTesting(
    uint64_t action_id,
    ActionForTest::ActionType type,
    TimeTicks start_time) {
  if (!log_for_testing_)
    return;

  ordered_actions_.emplace_back(action_id, type,
                                (start_time - initial_time_).InMilliseconds());
}

const std::vector<SequenceManagerFuzzerProcessor::TaskForTest>&
SequenceManagerFuzzerProcessor::ordered_tasks() const {
  return ordered_tasks_;
}

const std::vector<SequenceManagerFuzzerProcessor::ActionForTest>&
SequenceManagerFuzzerProcessor::ordered_actions() const {
  return ordered_actions_;
}

SequenceManagerFuzzerProcessor::Task::Task(
    SequenceManagerFuzzerProcessor* processor)
    : is_running(false), processor_(processor), weak_ptr_factory_(this) {}

void SequenceManagerFuzzerProcessor::Task::Execute(
    const SequenceManagerTestDescription::Task& task) {
  is_running = true;
  processor_->ExecuteTask(task);
  processor_->DeleteTask(this);
}

SequenceManagerFuzzerProcessor::TaskForTest::TaskForTest(uint64_t id,
                                                         uint64_t start,
                                                         uint64_t end)
    : task_id(id), start_time_ms(start), end_time_ms(end) {}

bool SequenceManagerFuzzerProcessor::TaskForTest::operator==(
    const TaskForTest& rhs) const {
  return task_id == rhs.task_id && start_time_ms == rhs.start_time_ms &&
         end_time_ms == rhs.end_time_ms;
}

SequenceManagerFuzzerProcessor::ActionForTest::ActionForTest(uint64_t id,
                                                             ActionType type,
                                                             uint64_t start)
    : action_id(id), action_type(type), start_time_ms(start) {}

bool SequenceManagerFuzzerProcessor::ActionForTest::operator==(
    const ActionForTest& rhs) const {
  return action_id == rhs.action_id && action_type == rhs.action_type &&
         start_time_ms == rhs.start_time_ms;
}

}  // namespace sequence_manager
}  // namespace base
