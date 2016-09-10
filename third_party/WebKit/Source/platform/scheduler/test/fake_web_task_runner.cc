// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/test/fake_web_task_runner.h"

#include "base/logging.h"
#include "wtf/RefCounted.h"

namespace blink {
namespace scheduler {

class FakeWebTaskRunner::Data : public WTF::ThreadSafeRefCounted<Data> {
 public:
  Data() : time_(0.0) {}

  std::unique_ptr<Task> task_;
  double time_;

 private:
  ~Data() {}

  friend ThreadSafeRefCounted<Data>;
  DISALLOW_COPY_AND_ASSIGN(Data);
};

FakeWebTaskRunner::FakeWebTaskRunner() : data_(adoptRef(new Data)) {}

FakeWebTaskRunner::FakeWebTaskRunner(PassRefPtr<Data> data)
    : data_(std::move(data)) {
}

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
  data_->task_.reset(task);
}

bool FakeWebTaskRunner::runsTasksOnCurrentThread() {
  return true;
}

std::unique_ptr<WebTaskRunner> FakeWebTaskRunner::clone() {
  return WTF::wrapUnique(new FakeWebTaskRunner(data_));
}

double FakeWebTaskRunner::virtualTimeSeconds() const {
  return data_->time_;
}

double FakeWebTaskRunner::monotonicallyIncreasingVirtualTimeSeconds() const {
  return data_->time_;
}

SingleThreadTaskRunner* FakeWebTaskRunner::toSingleThreadTaskRunner() {
  NOTREACHED();
  return nullptr;
}

}  // namespace scheduler
}  // namespace blink
