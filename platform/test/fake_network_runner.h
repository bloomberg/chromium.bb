// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_TEST_FAKE_NETWORK_RUNNER_H_
#define PLATFORM_TEST_FAKE_NETWORK_RUNNER_H_

#include <deque>
#include <map>

#include "platform/api/network_runner.h"

namespace openscreen {
namespace platform {

class FakeNetworkRunner final : public NetworkRunner {
 public:
  FakeNetworkRunner() = default;
  ~FakeNetworkRunner() override = default;

  // NetworkRunner overrides.
  Error ReadRepeatedly(UdpSocket* socket, UdpReadCallback* callback) override;

  Error CancelRead(UdpSocket* socket) override;

  void PostPackagedTask(Task task) override;

  // NOTE: PostPackagedTaskWithDelay does not respect delays, but instead runs
  // all delayed tasks iteratively once the immediate tasks are run.
  void PostPackagedTaskWithDelay(Task task, Clock::duration delay) override;

  // Helper methods.
  void RunTasksUntilIdle();

  void PostNewPacket(UdpPacket packet);

 private:
  std::deque<Task> task_queue_;
  std::map<UdpSocket*, UdpReadCallback*> callbacks_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_TEST_FAKE_NETWORK_RUNNER_H_
