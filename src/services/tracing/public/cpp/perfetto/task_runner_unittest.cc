// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/task_runner.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

namespace {

class TaskDestination {
 public:
  TaskDestination(size_t number_of_sequences,
                  size_t expected_tasks,
                  base::OnceClosure on_complete)
      : expected_tasks_(expected_tasks),
        on_complete_(std::move(on_complete)),
        last_task_id_(number_of_sequences),
        weak_ptr_factory_(this) {}

  size_t tasks_run() const { return tasks_run_; }

  void TestTask(int n, size_t sequence_number = 0) {
    EXPECT_LT(sequence_number, last_task_id_.size());
    EXPECT_GT(expected_tasks_, tasks_run_);
    EXPECT_GE(n, last_task_id_[sequence_number]);
    last_task_id_[sequence_number] = n;

    if (++tasks_run_ == expected_tasks_) {
      std::move(on_complete_).Run();
    }
  }

  base::WeakPtr<TaskDestination> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const size_t expected_tasks_;
  base::OnceClosure on_complete_;
  std::vector<int> last_task_id_;
  size_t tasks_run_ = 0;

  base::WeakPtrFactory<TaskDestination> weak_ptr_factory_;
};

class PosterThread : public base::SimpleThread {
 public:
  PosterThread(PerfettoTaskRunner* task_runner,
               base::WeakPtr<TaskDestination> weak_ptr,
               int n,
               size_t sequence_number)
      : SimpleThread("TaskPostThread"),
        task_runner_(task_runner),
        weak_ptr_(weak_ptr),
        n_(n),
        sequence_number_(sequence_number) {}
  ~PosterThread() override {}

  // base::SimpleThread overrides.
  void BeforeStart() override {}
  void BeforeJoin() override {}

  void Run() override {
    task_runner_->BlockPostTaskForThread();

    for (int i = 0; i < n_; ++i) {
      auto weak_ptr = weak_ptr_;
      auto sequence_number = sequence_number_;
      task_runner_->PostTask([weak_ptr, i, sequence_number]() {
        weak_ptr->TestTask(i, sequence_number);
      });
    }

    task_runner_->UnblockPostTaskForThread();
  }

 private:
  PerfettoTaskRunner* task_runner_;
  base::WeakPtr<TaskDestination> weak_ptr_;
  const int n_;
  const size_t sequence_number_;
};

class PerfettoTaskRunnerTest : public testing::Test {
 public:
  void SetUp() override {
    task_runner_ = std::make_unique<PerfettoTaskRunner>(CreateNewTaskrunner());
  }

  scoped_refptr<base::SequencedTaskRunner> CreateNewTaskrunner() {
    return base::CreateSingleThreadTaskRunnerWithTraits(
        {base::MayBlock()}, base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  }
  void SetTaskExpectations(base::OnceClosure on_complete,
                           size_t expected_tasks,
                           size_t number_of_sequences = 1) {
    task_destination_ = std::make_unique<TaskDestination>(
        number_of_sequences, expected_tasks, std::move(on_complete));
  }

  void TearDown() override {}

  PerfettoTaskRunner* task_runner() { return task_runner_.get(); }
  TaskDestination* destination() { return task_destination_.get(); }

 private:
  std::unique_ptr<PerfettoTaskRunner> task_runner_;
  std::unique_ptr<TaskDestination> task_destination_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

TEST_F(PerfettoTaskRunnerTest, SequentialTasks) {
  base::RunLoop wait_for_tasks;
  SetTaskExpectations(wait_for_tasks.QuitClosure(), 3);

  auto weak_ptr = destination()->GetWeakPtr();
  task_runner()->PostTask([weak_ptr]() { weak_ptr->TestTask(1); });
  task_runner()->PostTask([weak_ptr]() { weak_ptr->TestTask(2); });
  task_runner()->PostTask([weak_ptr]() { weak_ptr->TestTask(3); });

  wait_for_tasks.Run();
}

TEST_F(PerfettoTaskRunnerTest, SequentialDeferredTasks) {
  base::RunLoop wait_for_tasks;
  SetTaskExpectations(wait_for_tasks.QuitClosure(), 3);

  task_runner()->BlockPostTaskForThread();
  auto weak_ptr = destination()->GetWeakPtr();
  task_runner()->PostTask([weak_ptr]() { weak_ptr->TestTask(1); });
  task_runner()->PostTask([weak_ptr]() { weak_ptr->TestTask(2); });
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, destination()->tasks_run());
  task_runner()->UnblockPostTaskForThread();
  // Posting an unblocked task should post the earlier deferred ones,
  // in the right order.
  task_runner()->PostTask([weak_ptr]() { weak_ptr->TestTask(3); });

  wait_for_tasks.Run();
}

TEST_F(PerfettoTaskRunnerTest, SequentialDeferredTasksByTimer) {
  base::RunLoop wait_for_tasks;
  SetTaskExpectations(wait_for_tasks.QuitClosure(), 3);

  task_runner()->BlockPostTaskForThread();
  auto weak_ptr = destination()->GetWeakPtr();
  task_runner()->PostTask([weak_ptr]() { weak_ptr->TestTask(1); });
  task_runner()->PostTask([weak_ptr]() { weak_ptr->TestTask(2); });
  task_runner()->PostTask([weak_ptr]() { weak_ptr->TestTask(3); });
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, destination()->tasks_run());

  // Start the timer which eventually will tick and post the previously
  // deferred tasks. Note that this is posted directly to the taskqueue
  // rather than the Perfetto wrapper, so it won't be deferred.
  task_runner()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&PerfettoTaskRunner::StartDeferredTasksDrainTimer,
                     base::Unretained(task_runner())));

  wait_for_tasks.Run();
}

TEST_F(PerfettoTaskRunnerTest, SequentialByMultipleSequences) {
  base::RunLoop wait_for_tasks;
  SetTaskExpectations(wait_for_tasks.QuitClosure(), 2001, 3);

  auto weak_ptr = destination()->GetWeakPtr();

  PosterThread first_thread(task_runner(), weak_ptr, 1000, 1);
  PosterThread second_thread(task_runner(), weak_ptr, 1000, 2);
  first_thread.Start();
  second_thread.Start();
  first_thread.Join();
  second_thread.Join();

  // Both threads set the taskrunner to defer new tasks, so none
  // should have run at this point.
  EXPECT_EQ(0u, destination()->tasks_run());

  // Posting an unblocked task should post the earlier deferred ones,
  // in the right order.
  task_runner()->PostTask([weak_ptr]() { weak_ptr->TestTask(1, 0); });
  wait_for_tasks.Run();
}

}  // namespace

}  // namespace tracing
