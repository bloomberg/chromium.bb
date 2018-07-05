#include "third_party/blink/renderer/platform/scheduler/base/sequence_manager_fuzzer_processor.h"

#include <algorithm>
#include <string>

#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/blink/renderer/platform/scheduler/base/test/sequence_manager_for_test.h"

namespace base {
namespace sequence_manager {

void SequenceManagerFuzzerProcessor::ParseAndRun(
    const SequenceManagerTestDescription& description) {
  SequenceManagerFuzzerProcessor processor(description);
  processor.RunTest();
}

SequenceManagerFuzzerProcessor::SequenceManagerFuzzerProcessor(
    const SequenceManagerTestDescription& description)
    : SequenceManagerFuzzerProcessor(description, false) {}

SequenceManagerFuzzerProcessor::SequenceManagerFuzzerProcessor(
    const SequenceManagerTestDescription& description,
    bool log_tasks_for_testing)
    : log_tasks_for_testing_(log_tasks_for_testing) {
  test_task_runner_ = WrapRefCounted(
      new TestMockTimeTaskRunner(TestMockTimeTaskRunner::Type::kBoundToThread));

  // A zero clock triggers some assertions.
  test_task_runner_->AdvanceMockTickClock(TimeDelta::FromMilliseconds(1));

  initial_time_ = test_task_runner_->GetMockTickClock()->NowTicks();

  manager_ =
      SequenceManagerForTest::Create(nullptr, ThreadTaskRunnerHandle::Get(),
                                     test_task_runner_->GetMockTickClock());

  TaskQueue::Spec spec = TaskQueue::Spec("default_task_queue");
  task_queues_.push_back(manager_->CreateTaskQueue<TestTaskQueue>(spec));

  for (const auto& task : description.initial_task()) {
    PostDelayedTaskFromAction(task);
  }
}

void SequenceManagerFuzzerProcessor::RunTest() {
  test_task_runner_->FastForwardUntilNoTasksRemain();
}

void SequenceManagerFuzzerProcessor::RunAction(
    const SequenceManagerTestDescription::Action& action) {
  if (action.has_create_task_queue()) {
    return CreateTaskQueueFromAction(action.create_task_queue());
  }

  return PostDelayedTaskFromAction(action.post_delayed_task());
}

void SequenceManagerFuzzerProcessor::PostDelayedTaskFromAction(
    const SequenceManagerTestDescription::PostDelayedTaskAction& action) {
  DCHECK(!task_queues_.empty());

  size_t index = action.task_queue_id() % task_queues_.size();
  TestTaskQueue* chosen_task_queue = task_queues_[index].get();

  // TODO(farahcharab) After adding non-nestable/nestable tasks, fix this to
  // PostNonnestableDelayedTask for the former and PostDelayedTask for the
  // latter.
  chosen_task_queue->PostDelayedTask(
      FROM_HERE,
      BindOnce(&SequenceManagerFuzzerProcessor::ExecuteTask, Unretained(this),
               action.task()),
      TimeDelta::FromMilliseconds(action.delay_ms()));
}

void SequenceManagerFuzzerProcessor::CreateTaskQueueFromAction(
    const SequenceManagerTestDescription::CreateTaskQueueAction& action) {
  TaskQueue::Spec spec = TaskQueue::Spec("test_task_queue");
  task_queues_.push_back(manager_->CreateTaskQueue<TestTaskQueue>(spec));
}

void SequenceManagerFuzzerProcessor::ExecuteTask(
    const SequenceManagerTestDescription::Task& task) {
  TimeTicks start_time = test_task_runner_->GetMockTickClock()->NowTicks();

  // We can limit the depth of the nested post delayed action when processing
  // the proto.
  for (const auto& task_action : task.action()) {
    // TODO(farahcharab) Add run loop to deal with nested tasks later. So far,
    // we are assuming tasks are non-nestable.
    RunAction(task_action);
  }

  TimeTicks end_time = test_task_runner_->GetMockTickClock()->NowTicks();

  test_task_runner_->AdvanceMockTickClock(
      TimeDelta::FromMilliseconds(task.duration_ms()) -
      (end_time - start_time));

  if (log_tasks_for_testing_) {
    uint64_t start_time_ms = (start_time - initial_time_).InMilliseconds();
    uint64_t end_time_ms =
        (test_task_runner_->GetMockTickClock()->NowTicks() - initial_time_)
            .InMilliseconds();

    ordered_tasks_.emplace_back(task.task_id(), start_time_ms, end_time_ms);
  }
}

const std::vector<SequenceManagerFuzzerProcessor::TaskForTest>&
SequenceManagerFuzzerProcessor::ordered_tasks() const {
  return ordered_tasks_;
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

}  // namespace sequence_manager
}  // namespace base
