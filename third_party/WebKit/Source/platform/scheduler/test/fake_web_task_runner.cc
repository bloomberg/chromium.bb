// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/test/fake_web_task_runner.h"

#include <algorithm>
#include <deque>
#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "platform/wtf/RefCounted.h"

namespace blink {
namespace scheduler {

class FakeWebTaskRunner::Data : public WTF::ThreadSafeRefCounted<Data> {
 public:
  Data() : time_(0.0) {}

  void PostTask(base::OnceClosure task, base::TimeDelta delay) {
    task_queue_.push_back(
        std::make_pair(std::move(task), time_ + delay.InSecondsF()));
  }

  using QueueItem = std::pair<base::OnceClosure, double>;
  std::deque<QueueItem>::iterator FindRunnableTask() {
    // TODO(tkent): This should return an item which has the minimum |second|.
    return std::find_if(
        task_queue_.begin(), task_queue_.end(),
        [&](const QueueItem& item) { return item.second <= time_; });
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::deque<QueueItem> task_queue_;
  double time_;

 private:
  ~Data() {}

  friend ThreadSafeRefCounted<Data>;
  DISALLOW_COPY_AND_ASSIGN(Data);
};

class FakeWebTaskRunner::BaseTaskRunner : public base::SingleThreadTaskRunner {
 public:
  explicit BaseTaskRunner(RefPtr<Data> data) : data_(std::move(data)) {}

  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    data_->PostTask(std::move(task), delay);
    return true;
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    data_->PostTask(std::move(task), delay);
    return true;
  }

  bool RunsTasksInCurrentSequence() const { return true; }

 private:
  RefPtr<Data> data_;
};

FakeWebTaskRunner::FakeWebTaskRunner()
    : data_(WTF::AdoptRef(new Data)),
      base_task_runner_(new BaseTaskRunner(data_)) {}

FakeWebTaskRunner::FakeWebTaskRunner(
    RefPtr<Data> data,
    scoped_refptr<BaseTaskRunner> base_task_runner)
    : data_(std::move(data)), base_task_runner_(std::move(base_task_runner)) {}

FakeWebTaskRunner::~FakeWebTaskRunner() {
}

void FakeWebTaskRunner::SetTime(double new_time) {
  data_->time_ = new_time;
}

bool FakeWebTaskRunner::RunsTasksInCurrentSequence() {
  return true;
}

double FakeWebTaskRunner::VirtualTimeSeconds() const {
  return data_->time_;
}

double FakeWebTaskRunner::MonotonicallyIncreasingVirtualTimeSeconds() const {
  return data_->time_;
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeWebTaskRunner::ToSingleThreadTaskRunner() {
  return base_task_runner_;
}

void FakeWebTaskRunner::RunUntilIdle() {
  while (!data_->task_queue_.empty()) {
    // Move the task to run into a local variable in case it touches the
    // task queue by posting a new task.
    base::OnceClosure task = std::move(data_->task_queue_.front()).first;
    data_->task_queue_.pop_front();
    std::move(task).Run();
  }
}

void FakeWebTaskRunner::AdvanceTimeAndRun(double delta_seconds) {
  data_->time_ += delta_seconds;
  for (auto it = data_->FindRunnableTask(); it != data_->task_queue_.end();
       it = data_->FindRunnableTask()) {
    base::OnceClosure task = std::move(*it).first;
    data_->task_queue_.erase(it);
    std::move(task).Run();
  }
}

std::deque<std::pair<base::OnceClosure, double>>
FakeWebTaskRunner::TakePendingTasksForTesting() {
  return std::move(data_->task_queue_);
}

bool FakeWebTaskRunner::PostDelayedTask(const base::Location& location,
                                        base::OnceClosure task,
                                        base::TimeDelta delay) {
  return base_task_runner_->PostDelayedTask(location, std::move(task), delay);
}

}  // namespace scheduler
}  // namespace blink
