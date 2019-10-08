// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_PLATFORM_CLIENT_POSIX_H_
#define PLATFORM_IMPL_PLATFORM_CLIENT_POSIX_H_

#include <mutex>

#include "platform/api/platform_client.h"
#include "platform/impl/socket_handle_waiter_posix.h"
#include "platform/impl/task_runner.h"
#include "platform/impl/tls_data_router_posix.h"
#include "platform/impl/udp_socket_reader_posix.h"

namespace openscreen {
namespace platform {

class PlatformClientPosix : public PlatformClient {
 public:
  ~PlatformClientPosix() override;

  // This method is expected to be called before the library is used.
  // NOTE: This method is NOT thread safe and should only be called from the
  // embedder thread.
  static void Create();

  // Shuts down the PlatformClient instance currently stored as a singleton.
  // This method is expected to be called before program exit.
  // NOTE: This method is NOT thread safe and should only be called from the
  // embedder thread.
  static void ShutDown();

  // This method is thread-safe.
  TlsDataRouterPosix* tls_data_router();

  // This method is thread-safe.
  UdpSocketReaderPosix* udp_socket_reader();

  // PlatformClient overrides.
  TaskRunner* task_runner() override;

 private:
  PlatformClientPosix();

  // This method is thread-safe.
  SocketHandleWaiterPosix* socket_handle_waiter();

  // Mutex to ensure threadsafe lazy construction of class members.
  std::recursive_mutex initialization_mutex_;

  // Instance objects are created at runtime when they are first needed.
  std::unique_ptr<TaskRunnerImpl> task_runner_;
  std::unique_ptr<SocketHandleWaiterPosix> waiter_;
  std::unique_ptr<UdpSocketReaderPosix> udp_socket_reader_;
  std::unique_ptr<TlsDataRouterPosix> tls_data_router_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_PLATFORM_CLIENT_POSIX_H_
