// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_PLATFORM_CLIENT_POSIX_H_
#define PLATFORM_IMPL_PLATFORM_CLIENT_POSIX_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include "platform/api/time.h"
#include "platform/base/macros.h"
#include "platform/impl/socket_handle_waiter_posix.h"
#include "platform/impl/task_runner.h"
#include "platform/impl/tls_data_router_posix.h"
#include "util/operation_loop.h"

namespace openscreen {
namespace platform {

class UdpSocketReaderPosix;

// A PlatformClientPosix is an access point for all singletons in a standalone
// application. The static SetInstance method is to be called before library use
// begins, and the ShutDown() method should be called to deallocate the platform
// library's global singletons (for example to save memory when libcast is not
// in use).
class PlatformClientPosix {
 public:
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

  // Shuts down and deletes the PlatformClient instance currently stored as a
  // singleton. This method is expected to be called before program exit. After
  // calling this method, if the client wishes to continue using the platform
  // library, a new singleton must be created.
  // NOTE: This method is NOT thread safe and should only be called from the
  // embedder thread.
  static void ShutDown();

  static PlatformClientPosix* GetInstance() { return instance_; }

  // This method is thread-safe.
  TlsDataRouterPosix* tls_data_router();

  // This method is thread-safe.
  UdpSocketReaderPosix* udp_socket_reader();

  // Returns the TaskRunner associated with this PlatformClient.
  // NOTE: This method is expected to be thread safe.
  TaskRunner* GetTaskRunner();

 protected:
  // The TaskRunner parameter here is a user-provided TaskRunner to be used
  // instead of the one normally created within PlatformClientPosix. Ownership
  // of the TaskRunner is transferred to this class.
  PlatformClientPosix(Clock::duration networking_operation_timeout,
                      Clock::duration networking_loop_interval,
                      std::unique_ptr<TaskRunner> task_runner);

  // This method is expected to be called in order to set the singleton instance
  // (typically from the Create() method). It should only be called from the
  // embedder thread. Client should be a new instance create via 'new' and
  // ownership of this instance will be transferred to this class.
  // NOTE: This method is NOT thread safe and should only be called from the
  // embedder thread.
  static void SetInstance(PlatformClientPosix* client);

  // Called by ShutDown().
  ~PlatformClientPosix();

 private:
  // Called by Create().
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
  absl::optional<TaskRunnerImpl> owned_task_runner_;
  std::unique_ptr<TaskRunner> caller_provided_task_runner_;

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

  // Threads for running TaskRunner and OperationLoop instances.
  // NOTE: These must be declared last to avoid nondterministic failures.
  std::thread networking_loop_thread_;
  std::thread task_runner_thread_;

  static PlatformClientPosix* instance_;

  OSP_DISALLOW_COPY_AND_ASSIGN(PlatformClientPosix);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_PLATFORM_CLIENT_POSIX_H_
