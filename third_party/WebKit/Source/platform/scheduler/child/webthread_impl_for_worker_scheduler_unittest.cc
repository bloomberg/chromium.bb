// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/webthread_impl_for_worker_scheduler.h"

#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "platform/WebTaskRunner.h"
#include "platform/scheduler/child/web_scheduler_impl.h"
#include "platform/scheduler/child/worker_scheduler_impl.h"
#include "public/platform/WebTraceLocation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::Invoke;

namespace blink {
namespace scheduler {
namespace {

class MockTask {
 public:
  MOCK_METHOD0(Run, void());
};

class MockIdleTask : public blink::WebThread::IdleTask {
 public:
  ~MockIdleTask() override {}

  MOCK_METHOD1(Run, void(double deadline));
};

class TestObserver : public blink::WebThread::TaskObserver {
 public:
  explicit TestObserver(std::string* calls) : calls_(calls) {}

  ~TestObserver() override {}

  void WillProcessTask() override { calls_->append(" willProcessTask"); }

  void DidProcessTask() override { calls_->append(" didProcessTask"); }

 private:
  std::string* calls_;  // NOT OWNED
};

void RunTestTask(std::string* calls) {
  calls->append(" run");
}

void AddTaskObserver(WebThreadImplForWorkerScheduler* thread,
                     TestObserver* observer) {
  thread->AddTaskObserver(observer);
}

void RemoveTaskObserver(WebThreadImplForWorkerScheduler* thread,
                        TestObserver* observer) {
  thread->RemoveTaskObserver(observer);
}

void ShutdownOnThread(WebThreadImplForWorkerScheduler* thread) {
  WebSchedulerImpl* web_scheduler_impl =
      static_cast<WebSchedulerImpl*>(thread->Scheduler());
  web_scheduler_impl->Shutdown();
}

}  // namespace

class WebThreadImplForWorkerSchedulerTest : public ::testing::Test {
 public:
  WebThreadImplForWorkerSchedulerTest() {}

  ~WebThreadImplForWorkerSchedulerTest() override {}

  void SetUp() override {
    thread_.reset(new WebThreadImplForWorkerScheduler("test thread"));
    thread_->Init();
  }

  void RunOnWorkerThread(const tracked_objects::Location& from_here,
                         const base::Closure& task) {
    base::WaitableEvent completion(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    thread_->GetTaskRunner()->PostTask(
        from_here,
        base::Bind(&WebThreadImplForWorkerSchedulerTest::RunOnWorkerThreadTask,
                   base::Unretained(this), task, &completion));
    completion.Wait();
  }

 protected:
  void RunOnWorkerThreadTask(const base::Closure& task,
                             base::WaitableEvent* completion) {
    task.Run();
    completion->Signal();
  }

  std::unique_ptr<WebThreadImplForWorkerScheduler> thread_;

  DISALLOW_COPY_AND_ASSIGN(WebThreadImplForWorkerSchedulerTest);
};

TEST_F(WebThreadImplForWorkerSchedulerTest, TestDefaultTask) {
  MockTask task;
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  EXPECT_CALL(task, Run());
  ON_CALL(task, Run()).WillByDefault(Invoke([&completion]() {
    completion.Signal();
  }));

  thread_->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&MockTask::Run, WTF::Unretained(&task)));
  completion.Wait();
}

TEST_F(WebThreadImplForWorkerSchedulerTest,
       TestTaskExecutedBeforeThreadDeletion) {
  MockTask task;
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  EXPECT_CALL(task, Run());
  ON_CALL(task, Run()).WillByDefault(Invoke([&completion]() {
    completion.Signal();
  }));

  thread_->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&MockTask::Run, WTF::Unretained(&task)));
  thread_.reset();
}

TEST_F(WebThreadImplForWorkerSchedulerTest, TestIdleTask) {
  std::unique_ptr<MockIdleTask> task(new MockIdleTask());
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  EXPECT_CALL(*task, Run(_));
  ON_CALL(*task, Run(_)).WillByDefault(Invoke([&completion](double) {
    completion.Signal();
  }));

  thread_->PostIdleTask(BLINK_FROM_HERE, task.release());
  // We need to post a wake-up task or idle work will never happen.
  thread_->GetWebTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE, WTF::Bind([] {}), TimeDelta::FromMilliseconds(50));

  completion.Wait();
}

TEST_F(WebThreadImplForWorkerSchedulerTest, TestTaskObserver) {
  std::string calls;
  TestObserver observer(&calls);

  RunOnWorkerThread(FROM_HERE,
                    base::Bind(&AddTaskObserver, thread_.get(), &observer));
  thread_->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&RunTestTask, WTF::Unretained(&calls)));
  RunOnWorkerThread(FROM_HERE,
                    base::Bind(&RemoveTaskObserver, thread_.get(), &observer));

  // We need to be careful what we test here.  We want to make sure the
  // observers are un in the expected order before and after the task.
  // Sometimes we get an internal scheduler task running before or after
  // TestTask as well. This is not a bug, and we need to make sure the test
  // doesn't fail when that happens.
  EXPECT_THAT(calls,
              ::testing::HasSubstr("willProcessTask run didProcessTask"));
}

TEST_F(WebThreadImplForWorkerSchedulerTest, TestShutdown) {
  MockTask task;
  MockTask delayed_task;

  EXPECT_CALL(task, Run()).Times(0);
  EXPECT_CALL(delayed_task, Run()).Times(0);

  RunOnWorkerThread(FROM_HERE, base::Bind(&ShutdownOnThread, thread_.get()));
  thread_->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&MockTask::Run, WTF::Unretained(&task)));
  thread_->GetWebTaskRunner()->PostDelayedTask(
      BLINK_FROM_HERE,
      WTF::Bind(&MockTask::Run, WTF::Unretained(&delayed_task)),
      TimeDelta::FromMilliseconds(50));
  thread_.reset();
}

}  // namespace scheduler
}  // namespace blink
