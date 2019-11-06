// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef PLATFORM_IMPL_NETWORK_RUNNER_LIFETIME_MANAGER_H_
#define PLATFORM_IMPL_NETWORK_RUNNER_LIFETIME_MANAGER_H_

#include <queue>
#include <thread>

#include "platform/api/network_runner_lifetime_manager.h"
#include "platform/api/task_runner.h"
#include "platform/impl/network_runner.h"

namespace openscreen {
namespace platform {

class NetworkRunnerLifetimeManagerImpl final
    : public NetworkRunnerLifetimeManager {
 public:
  NetworkRunnerLifetimeManagerImpl();
  explicit NetworkRunnerLifetimeManagerImpl(
      std::unique_ptr<TaskRunner> task_runner);
  ~NetworkRunnerLifetimeManagerImpl() override;

  void CreateNetworkRunner() override;
  NetworkRunner* Get() override {
    OSP_CHECK(network_runner_.get()) << "NetworkRunner not yet created";
    return network_runner_.get();
  }

 private:
  std::unique_ptr<NetworkRunnerImpl> network_runner_;
  std::unique_ptr<std::thread> network_runner_thread_;
  std::unique_ptr<TaskRunner> task_runner_;
  std::unique_ptr<std::thread> task_runner_thread_;
  std::queue<TaskRunner::Task> cleanup_tasks_;

  OSP_DISALLOW_COPY_AND_ASSIGN(NetworkRunnerLifetimeManagerImpl);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_NETWORK_RUNNER_LIFETIME_MANAGER_H_
