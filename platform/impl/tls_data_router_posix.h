// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_TLS_DATA_ROUTER_POSIX_H_
#define PLATFORM_IMPL_TLS_DATA_ROUTER_POSIX_H_

#include <mutex>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "platform/api/logging.h"
#include "platform/impl/socket_handle_waiter.h"

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
  explicit TlsDataRouterPosix(SocketHandleWaiter* waiter);
  ~TlsDataRouterPosix() override;

  // Register a TlsConnection that should be watched for readable and writable
  // data.
  void RegisterConnection(TlsConnectionPosix* connection);

  // Deregister a TlsConnection.
  void DeregisterConnection(TlsConnectionPosix* connection);

  // Register a StreamSocket that should be watched for incoming Tcp Connections
  // with the SocketHandleWaiter.
  void RegisterSocketObserver(StreamSocketPosix* socket,
                              SocketObserver* observer);

  // Stops watching a Tcp Connections for incoming connections.
  void DeregisterSocketObserver(StreamSocketPosix* socket);

  // Method to be executed on TlsConnection destruction. This is expected to
  // block until the networking thread is not using the provided connection.
  void OnConnectionDestroyed(TlsConnectionPosix* connection);

  // Method to be executed on StreamSocket destruction. This is expected to
  // block until the networking thread is not using the provided socket.
  virtual void OnSocketDestroyed(StreamSocketPosix* socket);

  // Perform Read on all registered sockets.
  void ReadAll();

  // Perform write on all registered sockets.
  void WriteAll();

  // SocketHandleWaiter::Subscriber overrides.
  void ProcessReadyHandle(SocketHandleWaiter::SocketHandleRef handle) override;

 protected:
  // Determines if the provided socket is currently being watched by this
  // instance.
  bool IsSocketWatched(StreamSocketPosix* socket) const;

  friend class TestingDataRouter;

 private:
  void OnSocketDestroyed(StreamSocketPosix* socket,
                         bool skip_locking_for_testing);

  void RemoveWatchedSocket(StreamSocketPosix* socket);

  SocketHandleWaiter* waiter_;

  // Mutex guarding connections_ vector.
  mutable std::mutex connections_mutex_;

  // Mutex guarding socket_mappings_.
  mutable std::mutex socket_mutex_;

  // Mapping from all sockets to the observer that should be called when the
  // socket recognizes an incoming connection.
  std::unordered_map<StreamSocketPosix*, SocketObserver*> socket_mappings_
      GUARDED_BY(socket_mutex_);

  // Set of all TlsConnectionPosix objects currently registered.
  std::vector<TlsConnectionPosix*> connections_ GUARDED_BY(connections_mutex_);

  OSP_DISALLOW_COPY_AND_ASSIGN(TlsDataRouterPosix);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_TLS_DATA_ROUTER_POSIX_H_
