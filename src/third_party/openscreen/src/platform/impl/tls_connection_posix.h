// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_TLS_CONNECTION_POSIX_H_
#define PLATFORM_IMPL_TLS_CONNECTION_POSIX_H_

#include <openssl/ssl.h>

#include <atomic>
#include <memory>

#include "platform/api/tls_connection.h"
#include "platform/impl/platform_client_posix.h"
#include "platform/impl/stream_socket_posix.h"
#include "platform/impl/tls_write_buffer.h"
#include "platform/impl/weak_ptr.h"

namespace openscreen {
namespace platform {

class TaskRunner;
class TlsConnectionFactoryPosix;

class TlsConnectionPosix : public TlsConnection,
                           public TlsWriteBuffer::Observer {
 public:
  ~TlsConnectionPosix() override;

  // Sends any available bytes from this connection's buffer_.
  virtual void SendAvailableBytes();

  // Read out a block/message, if one is available, and notify this instance's
  // TlsConnection::Client.
  virtual void TryReceiveMessage();

  // TlsConnection overrides.
  void SetClient(Client* client) override;
  void Write(const void* data, size_t len) override;
  IPEndpoint GetLocalEndpoint() const override;
  IPEndpoint GetRemoteEndpoint() const override;

  // TlsWriteBuffer::Observer overrides.
  void NotifyWriteBufferFill(double fraction) override;

 protected:
  friend class TlsConnectionFactoryPosix;

  TlsConnectionPosix(IPEndpoint local_address,
                     TaskRunner* task_runner,
                     PlatformClientPosix* platform_client =
                         PlatformClientPosix::GetInstance());
  TlsConnectionPosix(IPAddress::Version version,
                     TaskRunner* task_runner,
                     PlatformClientPosix* platform_client =
                         PlatformClientPosix::GetInstance());
  TlsConnectionPosix(std::unique_ptr<StreamSocket> socket,
                     TaskRunner* task_runner,
                     PlatformClientPosix* platform_client =
                         PlatformClientPosix::GetInstance());

 private:
  // Called on any thread, to post a task to notify the Client that an |error|
  // has occurred.
  void DispatchError(Error error);

  // Helper to call OnWriteBlocked() or OnWriteUnblocked(). If this is not
  // called within a task run by |task_runner_|, it trampolines by posting a
  // task to call itself back via |task_runner_|. See comments in implementation
  // of NotifyWriteBufferFill() for further details.
  void NotifyClientOfWriteBlockStatusSequentially(bool is_blocked);

  TaskRunner* const task_runner_;
  PlatformClientPosix* const platform_client_;

  Client* client_ = nullptr;

  std::unique_ptr<StreamSocket> socket_;
  bssl::UniquePtr<SSL> ssl_;

  std::atomic_bool notified_client_buffer_is_blocked_{false};
  TlsWriteBuffer buffer_;

  WeakPtrFactory<TlsConnectionPosix> weak_factory_{this};

  OSP_DISALLOW_COPY_AND_ASSIGN(TlsConnectionPosix);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_TLS_CONNECTION_POSIX_H_
