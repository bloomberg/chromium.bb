// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "platform/impl/network_runner_lifetime_manager.h"

#include <thread>

#include "platform/api/time.h"
#include "platform/impl/network_runner.h"
#include "platform/impl/task_runner.h"

namespace openscreen {
namespace platform {

NetworkRunnerLifetimeManagerImpl::NetworkRunnerLifetimeManagerImpl() = default;

NetworkRunnerLifetimeManagerImpl::NetworkRunnerLifetimeManagerImpl(
    std::unique_ptr<TaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

void NetworkRunnerLifetimeManagerImpl::CreateNetworkRunner() {
  OSP_CHECK(!network_runner_.get()) << "NetworkRunner already created";

  if (!task_runner_.get()) {
    auto task_runner = std::make_unique<TaskRunnerImpl>(Clock::now);
    task_runner_thread_ = std::make_unique<std::thread>(
        [ptr = task_runner.get()]() { ptr->RunUntilStopped(); });
    cleanup_tasks_.emplace(
        [ptr = task_runner.get()]() { ptr->RequestStopSoon(); });
    task_runner_ = std::move(task_runner);
  }

  network_runner_ =
      std::make_unique<NetworkRunnerImpl>(std::move(task_runner_));
  network_runner_thread_ = std::make_unique<std::thread>(
      [ptr = network_runner_.get()]() { ptr->RunUntilStopped(); });
  cleanup_tasks_.emplace(
      [ptr = network_runner_.get()]() { ptr->RequestStopSoon(); });
}

NetworkRunnerLifetimeManagerImpl::~NetworkRunnerLifetimeManagerImpl() {
  while (!cleanup_tasks_.empty()) {
    cleanup_tasks_.front()();
    cleanup_tasks_.pop();
  }

  if (task_runner_thread_.get()) {
    task_runner_thread_->join();
  }
  if (network_runner_thread_.get()) {
    network_runner_thread_->join();
  }
}

// static
std::unique_ptr<NetworkRunnerLifetimeManager>
NetworkRunnerLifetimeManager::Create() {
  return std::make_unique<NetworkRunnerLifetimeManagerImpl>();
}

}  // namespace platform
}  // namespace openscreen
