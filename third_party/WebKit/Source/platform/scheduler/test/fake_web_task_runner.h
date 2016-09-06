// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_FAKE_WEB_TASK_RUNNER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_FAKE_WEB_TASK_RUNNER_H_

#include "base/macros.h"
#include "public/platform/WebTaskRunner.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {
namespace scheduler {

// A dummy WebTaskRunner for tests.
class FakeWebTaskRunner : public WebTaskRunner {
 public:
  FakeWebTaskRunner();
  ~FakeWebTaskRunner() override;

  void setTime(double new_time);

  // WebTaskRunner implementation:
  void postTask(const WebTraceLocation&, Task*) override;
  void postDelayedTask(const WebTraceLocation&, Task*, double) override;
  bool runsTasksOnCurrentThread() override;
  std::unique_ptr<WebTaskRunner> clone() override;
  double virtualTimeSeconds() const override;
  double monotonicallyIncreasingVirtualTimeSeconds() const override;
  SingleThreadTaskRunner* toSingleThreadTaskRunner() override;

 private:
  class Data;
  RefPtr<Data> data_;

  FakeWebTaskRunner(PassRefPtr<Data> data);

  DISALLOW_COPY_AND_ASSIGN(FakeWebTaskRunner);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_FAKE_WEB_TASK_RUNNER_H_
