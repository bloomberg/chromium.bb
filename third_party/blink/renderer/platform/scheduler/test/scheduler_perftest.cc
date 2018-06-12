// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_mock_time_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"
#include "third_party/blink/renderer/platform/scheduler/base/test/task_queue_manager_for_test.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_metrics_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

namespace blink {
namespace scheduler {

class FrameSchedulerForTest : public FrameSchedulerImpl {
 public:
  FrameSchedulerForTest(MainThreadSchedulerImpl* main_thread_scheduler,
                        PageSchedulerImpl* page_scheduler)
      : FrameSchedulerImpl(main_thread_scheduler,
                           page_scheduler,
                           nullptr,
                           FrameScheduler::FrameType::kMainFrame) {}

  using FrameSchedulerImpl::GetTaskQueueForTesting;
};

class SchedulerPerfTest : public testing::Test {
 protected:
  void SetUp() override {
    message_loop_ = std::make_unique<base::MessageLoop>();
    main_thread_scheduler_ = std::make_unique<MainThreadSchedulerImpl>(
        base::sequence_manager::TaskQueueManagerForTest::Create(
            message_loop_.get(), message_loop_->task_runner(),
            base::DefaultTickClock::GetInstance()),
        base::nullopt);
    metrics_helper_ = std::make_unique<MainThreadMetricsHelper>(
        main_thread_scheduler_.get(), base::TimeTicks::Now(), false);
    page_scheduler_ = std::make_unique<PageSchedulerImpl>(
        nullptr, main_thread_scheduler_.get(), false);
    frame_scheduler_ = std::make_unique<FrameSchedulerForTest>(
        main_thread_scheduler_.get(), page_scheduler_.get());
  }

  std::unique_ptr<base::MessageLoop> message_loop_;
  std::unique_ptr<MainThreadSchedulerImpl> main_thread_scheduler_;
  std::unique_ptr<MainThreadMetricsHelper> metrics_helper_;
  std::unique_ptr<PageSchedulerImpl> page_scheduler_;
  std::unique_ptr<FrameSchedulerForTest> frame_scheduler_;
};

void Measure(base::OnceCallback<void(size_t)> task,
             size_t num_iterations,
             std::string name) {
  base::TimeTicks start = base::TimeTicks::Now();
  base::ThreadTicks start_thread = base::ThreadTicks::Now();

  std::move(task).Run(num_iterations);

  base::TimeTicks now = base::TimeTicks::Now();
  base::ThreadTicks now_thread = base::ThreadTicks::Now();

  perf_test::PrintResult(
      "task", "", name + " (wall time)",
      (now - start).InMicroseconds() / static_cast<double>(num_iterations),
      "us/run", true);
  perf_test::PrintResult("task", "", name + " (thread time)",
                         (now_thread - start_thread).InMicroseconds() /
                             static_cast<double>(num_iterations),
                         "us/run", true);
}

void RunChainedTask(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                    size_t iterations) {
  if (iterations <= 1)
    return;
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&RunChainedTask, task_runner, iterations - 1));
}

TEST_F(SchedulerPerfTest, MeasureTimeCost) {
  const size_t num_iterations = 1e5;

  Measure(base::BindOnce([](size_t num_iterations) {
            base::TimeTicks now;
            for (size_t i = 0; i < num_iterations; ++i) {
              now = base::TimeTicks::Now();
            }
          }),
          num_iterations, "Measure wall time measurement cost");

  Measure(base::BindOnce([](size_t num_iterations) {
            base::ThreadTicks now;
            for (size_t i = 0; i < num_iterations; ++i) {
              now = base::ThreadTicks::Now();
            }
          }),
          num_iterations, "Measure thread time measurement cost");

  Measure(
      base::BindOnce(
          [](MainThreadMetricsHelper* metrics_helper,
             FrameSchedulerForTest* frame_scheduler, size_t num_iterations) {
            base::sequence_manager::TaskQueue::Task task(
                base::sequence_manager::TaskQueue::PostedTask(
                    base::OnceClosure(), FROM_HERE),
                base::TimeTicks());

            base::TimeTicks now =
                base::TimeTicks() + base::TimeDelta::FromSeconds(1);
            base::TimeDelta delta = base::TimeDelta::FromMilliseconds(10);

            scoped_refptr<MainThreadTaskQueue> queue =
                frame_scheduler->GetTaskQueueForTesting();

            for (size_t i = 0; i < num_iterations; ++i) {
              metrics_helper->RecordTaskMetrics(queue.get(), task, now,
                                                now + delta, base::nullopt);
              now += delta;
            }
          },
          base::Unretained(metrics_helper_.get()),
          base::Unretained(frame_scheduler_.get())),
      num_iterations, "Measure per-task metric cost");

  Measure(
      base::BindOnce(
          [](scoped_refptr<MainThreadTaskQueue> queue, size_t num_iterations) {
            queue->PostTask(FROM_HERE, base::BindOnce(&RunChainedTask, queue,
                                                      num_iterations));
            base::RunLoop().RunUntilIdle();
          },
          frame_scheduler_->GetTaskQueueForTesting()),
      num_iterations, "Measure a total cost of running a task");

  DCHECK(false);
}

}  // namespace scheduler
}  // namespace blink
