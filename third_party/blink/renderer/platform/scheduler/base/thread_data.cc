#include "third_party/blink/renderer/platform/scheduler/base/thread_data.h"

#include "base/task/sequence_manager/test/test_task_queue.h"
#include "base/threading/thread_task_runner_handle.h"

namespace base {
namespace sequence_manager {

ThreadData::ThreadData(TimeTicks initial_time) {
  test_task_runner_ = WrapRefCounted(
      new TestMockTimeTaskRunner(TestMockTimeTaskRunner::Type::kBoundToThread));

  DCHECK(!(initial_time - TimeTicks()).is_zero())
      << "A zero clock is not allowed as empty TimeTicks have a special value "
         "(i.e. base::TimeTicks::is_null())";

  test_task_runner_->AdvanceMockTickClock(initial_time - TimeTicks());

  manager_ =
      SequenceManagerForTest::Create(nullptr, ThreadTaskRunnerHandle::Get(),
                                     test_task_runner_->GetMockTickClock());

  TaskQueue::Spec spec = TaskQueue::Spec("default_task_queue");
  task_queues_.emplace_back(manager_->CreateTaskQueue<TestTaskQueue>(spec));
}

ThreadData::~ThreadData() = default;

}  // namespace sequence_manager
}  // namespace base
