// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/metronome_task_queue_factory.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/task_queue/task_queue_test.h"
#include "third_party/webrtc/rtc_base/task_utils/to_queued_task.h"
#include "third_party/webrtc_overrides/metronome_source.h"
#include "third_party/webrtc_overrides/test/metronome_like_task_queue_test.h"

namespace blink {

namespace {

using ::webrtc::TaskQueueTest;

// Test-only factory needed for the TaskQueueTest suite.
class TestMetronomeTaskQueueFactory final : public webrtc::TaskQueueFactory {
 public:
  TestMetronomeTaskQueueFactory()
      : factory_(CreateWebRtcMetronomeTaskQueueFactory()) {}

  std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
  CreateTaskQueue(absl::string_view name, Priority priority) const override {
    return factory_->CreateTaskQueue(name, priority);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<webrtc::TaskQueueFactory> factory_;
};

// Instantiate suite to run all tests defined in
// third_party/webrtc/api/task_queue/task_queue_test.h.
INSTANTIATE_TEST_SUITE_P(
    WebRtcMetronomeTaskQueue,
    TaskQueueTest,
    ::testing::Values(std::make_unique<TestMetronomeTaskQueueFactory>));

// Provider needed for the MetronomeLikeTaskQueueTest suite.
class MetronomeTaskQueueProvider : public MetronomeLikeTaskQueueProvider {
 public:
  void Initialize() override {
    task_queue_ = CreateWebRtcMetronomeTaskQueueFactory()->CreateTaskQueue(
        "MetronomeTestTaskQueue", webrtc::TaskQueueFactory::Priority::NORMAL);
  }

  base::TimeDelta DeltaToNextTick() const override {
    base::TimeTicks now = base::TimeTicks::Now();
    return MetronomeSource::TimeSnappedToNextTick(now) - now;
  }
  base::TimeDelta MetronomeTick() const override {
    return MetronomeSource::Tick();
  }
  webrtc::TaskQueueBase* TaskQueue() const override {
    return task_queue_.get();
  }

 private:
  std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter> task_queue_;
};

// Instantiate suite to run all tests defined in
// third_party/webrtc_overrides/test/metronome_like_task_queue_test.h
INSTANTIATE_TEST_SUITE_P(
    WebRtcMetronomeTaskQueue,
    MetronomeLikeTaskQueueTest,
    ::testing::Values(std::make_unique<MetronomeTaskQueueProvider>));

}  // namespace

}  // namespace blink
