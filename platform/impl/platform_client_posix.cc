// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/platform_client_posix.h"

#include <mutex>

#include "platform/impl/udp_socket_reader_posix.h"

namespace openscreen {
namespace platform {

PlatformClientPosix::PlatformClientPosix(
    Clock::duration networking_operation_timeout,
    Clock::duration networking_loop_interval)
    : networking_loop_(networking_operations(),
                       networking_operation_timeout,
                       networking_loop_interval),
      task_runner_(Clock::now),
      networking_loop_thread_(&OperationLoop::RunUntilStopped,
                              &networking_loop_),
      task_runner_thread_(&TaskRunnerImpl::RunUntilStopped, &task_runner_) {}

PlatformClientPosix::~PlatformClientPosix() {
  networking_loop_.RequestStopSoon();
  task_runner_.RequestStopSoon();
  networking_loop_thread_.join();
  task_runner_thread_.join();
}

// static
void PlatformClientPosix::Create(Clock::duration networking_operation_timeout,
                                 Clock::duration networking_loop_interval) {
  PlatformClient::SetInstance(new PlatformClientPosix(
      networking_operation_timeout, networking_loop_interval));
}

// static
void PlatformClientPosix::ShutDown() {
  PlatformClient::ShutDown();
}

UdpSocketReaderPosix* PlatformClientPosix::udp_socket_reader() {
  std::call_once(udp_socket_reader_initialization_, [this]() {
    udp_socket_reader_ =
        std::make_unique<UdpSocketReaderPosix>(socket_handle_waiter());
  });
  return udp_socket_reader_.get();
}

TlsDataRouterPosix* PlatformClientPosix::tls_data_router() {
  std::call_once(tls_data_router_initialization_, [this]() {
    tls_data_router_ =
        std::make_unique<TlsDataRouterPosix>(socket_handle_waiter());
    tls_data_router_created_.store(true);
  });
  return tls_data_router_.get();
}

SocketHandleWaiterPosix* PlatformClientPosix::socket_handle_waiter() {
  std::call_once(waiter_initialization_, [this]() {
    waiter_ = std::make_unique<SocketHandleWaiterPosix>();
    waiter_created_.store(true);
  });
  return waiter_.get();
}

TaskRunner* PlatformClientPosix::GetTaskRunner() {
  return &task_runner_;
}

void PlatformClientPosix::PerformSocketHandleWaiterActions(
    Clock::duration timeout) {
  if (!waiter_created_.load()) {
    return;
  }

  socket_handle_waiter()->ProcessHandles(timeout);
}

void PlatformClientPosix::PerformTlsDataRouterActions(Clock::duration timeout) {
  if (!tls_data_router_created_.load()) {
    return;
  }

  tls_data_router()->PerformNetworkingOperations(timeout);
}

std::vector<std::function<void(Clock::duration)>>
PlatformClientPosix::networking_operations() {
  return {[this](Clock::duration timeout) {
            PerformSocketHandleWaiterActions(timeout);
          },
          [this](Clock::duration timeout) {
            PerformTlsDataRouterActions(timeout);
          }};
}

}  // namespace platform
}  // namespace openscreen
