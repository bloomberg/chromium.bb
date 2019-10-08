// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_TLS_CONNECTION_FACTORY_POSIX_H_
#define PLATFORM_IMPL_TLS_CONNECTION_FACTORY_POSIX_H_

#include <future>
#include <memory>
#include <string>

#include "platform/api/tls_connection.h"
#include "platform/api/tls_connection_factory.h"
#include "platform/base/socket_state.h"
#include "platform/impl/tls_data_router_posix.h"

namespace openscreen {
namespace platform {

class StreamSocket;

class TlsConnectionFactoryPosix : public TlsConnectionFactory,
                                  public TlsDataRouterPosix::SocketObserver {
 public:
  TlsConnectionFactoryPosix(Client* client, TaskRunner* task_runner);
  ~TlsConnectionFactoryPosix() override;

  // TlsConnectionFactory overrides
  void Connect(const IPEndpoint& remote_address,
               const TlsConnectOptions& options) override;

  void SetListenCredentials(const TlsCredentials& credentials) override;
  void Listen(const IPEndpoint& local_address,
              const TlsListenOptions& options) override;

  // TlsDataRouterPosix::SocketObserver overrides.
  void OnConnectionPending(StreamSocketPosix* socket) override;

 private:
  // Configures a new SSL connection when a StreamSocket connection is accepted.
  void OnSocketAccepted(std::unique_ptr<StreamSocket> socket);

  // Configures the SSL connection for the provided TlsConnectionPosix,
  // returning true if the process is successful, false otherwise.
  bool ConfigureSsl(TlsConnectionPosix* connection);

  // Ensures that SSL is initialized, then gets a new SSL connection.
  ErrorOr<bssl::UniquePtr<SSL>> GetSslConnection();

  // Method wrapping the Initialize() private method, that can be safely called
  // multiple times.
  void EnsureInitialized();

  // Create the shared context used for all SSL connections created by this
  // factory.
  void Initialize();

  // Thread-safe mechanism to ensure Initialize() is only called once.
  std::once_flag init_instance_flag_;

  // Are the Listen() credentials set? Getting the certificate directly
  // from the SSL_CTX is non-trivial, so we store a property instead.
  bool listen_credentials_set_ = false;

  // The task runner used for internal operations.
  TaskRunner* const task_runner_;

  // SSL context, for creating SSL Connections via BoringSSL.
  bssl::UniquePtr<SSL_CTX> ssl_context_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_TLS_CONNECTION_FACTORY_POSIX_H_
