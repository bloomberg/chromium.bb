// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_connection_factory_posix.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <openssl/ssl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <memory>

#include "absl/types/optional.h"
#include "platform/api/logging.h"
#include "platform/api/tls_connection_factory.h"
#include "platform/api/trace_logging.h"
#include "platform/base/error.h"
#include "platform/impl/stream_socket.h"
#include "platform/impl/tls_connection_posix.h"
#include "util/crypto/openssl_util.h"

namespace openscreen {
namespace platform {

std::unique_ptr<TlsConnectionFactory> TlsConnectionFactory::CreateFactory(
    Client* client,
    TaskRunner* task_runner) {
  return std::unique_ptr<TlsConnectionFactory>(
      new TlsConnectionFactoryPosix(client, task_runner));
}

TlsConnectionFactoryPosix::TlsConnectionFactoryPosix(Client* client,
                                                     TaskRunner* task_runner)
    : TlsConnectionFactory(client, task_runner), task_runner_(task_runner) {}

TlsConnectionFactoryPosix::~TlsConnectionFactoryPosix() = default;

// TODO(rwkeane): Add support for resuming sessions.
// TODO(rwkeane): Integrate with Auth.
void TlsConnectionFactoryPosix::Connect(const IPEndpoint& remote_address,
                                        const TlsConnectOptions& options) {
  TRACE_SCOPED(TraceCategory::SSL, "TlsConnectionFactoryPosix::Connect");
  IPAddress::Version version = remote_address.address.version();
  std::unique_ptr<TlsConnectionPosix> connection =
      std::make_unique<TlsConnectionPosix>(version, task_runner_);
  Error connect_error = connection->socket_->Connect(remote_address);
  if (!connect_error.ok()) {
    TRACE_SET_RESULT(connect_error.error());
    OnConnectionFailed(remote_address);
  }

  if (!ConfigureSsl(connection.get())) {
    return;
  }

  if (options.unsafely_skip_certificate_validation) {
    // Verifies the server certificate but does not make errors fatal.
    SSL_set_verify(connection->ssl_.get(), SSL_VERIFY_NONE, nullptr);
  } else {
    // Make server certificate errors fatal.
    SSL_set_verify(connection->ssl_.get(), SSL_VERIFY_PEER, nullptr);
  }

  const int connection_status = SSL_connect(connection->ssl_.get());
  if (connection_status != 1) {
    OnConnectionFailed(connection->remote_address());
    TRACE_SET_RESULT(GetSSLError(connection->ssl_.get(), connection_status));
    return;
  }

  OnConnected(std::move(connection));
}

bool TlsConnectionFactoryPosix::ConfigureSsl(TlsConnectionPosix* connection) {
  ErrorOr<bssl::UniquePtr<SSL>> ssl_or_error = GetSslConnection();
  if (ssl_or_error.is_error()) {
    OnError(ssl_or_error.error());
    TRACE_SET_RESULT(ssl_or_error.error());
    return false;
  }

  bssl::UniquePtr<SSL> ssl = std::move(ssl_or_error.value());
  if (!SSL_set_fd(ssl.get(), connection->socket_->socket_handle().fd)) {
    OnConnectionFailed(connection->remote_address());
    TRACE_SET_RESULT(Error(Error::Code::kSocketBindFailure));
    return false;
  }

  connection->ssl_.swap(ssl);
  return true;
}

void TlsConnectionFactoryPosix::Listen(const IPEndpoint& local_address,
                                       const TlsCredentials& credentials,
                                       const TlsListenOptions& options) {
  StreamSocketPosix socket(local_address);
  socket.Listen(options.backlog_size);

  // TODO(jophba, rwkeane): Call on singleton:
  // TlsDataRouterPosix::RegisterSocketObserver(std::move(socket), this)
  OSP_UNIMPLEMENTED();
}

void TlsConnectionFactoryPosix::OnConnectionPending(StreamSocketPosix* socket) {
  task_runner_->PostTask([socket, this]() mutable {
    // TODO(issues/71): |this|, |socket| may be invalid at this point.
    ErrorOr<std::unique_ptr<StreamSocket>> accepted = socket->Accept();
    if (accepted.is_error()) {
      // Check for special error code. Because this call doesn't get executed
      // until it gets through the task runner, OnConnectionPending may get
      // called multiple times. This check ensures only the first such call will
      // create a new SSL connection.
      if (accepted.error().code() != Error::Code::kAgain) {
        this->OnError(accepted.error());
      }
      return;
    }

    this->OnSocketAccepted(std::move(accepted.value()));
  });
}

void TlsConnectionFactoryPosix::OnSocketAccepted(
    std::unique_ptr<StreamSocket> socket) {
  TRACE_SCOPED(TraceCategory::SSL,
               "TlsConnectionFactoryPosix::OnSocketAccepted");
  auto connection =
      std::make_unique<TlsConnectionPosix>(std::move(socket), task_runner_);

  if (!ConfigureSsl(connection.get())) {
    return;
  }

  const int connection_status = SSL_accept(connection->ssl_.get());
  if (connection_status != 1) {
    OnConnectionFailed(connection->remote_address());
    TRACE_SET_RESULT(GetSSLError(ssl.get(), connection_status));
    return;
  }

  OnAccepted(std::move(connection));
}

ErrorOr<bssl::UniquePtr<SSL>> TlsConnectionFactoryPosix::GetSslConnection() {
  std::call_once(initInstanceFlag, [this]() { this->Initialize(); });
  if (!ssl_context_.get()) {
    return Error::Code::kFatalSSLError;
  }

  // Create this specific call's SSL Context. Handling of the SSL_CTX is thread
  // safe, so this part doesn't need to be in the mutex.
  SSL* ssl = SSL_new(ssl_context_.get());
  if (ssl == nullptr) {
    return Error::Code::kFatalSSLError;
  }

  return bssl::UniquePtr<SSL>(ssl);
}

void TlsConnectionFactoryPosix::Initialize() {
  EnsureOpenSSLInit();
  SSL_CTX* context = SSL_CTX_new(TLS_method());
  if (context == nullptr) {
    return;
  }

  SSL_CTX_set_mode(context, SSL_MODE_ENABLE_PARTIAL_WRITE);

  ssl_context_.reset(context);
}

}  // namespace platform
}  // namespace openscreen
