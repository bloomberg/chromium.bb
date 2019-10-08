// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/platform_client_posix.h"

#include <mutex>

#include "platform/api/time.h"

namespace openscreen {
namespace platform {

PlatformClientPosix::PlatformClientPosix() = default;

PlatformClientPosix::~PlatformClientPosix() {
  // TODO(rwkeane): Handle threads and safely shutting down singletons.
}

// static
void PlatformClientPosix::Create() {
  PlatformClient::SetInstance(new PlatformClientPosix());
}

// static
void PlatformClientPosix::ShutDown() {
  PlatformClient::ShutDown();
}

UdpSocketReaderPosix* PlatformClientPosix::udp_socket_reader() {
  if (!udp_socket_reader_.get()) {
    std::lock_guard<std::recursive_mutex> lock(initialization_mutex_);
    if (!udp_socket_reader_.get()) {
      udp_socket_reader_ =
          std::make_unique<UdpSocketReaderPosix>(socket_handle_waiter());
    }
  }
  return udp_socket_reader_.get();
}

TlsDataRouterPosix* PlatformClientPosix::tls_data_router() {
  if (!tls_data_router_.get()) {
    std::lock_guard<std::recursive_mutex> lock(initialization_mutex_);
    if (!tls_data_router_.get()) {
      tls_data_router_ =
          std::make_unique<TlsDataRouterPosix>(socket_handle_waiter());
    }
  }
  return tls_data_router_.get();
}

SocketHandleWaiterPosix* PlatformClientPosix::socket_handle_waiter() {
  if (!waiter_.get()) {
    std::lock_guard<std::recursive_mutex> lock(initialization_mutex_);
    if (!waiter_.get()) {
      waiter_ = std::make_unique<SocketHandleWaiterPosix>();
      // TODO(rwkeane): Start networking thread.
    }
  }
  return waiter_.get();
}

TaskRunner* PlatformClientPosix::task_runner() {
  if (!task_runner_.get()) {
    std::lock_guard<std::recursive_mutex> lock(initialization_mutex_);
    if (!task_runner_.get()) {
      task_runner_ = std::make_unique<TaskRunnerImpl>(Clock::now);
      // TODO(rwkeane): Start TaskRunner thread.
    }
  }
  return task_runner_.get();
}

}  // namespace platform
}  // namespace openscreen
