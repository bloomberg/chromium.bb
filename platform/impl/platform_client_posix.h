// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_PLATFORM_CLIENT_POSIX_H_
#define PLATFORM_IMPL_PLATFORM_CLIENT_POSIX_H_

#include <atomic>
#include <mutex>
#include <thread>

#include "platform/api/platform_client.h"
#include "platform/api/time.h"
#include "platform/impl/socket_handle_waiter_posix.h"
#include "platform/impl/task_runner.h"
#include "platform/impl/tls_data_router_posix.h"
#include "util/operation_loop.h"

namespace openscreen {
namespace platform {

class UdpSocketReaderPosix;

class PlatformClientPosix : public PlatformClient {
 public:
  ~PlatformClientPosix() override;

  // This method is expected to be called before the library is used.
  // The networking_loop_interval parameter here represents the minimum amount
  // of time that should pass between iterations of the loop used to handle
  // networking operations. Higher values will result in less time being spent
  // on these operations, but also potentially less performant networking
  // operations. The networking_operation_timeout parameter refers to how much
  // time may be spent on a single networking operation type.
  // NOTE: This method is NOT thread safe and should only be called from the
  // embedder thread.
  static void Create(Clock::duration networking_operation_timeout,
                     Clock::duration networking_loop_interval);

  // Shuts down the PlatformClient instance currently stored as a singleton.
  // This method is expected to be called before program exit.
  // NOTE: This method is NOT thread safe and should only be called from the
  // embedder thread.
  static void ShutDown();

  inline static PlatformClientPosix* GetInstance() {
    return static_cast<PlatformClientPosix*>(PlatformClient::GetInstance());
  }

  // This method is thread-safe.
  TlsDataRouterPosix* tls_data_router();

  // This method is thread-safe.
  UdpSocketReaderPosix* udp_socket_reader();

  // PlatformClient overrides.
  TaskRunner* GetTaskRunner() override;

 private:
  PlatformClientPosix(Clock::duration networking_operation_timeout,
                      Clock::duration networking_loop_interval);

  // This method is thread-safe.
  SocketHandleWaiterPosix* socket_handle_waiter();

  // Helper functions to use when creating and calling the OperationLoop used
  // for the networking thread.
  void PerformSocketHandleWaiterActions(Clock::duration timeout);
  void PerformTlsDataRouterActions(Clock::duration timeout);
  std::vector<std::function<void(Clock::duration)>> networking_operations();

  // Instance objects with threads are created at object-creation time.
  // NOTE: Delayed instantiation of networking_loop_ may be useful in future.
  OperationLoop networking_loop_;
  TaskRunnerImpl task_runner_;
  std::thread networking_loop_thread_;
  std::thread task_runner_thread_;

  // Track whether the associated instance variable has been created yet.
  std::atomic_bool waiter_created_{false};
  std::atomic_bool tls_data_router_created_{false};

  // Flags used to ensure that initialization of below instance objects occurs
  // only once across all threads.
  std::once_flag waiter_initialization_;
  std::once_flag udp_socket_reader_initialization_;
  std::once_flag tls_data_router_initialization_;

  // Instance objects are created at runtime when they are first needed.
  std::unique_ptr<SocketHandleWaiterPosix> waiter_;
  std::unique_ptr<UdpSocketReaderPosix> udp_socket_reader_;
  std::unique_ptr<TlsDataRouterPosix> tls_data_router_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_PLATFORM_CLIENT_POSIX_H_
