// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/webthread_impl_for_renderer_scheduler.h"

#include <stddef.h>
#include <memory>

#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "platform/WebTaskRunner.h"
#include "platform/scheduler/base/test_time_source.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/test/create_task_queue_manager_for_test.h"
#include "public/platform/WebTraceLocation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace webthread_impl_for_renderer_scheduler_unittest {

const int kWorkBatchSize = 2;

class MockTask {
 public:
  MOCK_METHOD0(Run, void());
};

class MockTaskObserver : public blink::WebThread::TaskObserver {
 public:
  MOCK_METHOD0(WillProcessTask, void());
  MOCK_METHOD0(DidProcessTask, void());
};

class WebThreadImplForRendererSchedulerTest : public ::testing::Test {
 public:
  WebThreadImplForRendererSchedulerTest() {}

  void SetUp() override {
    clock_.reset(new base::SimpleTestTickClock());
    clock_->Advance(base::TimeDelta::FromMicroseconds(5000));
    scheduler_.reset(
        new RendererSchedulerImpl(CreateTaskQueueManagerWithUnownedClockForTest(
            &message_loop_, message_loop_.task_runner(), clock_.get())));
    default_task_runner_ = scheduler_->DefaultTaskQueue();
    thread_ = scheduler_->CreateMainThread();
  }

  ~WebThreadImplForRendererSchedulerTest() override {}

  void SetWorkBatchSizeForTesting(size_t work_batch_size) {
    scheduler_->GetSchedulerHelperForTesting()->SetWorkBatchSizeForTesting(
        work_batch_size);
  }

  void TearDown() override { scheduler_->Shutdown(); }

 protected:
  base::MessageLoop message_loop_;
  std::unique_ptr<base::SimpleTestTickClock> clock_;
  std::unique_ptr<RendererSchedulerImpl> scheduler_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  std::unique_ptr<blink::WebThread> thread_;

  DISALLOW_COPY_AND_ASSIGN(WebThreadImplForRendererSchedulerTest);
};

TEST_F(WebThreadImplForRendererSchedulerTest, TestTaskObserver) {
  MockTaskObserver observer;
  thread_->AddTaskObserver(&observer);
  MockTask task;

  {
    ::testing::InSequence sequence;
    EXPECT_CALL(observer, WillProcessTask());
    EXPECT_CALL(task, Run());
    EXPECT_CALL(observer, DidProcessTask());
  }

  thread_->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&MockTask::Run, WTF::Unretained(&task)));
  base::RunLoop().RunUntilIdle();
  thread_->RemoveTaskObserver(&observer);
}

TEST_F(WebThreadImplForRendererSchedulerTest, TestWorkBatchWithOneTask) {
  MockTaskObserver observer;
  thread_->AddTaskObserver(&observer);
  MockTask task;

  SetWorkBatchSizeForTesting(kWorkBatchSize);
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(observer, WillProcessTask());
    EXPECT_CALL(task, Run());
    EXPECT_CALL(observer, DidProcessTask());
  }

  thread_->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&MockTask::Run, WTF::Unretained(&task)));
  base::RunLoop().RunUntilIdle();
  thread_->RemoveTaskObserver(&observer);
}

TEST_F(WebThreadImplForRendererSchedulerTest, TestWorkBatchWithTwoTasks) {
  MockTaskObserver observer;
  thread_->AddTaskObserver(&observer);
  MockTask task1;
  MockTask task2;

  SetWorkBatchSizeForTesting(kWorkBatchSize);
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(observer, WillProcessTask());
    EXPECT_CALL(task1, Run());
    EXPECT_CALL(observer, DidProcessTask());

    EXPECT_CALL(observer, WillProcessTask());
    EXPECT_CALL(task2, Run());
    EXPECT_CALL(observer, DidProcessTask());
  }

  thread_->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&MockTask::Run, WTF::Unretained(&task1)));
  thread_->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&MockTask::Run, WTF::Unretained(&task2)));
  base::RunLoop().RunUntilIdle();
  thread_->RemoveTaskObserver(&observer);
}

TEST_F(WebThreadImplForRendererSchedulerTest, TestWorkBatchWithThreeTasks) {
  MockTaskObserver observer;
  thread_->AddTaskObserver(&observer);
  MockTask task1;
  MockTask task2;
  MockTask task3;

  SetWorkBatchSizeForTesting(kWorkBatchSize);
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(observer, WillProcessTask());
    EXPECT_CALL(task1, Run());
    EXPECT_CALL(observer, DidProcessTask());

    EXPECT_CALL(observer, WillProcessTask());
    EXPECT_CALL(task2, Run());
    EXPECT_CALL(observer, DidProcessTask());

    EXPECT_CALL(observer, WillProcessTask());
    EXPECT_CALL(task3, Run());
    EXPECT_CALL(observer, DidProcessTask());
  }

  thread_->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&MockTask::Run, WTF::Unretained(&task1)));
  thread_->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&MockTask::Run, WTF::Unretained(&task2)));
  thread_->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&MockTask::Run, WTF::Unretained(&task3)));
  base::RunLoop().RunUntilIdle();
  thread_->RemoveTaskObserver(&observer);
}

void EnterRunLoop(base::MessageLoop* message_loop, blink::WebThread* thread) {
  // Note: WebThreads do not support nested run loops, which is why we use a
  // run loop directly.
  base::RunLoop run_loop;
  thread->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE,
      WTF::Bind(&base::RunLoop::Quit, WTF::Unretained(&run_loop)));
  message_loop->SetNestableTasksAllowed(true);
  run_loop.Run();
}

TEST_F(WebThreadImplForRendererSchedulerTest, TestNestedRunLoop) {
  MockTaskObserver observer;
  thread_->AddTaskObserver(&observer);

  {
    ::testing::InSequence sequence;

    // One callback for EnterRunLoop.
    EXPECT_CALL(observer, WillProcessTask());

    // A pair for ExitRunLoopTask.
    EXPECT_CALL(observer, WillProcessTask());
    EXPECT_CALL(observer, DidProcessTask());

    // A final callback for EnterRunLoop.
    EXPECT_CALL(observer, DidProcessTask());
  }

  message_loop_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&EnterRunLoop, base::Unretained(&message_loop_),
                            base::Unretained(thread_.get())));
  base::RunLoop().RunUntilIdle();
  thread_->RemoveTaskObserver(&observer);
}

}  // namespace webthread_impl_for_renderer_scheduler_unittest
}  // namespace scheduler
}  // namespace blink
