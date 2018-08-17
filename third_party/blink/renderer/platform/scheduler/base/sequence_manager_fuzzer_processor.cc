#include "third_party/blink/renderer/platform/scheduler/base/sequence_manager_fuzzer_processor.h"

#include "third_party/blink/renderer/platform/scheduler/base/simple_thread_impl.h"
#include "third_party/blink/renderer/platform/scheduler/base/thread_data.h"
#include "third_party/blink/renderer/platform/scheduler/base/thread_pool_manager.h"

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
    : log_for_testing_(log_for_testing),
      initial_time_(TimeTicks() + TimeDelta::FromMilliseconds(1)),
      thread_pool_manager_(std::make_unique<ThreadPoolManager>(this)),
      main_thread_data_(std::make_unique<ThreadData>(initial_time_)) {}

SequenceManagerFuzzerProcessor::~SequenceManagerFuzzerProcessor() = default;

void SequenceManagerFuzzerProcessor::RunTest(
    const SequenceManagerTestDescription& description) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_data_->thread_checker_);
  for (const auto& initial_action : description.main_thread_actions()) {
    ExecuteCreateThreadAction(main_thread_data_.get(),
                              initial_action.action_id(),
                              initial_action.create_thread());
  }

  thread_pool_manager_->StartInitialThreads();

  thread_pool_manager_->WaitForAllThreads();

  if (log_for_testing_) {
    ordered_actions_.emplace_back(main_thread_data_->ordered_actions_);
    ordered_tasks_.emplace_back(main_thread_data_->ordered_tasks_);

    for (auto&& thread : (thread_pool_manager_->threads())) {
      DCHECK(thread->thread_data());
      ordered_actions_.emplace_back(thread->thread_data()->ordered_actions_);
      ordered_tasks_.emplace_back(thread->thread_data()->ordered_tasks_);
    }
  }
}

void SequenceManagerFuzzerProcessor::RunAction(
    ThreadData* thread_data,
    const SequenceManagerTestDescription::Action& action) {
  if (action.has_create_task_queue()) {
    ExecuteCreateTaskQueueAction(thread_data, action.action_id(),
                                 action.create_task_queue());
  } else if (action.has_set_queue_priority()) {
    ExecuteSetQueuePriorityAction(thread_data, action.action_id(),
                                  action.set_queue_priority());
  } else if (action.has_set_queue_enabled()) {
    ExecuteSetQueueEnabledAction(thread_data, action.action_id(),
                                 action.set_queue_enabled());
  } else if (action.has_create_queue_voter()) {
    ExecuteCreateQueueVoterAction(thread_data, action.action_id(),
                                  action.create_queue_voter());
  } else if (action.has_shutdown_task_queue()) {
    ExecuteShutdownTaskQueueAction(thread_data, action.action_id(),
                                   action.shutdown_task_queue());
  } else if (action.has_cancel_task()) {
    ExecuteCancelTaskAction(thread_data, action.action_id(),
                            action.cancel_task());
  } else if (action.has_insert_fence()) {
    ExecuteInsertFenceAction(thread_data, action.action_id(),
                             action.insert_fence());
  } else if (action.has_remove_fence()) {
    ExecuteRemoveFenceAction(thread_data, action.action_id(),
                             action.remove_fence());
  } else if (action.has_create_thread()) {
    ExecuteCreateThreadAction(thread_data, action.action_id(),
                              action.create_thread());
  } else {
    ExecutePostDelayedTaskAction(thread_data, action.action_id(),
                                 action.post_delayed_task());
  }
}

void SequenceManagerFuzzerProcessor::ExecuteCreateThreadAction(
    ThreadData* thread_data,
    uint64_t action_id,
    const SequenceManagerTestDescription::CreateThreadAction& action) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_data->thread_checker_);

  LogActionForTesting(
      thread_data, action_id, ActionForTest::ActionType::kCreateThread,
      thread_data->test_task_runner_->GetMockTickClock()->NowTicks());

  thread_pool_manager_->CreateThread(
      action.initial_thread_actions(),
      thread_data->test_task_runner_->GetMockTickClock()->NowTicks());
}

void SequenceManagerFuzzerProcessor::ExecuteThread(
    ThreadData* thread_data,
    const google::protobuf::RepeatedPtrField<
        SequenceManagerTestDescription::Action>& initial_thread_actions) {
  for (const auto& initial_thread_action : initial_thread_actions) {
    RunAction(thread_data, initial_thread_action);
  }

  while (thread_data->test_task_runner_->GetMockTickClock()->NowTicks() <
         TimeTicks::Max()) {
    RunLoop().RunUntilIdle();
    thread_pool_manager_->AdvanceClockSynchronouslyByPendingTaskDelay(
        thread_data);
  }

  RunLoop().RunUntilIdle();
  thread_pool_manager_->ThreadDone();
}

void SequenceManagerFuzzerProcessor::ExecutePostDelayedTaskAction(
    ThreadData* thread_data,
    uint64_t action_id,
    const SequenceManagerTestDescription::PostDelayedTaskAction& action) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_data->thread_checker_);
  DCHECK(!thread_data->task_queues_.empty());

  LogActionForTesting(
      thread_data, action_id, ActionForTest::ActionType::kPostDelayedTask,
      thread_data->test_task_runner_->GetMockTickClock()->NowTicks());

  size_t queue_index =
      action.task_queue_id() % thread_data->task_queues_.size();
  TestTaskQueue* chosen_task_queue =
      thread_data->task_queues_[queue_index].queue.get();

  std::unique_ptr<Task> pending_task =
      std::make_unique<Task>(thread_data, this);

  // TODO(farahcharab) After adding non-nestable/nestable tasks, fix this to
  // PostNonNestableDelayedTask for the former and PostDelayedTask for the
  // latter.
  chosen_task_queue->PostDelayedTask(
      FROM_HERE,
      BindOnce(&Task::Execute, pending_task->weak_ptr_factory_.GetWeakPtr(),
               action.task()),
      TimeDelta::FromMilliseconds(action.delay_ms()));

  thread_data->pending_tasks_.push_back(std::move(pending_task));
}

void SequenceManagerFuzzerProcessor::ExecuteCreateTaskQueueAction(
    ThreadData* thread_data,
    uint64_t action_id,
    const SequenceManagerTestDescription::CreateTaskQueueAction& action) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_data->thread_checker_);

  LogActionForTesting(
      thread_data, action_id, ActionForTest::ActionType::kCreateTaskQueue,
      thread_data->test_task_runner_->GetMockTickClock()->NowTicks());

  TaskQueue::Spec spec = TaskQueue::Spec("test_task_queue");
  thread_data->task_queues_.emplace_back(
      thread_data->manager_->CreateTaskQueue<TestTaskQueue>(spec));
  thread_data->task_queues_.back().queue->SetQueuePriority(
      ToTaskQueuePriority(action.initial_priority()));
}

void SequenceManagerFuzzerProcessor::ExecuteSetQueuePriorityAction(
    ThreadData* thread_data,
    uint64_t action_id,
    const SequenceManagerTestDescription::SetQueuePriorityAction& action) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_data->thread_checker_);
  DCHECK(!thread_data->task_queues_.empty());

  LogActionForTesting(
      thread_data, action_id, ActionForTest::ActionType::kSetQueuePriority,
      thread_data->test_task_runner_->GetMockTickClock()->NowTicks());

  size_t queue_index =
      action.task_queue_id() % thread_data->task_queues_.size();
  TestTaskQueue* chosen_task_queue =
      thread_data->task_queues_[queue_index].queue.get();

  chosen_task_queue->SetQueuePriority(ToTaskQueuePriority(action.priority()));
}

void SequenceManagerFuzzerProcessor::ExecuteSetQueueEnabledAction(
    ThreadData* thread_data,
    uint64_t action_id,
    const SequenceManagerTestDescription::SetQueueEnabledAction& action) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_data->thread_checker_);
  DCHECK(!thread_data->task_queues_.empty());

  LogActionForTesting(
      thread_data, action_id, ActionForTest::ActionType::kSetQueueEnabled,
      thread_data->test_task_runner_->GetMockTickClock()->NowTicks());

  size_t queue_index =
      action.task_queue_id() % thread_data->task_queues_.size();
  TestTaskQueue* chosen_task_queue =
      thread_data->task_queues_[queue_index].queue.get();

  if (thread_data->task_queues_[queue_index].voters.empty()) {
    thread_data->task_queues_[queue_index].voters.push_back(
        chosen_task_queue->CreateQueueEnabledVoter());
  }

  size_t voter_index =
      action.voter_id() % thread_data->task_queues_[queue_index].voters.size();
  thread_data->task_queues_[queue_index].voters[voter_index]->SetQueueEnabled(
      action.enabled());
}

void SequenceManagerFuzzerProcessor::ExecuteCreateQueueVoterAction(
    ThreadData* thread_data,
    uint64_t action_id,
    const SequenceManagerTestDescription::CreateQueueVoterAction& action) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_data->thread_checker_);
  DCHECK(!thread_data->task_queues_.empty());

  LogActionForTesting(
      thread_data, action_id, ActionForTest::ActionType::kCreateQueueVoter,
      thread_data->test_task_runner_->GetMockTickClock()->NowTicks());

  size_t queue_index =
      action.task_queue_id() % thread_data->task_queues_.size();
  TestTaskQueue* chosen_task_queue =
      thread_data->task_queues_[queue_index].queue.get();

  thread_data->task_queues_[queue_index].voters.push_back(
      chosen_task_queue->CreateQueueEnabledVoter());
}

void SequenceManagerFuzzerProcessor::ExecuteShutdownTaskQueueAction(
    ThreadData* thread_data,
    uint64_t action_id,
    const SequenceManagerTestDescription::ShutdownTaskQueueAction& action) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_data->thread_checker_);
  DCHECK(!thread_data->task_queues_.empty());

  LogActionForTesting(
      thread_data, action_id, ActionForTest::ActionType::kShutdownTaskQueue,
      thread_data->test_task_runner_->GetMockTickClock()->NowTicks());

  // We always want to have a default task queue.
  if (thread_data->task_queues_.size() > 1) {
    size_t queue_index =
        action.task_queue_id() % thread_data->task_queues_.size();
    thread_data->task_queues_[queue_index].queue.get()->ShutdownTaskQueue();
    thread_data->task_queues_.erase(thread_data->task_queues_.begin() +
                                    queue_index);
  }
}

void SequenceManagerFuzzerProcessor::ExecuteCancelTaskAction(
    ThreadData* thread_data,
    uint64_t action_id,
    const SequenceManagerTestDescription::CancelTaskAction& action) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_data->thread_checker_);

  LogActionForTesting(
      thread_data, action_id, ActionForTest::ActionType::kCancelTask,
      thread_data->test_task_runner_->GetMockTickClock()->NowTicks());

  if (!thread_data->pending_tasks_.empty()) {
    size_t task_index = action.task_id() % thread_data->pending_tasks_.size();
    thread_data->pending_tasks_[task_index]
        ->weak_ptr_factory_.InvalidateWeakPtrs();

    // If it is already running, it is a parent task and will be deleted when
    // it is done.
    if (!thread_data->pending_tasks_[task_index]->is_running) {
      thread_data->pending_tasks_.erase(thread_data->pending_tasks_.begin() +
                                        task_index);
    }
  }
}

void SequenceManagerFuzzerProcessor::ExecuteInsertFenceAction(
    ThreadData* thread_data,
    uint64_t action_id,
    const SequenceManagerTestDescription::InsertFenceAction& action) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_data->thread_checker_);
  DCHECK(!thread_data->task_queues_.empty());

  LogActionForTesting(
      thread_data, action_id, ActionForTest::ActionType::kInsertFence,
      thread_data->test_task_runner_->GetMockTickClock()->NowTicks());

  size_t queue_index =
      action.task_queue_id() % thread_data->task_queues_.size();

  if (action.position() ==
      SequenceManagerTestDescription::InsertFenceAction::NOW) {
    thread_data->task_queues_[queue_index].queue.get()->InsertFence(
        TaskQueue::InsertFencePosition::kNow);
  } else {
    thread_data->task_queues_[queue_index].queue.get()->InsertFence(
        TaskQueue::InsertFencePosition::kBeginningOfTime);
  }
}

void SequenceManagerFuzzerProcessor::ExecuteRemoveFenceAction(
    ThreadData* thread_data,
    uint64_t action_id,
    const SequenceManagerTestDescription::RemoveFenceAction& action) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_data->thread_checker_);
  DCHECK(!thread_data->task_queues_.empty());

  LogActionForTesting(
      thread_data, action_id, ActionForTest::ActionType::kRemoveFence,
      thread_data->test_task_runner_->GetMockTickClock()->NowTicks());

  size_t queue_index =
      action.task_queue_id() % thread_data->task_queues_.size();
  thread_data->task_queues_[queue_index].queue.get()->RemoveFence();
}

void SequenceManagerFuzzerProcessor::ExecuteTask(
    ThreadData* thread_data,
    const SequenceManagerTestDescription::Task& task) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_data->thread_checker_);

  TimeTicks start_time =
      thread_data->test_task_runner_->GetMockTickClock()->NowTicks();

  // We can limit the depth of the nested post delayed action when processing
  // the proto.
  for (const auto& task_action : task.actions()) {
    // TODO(farahcharab) Add run loop to deal with nested tasks later. So far,
    // we are assuming tasks are non-nestable.
    RunAction(thread_data, task_action);
  }

  TimeTicks end_time =
      thread_data->test_task_runner_->GetMockTickClock()->NowTicks();

  TimeTicks next_time =
      start_time +
      std::max(TimeDelta(), TimeDelta::FromMilliseconds(task.duration_ms()) -
                                (end_time - start_time));

  while (thread_data->test_task_runner_->GetMockTickClock()->NowTicks() !=
         next_time) {
    thread_pool_manager_->AdvanceClockSynchronouslyToTime(thread_data,
                                                          next_time);
  }

  LogTaskForTesting(
      thread_data, task.task_id(), start_time,
      thread_data->test_task_runner_->GetMockTickClock()->NowTicks());
}

void SequenceManagerFuzzerProcessor::DeleteTask(Task* task) {
  DCHECK_CALLED_ON_VALID_THREAD(task->thread_data_->thread_checker_);

  size_t i = 0;
  std::vector<std::unique_ptr<Task>>* pending_tasks =
      &(task->thread_data_->pending_tasks_);

  while (i < pending_tasks->size() && task != (*pending_tasks)[i].get()) {
    i++;
  }
  pending_tasks->erase(pending_tasks->begin() + i);
}

void SequenceManagerFuzzerProcessor::LogTaskForTesting(ThreadData* thread_data,
                                                       uint64_t task_id,
                                                       TimeTicks start_time,
                                                       TimeTicks end_time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_data->thread_checker_);

  if (!log_for_testing_)
    return;

  uint64_t start_time_ms = (start_time - initial_time_).InMilliseconds();
  uint64_t end_time_ms = (end_time - initial_time_).InMilliseconds();

  thread_data->ordered_tasks_.emplace_back(task_id, start_time_ms, end_time_ms);
}

void SequenceManagerFuzzerProcessor::LogActionForTesting(
    ThreadData* thread_data,
    uint64_t action_id,
    ActionForTest::ActionType type,
    TimeTicks start_time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_data->thread_checker_);

  if (!log_for_testing_)
    return;

  thread_data->ordered_actions_.emplace_back(
      action_id, type, (start_time - initial_time_).InMilliseconds());
}

const std::vector<std::vector<SequenceManagerFuzzerProcessor::TaskForTest>>&
SequenceManagerFuzzerProcessor::ordered_tasks() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_data_->thread_checker_);
  return ordered_tasks_;
}

const std::vector<std::vector<SequenceManagerFuzzerProcessor::ActionForTest>>&
SequenceManagerFuzzerProcessor::ordered_actions() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_data_->thread_checker_);
  return ordered_actions_;
}

SequenceManagerFuzzerProcessor::Task::Task(
    ThreadData* thread_data,
    SequenceManagerFuzzerProcessor* processor)
    : is_running(false),
      processor_(processor),
      thread_data_(thread_data),
      weak_ptr_factory_(this) {
  DCHECK(processor_);
  DCHECK(thread_data_);
}

void SequenceManagerFuzzerProcessor::Task::Execute(
    const SequenceManagerTestDescription::Task& task) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_data_->thread_checker_);
  is_running = true;
  processor_->ExecuteTask(thread_data_, task);
  processor_->DeleteTask(this);
}

SequenceManagerFuzzerProcessor::TaskForTest::TaskForTest(uint64_t id,
                                                         uint64_t start_time_ms,
                                                         uint64_t end_time_ms)
    : task_id(id), start_time_ms(start_time_ms), end_time_ms(end_time_ms) {}

bool SequenceManagerFuzzerProcessor::TaskForTest::operator==(
    const TaskForTest& rhs) const {
  return task_id == rhs.task_id && start_time_ms == rhs.start_time_ms &&
         end_time_ms == rhs.end_time_ms;
}

SequenceManagerFuzzerProcessor::ActionForTest::ActionForTest(
    uint64_t id,
    ActionType type,
    uint64_t start_time_ms)
    : action_id(id), action_type(type), start_time_ms(start_time_ms) {}

bool SequenceManagerFuzzerProcessor::ActionForTest::operator==(
    const ActionForTest& rhs) const {
  return action_id == rhs.action_id && action_type == rhs.action_type &&
         start_time_ms == rhs.start_time_ms;
}

}  // namespace sequence_manager
}  // namespace base
