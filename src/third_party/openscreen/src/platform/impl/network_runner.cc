// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/network_runner.h"

#include <iostream>

#include "platform/api/logging.h"
#include "platform/impl/task_runner.h"

namespace openscreen {
namespace platform {

NetworkRunnerImpl::NetworkRunnerImpl(std::unique_ptr<TaskRunner> task_runner)
    : NetworkRunnerImpl(std::move(task_runner),
                        std::make_unique<NetworkReader>(task_runner.get())) {}

NetworkRunnerImpl::NetworkRunnerImpl(
    std::unique_ptr<TaskRunner> task_runner,
    std::unique_ptr<NetworkReader> network_loop)
    : network_loop_(std::move(network_loop)),
      task_runner_(std::move(task_runner)){};

Error NetworkRunnerImpl::ReadRepeatedly(UdpSocket* socket,
                                        UdpReadCallback* callback) {
  NetworkReader::Callback func = [callback, this](UdpPacket packet) {
    callback->OnRead(std::move(packet), this);
  };
  return network_loop_->ReadRepeatedly(socket, func);
}

bool NetworkRunnerImpl::CancelRead(UdpSocket* socket) {
  return network_loop_->CancelRead(socket);
}

void NetworkRunnerImpl::PostPackagedTask(Task task) {
  task_runner_->PostPackagedTask(std::move(task));
}

void NetworkRunnerImpl::PostPackagedTaskWithDelay(Task task,
                                                  Clock::duration delay) {
  task_runner_->PostPackagedTaskWithDelay(std::move(task), delay);
}

void NetworkRunnerImpl::RunUntilStopped() {
  const bool was_running = is_running_.exchange(true);
  OSP_CHECK(!was_running);

  network_loop_->RunUntilStopped();
}

void NetworkRunnerImpl::RequestStopSoon() {
  is_running_.exchange(false);

  network_loop_->RequestStopSoon();
}

}  // namespace platform
}  // namespace openscreen
