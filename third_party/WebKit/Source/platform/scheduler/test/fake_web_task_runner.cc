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

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::deque<base::Closure> task_queue_;
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
    data_->task_queue_.push_back(task);
    return true;
  }

  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override {
    data_->task_queue_.push_back(task);
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

void FakeWebTaskRunner::postTask(const WebTraceLocation&, Task*) {
  NOTREACHED();
}

void FakeWebTaskRunner::postDelayedTask(const WebTraceLocation&,
                                        Task* task,
                                        double) {
  data_->task_queue_.push_back(
      base::Bind(&WebTaskRunner::Task::run, base::Owned(task)));
}

void FakeWebTaskRunner::postDelayedTask(const WebTraceLocation&,
                                        const base::Closure& closure,
                                        double) {
  data_->task_queue_.push_back(closure);
}

bool FakeWebTaskRunner::runsTasksOnCurrentThread() {
  return true;
}

std::unique_ptr<WebTaskRunner> FakeWebTaskRunner::clone() {
  return WTF::wrapUnique(new FakeWebTaskRunner(data_, base_task_runner_));
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
    base::Closure task = std::move(data_->task_queue_.front());
    data_->task_queue_.pop_front();
    task.Run();
  }
}

std::deque<base::Closure> FakeWebTaskRunner::takePendingTasksForTesting() {
  return std::move(data_->task_queue_);
}

}  // namespace scheduler
}  // namespace blink
