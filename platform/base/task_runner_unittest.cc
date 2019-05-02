// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <thread>  // NOLINT

#include "osp/impl/testing/fake_clock.h"
#include "platform/api/task_runner_factory.h"
#include "platform/api/time.h"
#include "platform/base/task_runner_impl.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {
using openscreen::platform::Clock;
using std::chrono::milliseconds;

const auto kTaskRunnerSleepTime = milliseconds(1);

void WaitUntilCondition(std::function<bool()> predicate) {
  while (!predicate()) {
    std::this_thread::sleep_for(kTaskRunnerSleepTime);
  }
}
}  // anonymous namespace

namespace openscreen {
namespace platform {

TEST(TaskRunnerTest, TaskRunnerFromFactoryExecutesTask) {
  FakeClock fake_clock{platform::Clock::time_point(milliseconds(1337))};
  auto runner = TaskRunnerFactory::Create(fake_clock.now);

  std::thread t([&runner] {
    static_cast<TaskRunnerImpl*>(runner.get())->RunUntilStopped();
  });

  std::string ran_tasks = "";
  const auto task = [&ran_tasks] { ran_tasks += "1"; };
  EXPECT_EQ(ran_tasks, "");

  runner->PostTask(task);

  WaitUntilCondition([&ran_tasks] { return ran_tasks == "1"; });
  EXPECT_EQ(ran_tasks, "1");

  static_cast<TaskRunnerImpl*>(runner.get())->RequestStopSoon();
  t.join();
}

TEST(TaskRunnerTest, TaskRunnerRunsDelayedTasksInOrder) {
  FakeClock fake_clock{platform::Clock::time_point(milliseconds(1337))};
  auto runner = TaskRunnerFactory::Create(fake_clock.now);

  std::thread t([&runner] {
    static_cast<TaskRunnerImpl*>(runner.get())->RunUntilStopped();
  });

  std::string ran_tasks = "";

  const auto kDelayTime = milliseconds(5);
  const auto task_one = [&ran_tasks] { ran_tasks += "1"; };
  runner->PostTaskWithDelay(task_one, kDelayTime);

  const auto task_two = [&ran_tasks] { ran_tasks += "2"; };
  runner->PostTaskWithDelay(task_two, kDelayTime * 2);

  EXPECT_EQ(ran_tasks, "");
  fake_clock.Advance(kDelayTime);
  WaitUntilCondition([&ran_tasks] { return ran_tasks == "1"; });
  EXPECT_EQ(ran_tasks, "1");

  fake_clock.Advance(kDelayTime);
  WaitUntilCondition([&ran_tasks] { return ran_tasks == "12"; });
  EXPECT_EQ(ran_tasks, "12");

  static_cast<TaskRunnerImpl*>(runner.get())->RequestStopSoon();
  t.join();
}

TEST(TaskRunnerTest, SingleThreadedTaskRunnerRunsSequentially) {
  FakeClock fake_clock{platform::Clock::time_point(milliseconds(1337))};
  TaskRunnerImpl runner(fake_clock.now);

  std::string ran_tasks;
  const auto task_one = [&ran_tasks] { ran_tasks += "1"; };
  const auto task_two = [&ran_tasks] { ran_tasks += "2"; };
  const auto task_three = [&ran_tasks] { ran_tasks += "3"; };
  const auto task_four = [&ran_tasks] { ran_tasks += "4"; };
  const auto task_five = [&ran_tasks] { ran_tasks += "5"; };

  runner.PostTask(task_one);
  runner.PostTask(task_two);
  runner.PostTask(task_three);
  runner.PostTask(task_four);
  runner.PostTask(task_five);
  EXPECT_EQ(ran_tasks, "");

  runner.RunUntilIdleForTesting();
  EXPECT_EQ(ran_tasks, "12345");
}

TEST(TaskRunnerTest, TaskRunnerCanStopRunning) {
  FakeClock fake_clock{platform::Clock::time_point(milliseconds(1337))};
  TaskRunnerImpl runner(fake_clock.now);

  std::string ran_tasks;
  const auto task_one = [&ran_tasks] { ran_tasks += "1"; };
  const auto task_two = [&ran_tasks] { ran_tasks += "2"; };

  runner.PostTask(task_one);
  EXPECT_EQ(ran_tasks, "");

  std::thread start_thread([&runner] { runner.RunUntilStopped(); });

  WaitUntilCondition([&ran_tasks] { return !ran_tasks.empty(); });
  EXPECT_EQ(ran_tasks, "1");

  // Since Stop is called first, and the single threaded task
  // runner should honor the queue, we know the task runner is not running
  // since task two doesn't get ran.
  runner.RequestStopSoon();
  runner.PostTask(task_two);
  EXPECT_EQ(ran_tasks, "1");

  start_thread.join();
}

TEST(TaskRunnerTest, StoppingDoesNotDeleteTasks) {
  FakeClock fake_clock{platform::Clock::time_point(milliseconds(1337))};
  TaskRunnerImpl runner(fake_clock.now);

  std::string ran_tasks;
  const auto task_one = [&ran_tasks] { ran_tasks += "1"; };

  runner.PostTask(task_one);
  runner.RequestStopSoon();

  EXPECT_EQ(ran_tasks, "");
  runner.RunUntilIdleForTesting();

  EXPECT_EQ(ran_tasks, "1");
}

TEST(TaskRunnerTest, TaskRunnerIsStableWithLotsOfTasks) {
  FakeClock fake_clock{platform::Clock::time_point(milliseconds(1337))};
  TaskRunnerImpl runner(fake_clock.now);

  const int kNumberOfTasks = 500;
  std::string expected_ran_tasks;
  expected_ran_tasks.append(kNumberOfTasks, '1');

  std::string ran_tasks;
  for (int i = 0; i < kNumberOfTasks; ++i) {
    const auto task = [&ran_tasks] { ran_tasks += "1"; };
    runner.PostTask(task);
  }

  runner.RunUntilIdleForTesting();
  EXPECT_EQ(ran_tasks, expected_ran_tasks);
}

TEST(TaskRunnerTest, TaskRunnerDelayedTasksDontBlockImmediateTasks) {
  TaskRunnerImpl runner(platform::Clock::now);

  std::string ran_tasks;
  const auto task = [&ran_tasks] { ran_tasks += "1"; };
  const auto delayed_task = [&ran_tasks] { ran_tasks += "A"; };

  runner.PostTaskWithDelay(delayed_task, milliseconds(10000));
  runner.PostTask(task);

  runner.RunUntilIdleForTesting();
  // The immediate task should have run, even though the delayed task
  // was added first.

  EXPECT_EQ(ran_tasks, "1");
}
}  // namespace platform
}  // namespace openscreen
