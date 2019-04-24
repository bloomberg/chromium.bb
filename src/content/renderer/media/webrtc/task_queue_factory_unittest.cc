// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/task_queue_factory.h"

#include "base/task/task_traits.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/webrtc/api/task_queue/task_queue_test.h"

namespace {

using ::webrtc::TaskQueueTest;

// Wrapper around WebrtcTaskQueueFactory to set up required testing environment.
class TestTaskQueueFactory final : public webrtc::TaskQueueFactory {
 public:
  // webrtc tests block TaskQueue to verify how it behave when it is too busy,
  // allow base sync primitives for that blocking.
  TestTaskQueueFactory()
      : factory_(
            CreateWebRtcTaskQueueFactory({base::WithBaseSyncPrimitives()})) {}

  std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
  CreateTaskQueue(absl::string_view name, Priority priority) const override {
    return factory_->CreateTaskQueue(name, priority);
  }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  std::unique_ptr<webrtc::TaskQueueFactory> factory_;
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(
    WebRtc,
    TaskQueueTest,
    ::testing::Values(std::make_unique<TestTaskQueueFactory>));
