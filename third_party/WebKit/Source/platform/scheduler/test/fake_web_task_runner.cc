// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/test/fake_web_task_runner.h"

#include <deque>

#include "base/callback.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "wtf/RefCounted.h"

namespace blink {
namespace scheduler {

class FakeWebTaskRunner::Data : public WTF::ThreadSafeRefCounted<Data> {
 public:
  Data() : time_(0.0) {}

  void PostTask(const base::Closure& task, base::TimeDelta delay) {
    task_queue_.push_back(std::make_pair(task, time_ + delay.InSecondsF()));
  }

  using QueueItem = std::pair<base::Closure, double>;
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
  explicit BaseTaskRunner(PassRefPtr<Data> data) : data_(std::move(data)) {}

  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override {
    data_->PostTask(task, delay);
    return true;
  }

  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override {
    data_->PostTask(task, delay);
    return true;
  }

  bool RunsTasksOnCurrentThread() const { return true; }

 private:
  RefPtr<Data> data_;
};

FakeWebTaskRunner::FakeWebTaskRunner()
    : data_(adoptRef(new Data)), base_task_runner_(new BaseTaskRunner(data_)) {}

FakeWebTaskRunner::FakeWebTaskRunner(
    PassRefPtr<Data> data,
    scoped_refptr<BaseTaskRunner> base_task_runner)
    : data_(std::move(data)), base_task_runner_(std::move(base_task_runner)) {}

FakeWebTaskRunner::~FakeWebTaskRunner() {
}

void FakeWebTaskRunner::setTime(double new_time) {
  data_->time_ = new_time;
}

void FakeWebTaskRunner::postDelayedTask(const WebTraceLocation&,
                                        const base::Closure& closure,
                                        double delay_ms) {
  data_->PostTask(closure, base::TimeDelta::FromMillisecondsD(delay_ms));
}

bool FakeWebTaskRunner::runsTasksOnCurrentThread() {
  return true;
}

double FakeWebTaskRunner::virtualTimeSeconds() const {
  return data_->time_;
}

double FakeWebTaskRunner::monotonicallyIncreasingVirtualTimeSeconds() const {
  return data_->time_;
}

SingleThreadTaskRunner* FakeWebTaskRunner::toSingleThreadTaskRunner() {
  return base_task_runner_.get();
}

void FakeWebTaskRunner::runUntilIdle() {
  while (!data_->task_queue_.empty()) {
    // Move the task to run into a local variable in case it touches the
    // task queue by posting a new task.
    base::Closure task = std::move(data_->task_queue_.front()).first;
    data_->task_queue_.pop_front();
    task.Run();
  }
}

void FakeWebTaskRunner::advanceTimeAndRun(double delta_seconds) {
  data_->time_ += delta_seconds;
  for (auto it = data_->FindRunnableTask(); it != data_->task_queue_.end();
       it = data_->FindRunnableTask()) {
    base::Closure task = std::move(*it).first;
    data_->task_queue_.erase(it);
    task.Run();
  }
}

std::deque<std::pair<base::Closure, double>>
FakeWebTaskRunner::takePendingTasksForTesting() {
  return std::move(data_->task_queue_);
}

}  // namespace scheduler
}  // namespace blink
