// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_TLS_DATA_ROUTER_POSIX_H_
#define PLATFORM_IMPL_TLS_DATA_ROUTER_POSIX_H_

#include <mutex>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "platform/api/time.h"
#include "platform/impl/socket_handle_waiter.h"
#include "util/logging.h"

namespace openscreen {
namespace platform {

class StreamSocketPosix;
class TlsConnectionPosix;

// This class is responsible for 3 operations:
//   1) Listen for incoming connections on registed StreamSockets.
//   2) Check all registered TlsConnections for read data via boringSSL call
//      and pass all read data to the connection's observer.
//   3) Check all registered TlsConnections' write buffers for additional data
//      to be written out. If any is present, write it using boringSSL.
// The above operations also imply that this class must support registration
// of StreamSockets and TlsConnections.
// These operations will be called repeatedly on the networking thread, so none
// of them should block. Additionally, this class must ensure that deletions of
// the above types do not occur while a socket/connection is currently being
// accessed from the networking thread.
class TlsDataRouterPosix : public SocketHandleWaiter::Subscriber {
 public:
  class SocketObserver {
   public:
    virtual ~SocketObserver() = default;

    // Socket creation shouldn't occur on the Networking thread, so pass the
    // socket to the observer and expect them to call socket->Accept() on the
    // correct thread.
    virtual void OnConnectionPending(StreamSocketPosix* socket) = 0;
  };

  // The provided SocketHandleWaiter is expected to live for the duration of
  // this object's lifetime.
  TlsDataRouterPosix(
      SocketHandleWaiter* waiter,
      std::function<Clock::time_point()> now_function = Clock::now);
  ~TlsDataRouterPosix() override;

  // Register a TlsConnection that should be watched for readable and writable
  // data.
  void RegisterConnection(TlsConnectionPosix* connection);

  // Deregister a TlsConnection.
  void DeregisterConnection(TlsConnectionPosix* connection);

  // Takes ownership of a StreamSocket and registers that should be watched for
  // incoming Tcp Connections with the SocketHandleWaiter.
  void RegisterSocketObserver(std::unique_ptr<StreamSocketPosix> socket,
                              SocketObserver* observer);

  // Stops watching a Tcp Connections for incoming connections.
  // NOTE: This will destroy the StreamSocket.
  virtual void DeregisterSocketObserver(StreamSocketPosix* socket);

  // Method to be executed on TlsConnection destruction. This is expected to
  // block until the networking thread is not using the provided connection.
  void OnConnectionDestroyed(TlsConnectionPosix* connection);

  // Performs Read and Write operations for all connections or until the timeout
  // has been hit, whichever is first. In the latter case, the following
  // iteration will continue from wherever the previous iteration left off.
  void PerformNetworkingOperations(Clock::duration timeout);

  // SocketHandleWaiter::Subscriber overrides.
  void ProcessReadyHandle(SocketHandleWaiter::SocketHandleRef handle) override;

  OSP_DISALLOW_COPY_AND_ASSIGN(TlsDataRouterPosix);

 protected:
  // Determines if the provided socket is currently being watched by this
  // instance.
  bool IsSocketWatched(StreamSocketPosix* socket) const;

  virtual bool HasTimedOut(Clock::time_point start_time,
                           Clock::duration timeout);

  friend class TestingDataRouter;

 private:
  enum class NetworkingOperation { kReading, kWriting };

  void OnSocketDestroyed(StreamSocketPosix* socket,
                         bool skip_locking_for_testing);

  void RemoveWatchedSocket(StreamSocketPosix* socket);

  // Helper methods for PerformNetworkingOperations.
  NetworkingOperation GetNextMode(NetworkingOperation state);
  std::vector<TlsConnectionPosix*>::iterator GetConnection(
      TlsConnectionPosix* current);

  SocketHandleWaiter* waiter_;

  // Mutex guarding connections_ vector.
  mutable std::mutex connections_mutex_;

  // Mutex guarding socket_mappings_.
  mutable std::mutex socket_mutex_;

  // Function to get the current time.
  std::function<Clock::time_point()> now_function_;

  // Information related to how much of PerformNetworkingOperations(...) was
  // completed before hitting the timeout.
  NetworkingOperation last_operation_ = NetworkingOperation::kReading;
  TlsConnectionPosix* last_connection_processed_ = nullptr;

  // Mapping from all sockets to the observer that should be called when the
  // socket recognizes an incoming connection.
  std::unordered_map<StreamSocketPosix*, SocketObserver*> socket_mappings_
      GUARDED_BY(socket_mutex_);

  // Set of all TlsConnectionPosix objects currently registered.
  std::vector<TlsConnectionPosix*> connections_ GUARDED_BY(connections_mutex_);

  // StreamSockets currently owned by this object, being watched for
  std::vector<std::unique_ptr<StreamSocketPosix>> watched_stream_sockets_
      GUARDED_BY(connections_mutex_);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_TLS_DATA_ROUTER_POSIX_H_
