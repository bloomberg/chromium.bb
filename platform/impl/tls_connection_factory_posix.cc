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

#include "platform/api/logging.h"
#include "platform/api/task_runner.h"
#include "platform/api/tls_connection_factory.h"
#include "platform/api/trace_logging.h"
#include "platform/base/tls_connect_options.h"
#include "platform/base/tls_credentials.h"
#include "platform/base/tls_listen_options.h"
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

TlsConnectionFactoryPosix::TlsConnectionFactoryPosix(
    Client* client,
    TaskRunner* task_runner,
    PlatformClientPosix* platform_client)
    : client_(client),
      task_runner_(task_runner),
      platform_client_(platform_client) {
  OSP_DCHECK(client_);
  OSP_DCHECK(task_runner_);
}

TlsConnectionFactoryPosix::~TlsConnectionFactoryPosix() = default;

// TODO(rwkeane): Add support for resuming sessions.
// TODO(rwkeane): Integrate with Auth.
void TlsConnectionFactoryPosix::Connect(const IPEndpoint& remote_address,
                                        const TlsConnectOptions& options) {
  TRACE_SCOPED(TraceCategory::SSL, "TlsConnectionFactoryPosix::Connect");
  IPAddress::Version version = remote_address.address.version();
  std::unique_ptr<TlsConnectionPosix> connection(
      new TlsConnectionPosix(version, task_runner_, platform_client_));
  Error connect_error = connection->socket_->Connect(remote_address);
  if (!connect_error.ok()) {
    TRACE_SET_RESULT(connect_error.error());
    DispatchConnectionFailed(remote_address);
    return;
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
    DispatchConnectionFailed(connection->GetRemoteEndpoint());
    TRACE_SET_RESULT(GetSSLError(connection->ssl_.get(), connection_status));
    return;
  }

  task_runner_->PostTask([weak_this = weak_factory_.GetWeakPtr(),
                          moved_connection = std::move(connection)]() mutable {
    if (auto* self = weak_this.get()) {
      X509* peer_cert = SSL_get_peer_certificate(moved_connection->ssl_.get());
      self->client_->OnConnected(self, peer_cert, std::move(moved_connection));
    }
  });
}

void TlsConnectionFactoryPosix::SetListenCredentials(
    const TlsCredentials& credentials) {
  EnsureInitialized();

  // We don't really change the certificate, but we do apply ref counting to
  // it, so a const cast is unfortunately necessary.
  X509* non_const_cert = const_cast<X509*>(&credentials.certificate());
  if (SSL_CTX_use_certificate(ssl_context_.get(), non_const_cert) != 1) {
    DispatchError(Error::Code::kSocketListenFailure);
    TRACE_SET_RESULT(Error::Code::kSocketListenFailure);
    return;
  }

  listen_credentials_set_ = true;
}

void TlsConnectionFactoryPosix::Listen(const IPEndpoint& local_address,
                                       const TlsListenOptions& options) {
  // Credentials must be set before Listen() is called.
  OSP_DCHECK(listen_credentials_set_);

  auto socket = std::make_unique<StreamSocketPosix>(local_address);
  socket->Listen(options.backlog_size);

  OSP_DCHECK(platform_client_);
  if (platform_client_) {
    platform_client_->tls_data_router()->RegisterSocketObserver(
        std::move(socket), this);
  }
}

void TlsConnectionFactoryPosix::OnConnectionPending(StreamSocketPosix* socket) {
  task_runner_->PostTask([connection_factory_weak_ptr =
                              weak_factory_.GetWeakPtr(),
                          socket_weak_ptr = socket->GetWeakPtr()] {
    if (!connection_factory_weak_ptr || !socket_weak_ptr) {
      // Cancel the Accept() since either the factory or the listener socket
      // went away before this task has run.
      return;
    }

    ErrorOr<std::unique_ptr<StreamSocket>> accepted = socket_weak_ptr->Accept();
    if (accepted.is_error()) {
      // Check for special error code. Because this call doesn't get executed
      // until it gets through the task runner, OnConnectionPending may get
      // called multiple times. This check ensures only the first such call will
      // create a new SSL connection.
      if (accepted.error().code() != Error::Code::kAgain) {
        connection_factory_weak_ptr->DispatchError(std::move(accepted.error()));
      }
      return;
    }

    connection_factory_weak_ptr->OnSocketAccepted(std::move(accepted.value()));
  });
}

void TlsConnectionFactoryPosix::OnSocketAccepted(
    std::unique_ptr<StreamSocket> socket) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  TRACE_SCOPED(TraceCategory::SSL,
               "TlsConnectionFactoryPosix::OnSocketAccepted");
  std::unique_ptr<TlsConnectionPosix> connection(new TlsConnectionPosix(
      std::move(socket), task_runner_, platform_client_));

  if (!ConfigureSsl(connection.get())) {
    return;
  }

  const int connection_status = SSL_accept(connection->ssl_.get());
  if (connection_status != 1) {
    DispatchConnectionFailed(connection->GetRemoteEndpoint());
    TRACE_SET_RESULT(GetSSLError(ssl.get(), connection_status));
    return;
  }

  task_runner_->PostTask([weak_this = weak_factory_.GetWeakPtr(),
                          moved_connection = std::move(connection)]() mutable {
    if (auto* self = weak_this.get()) {
      X509* peer_cert = SSL_get_peer_certificate(moved_connection->ssl_.get());
      self->client_->OnAccepted(self, peer_cert, std::move(moved_connection));
    }
  });
}

bool TlsConnectionFactoryPosix::ConfigureSsl(TlsConnectionPosix* connection) {
  ErrorOr<bssl::UniquePtr<SSL>> connection_result = GetSslConnection();
  if (connection_result.is_error()) {
    DispatchError(connection_result.error());
    TRACE_SET_RESULT(connection_result.error());
    return false;
  }

  bssl::UniquePtr<SSL> ssl = std::move(connection_result.value());
  if (!SSL_set_fd(ssl.get(), connection->socket_->socket_handle().fd)) {
    DispatchConnectionFailed(connection->GetRemoteEndpoint());
    TRACE_SET_RESULT(Error(Error::Code::kSocketBindFailure));
    return false;
  }

  connection->ssl_.swap(ssl);
  return true;
}

ErrorOr<bssl::UniquePtr<SSL>> TlsConnectionFactoryPosix::GetSslConnection() {
  EnsureInitialized();
  if (!ssl_context_.get()) {
    return Error::Code::kFatalSSLError;
  }

  SSL* ssl = SSL_new(ssl_context_.get());
  if (ssl == nullptr) {
    return Error::Code::kFatalSSLError;
  }

  return bssl::UniquePtr<SSL>(ssl);
}

void TlsConnectionFactoryPosix::EnsureInitialized() {
  std::call_once(init_instance_flag_, [this]() { this->Initialize(); });
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

void TlsConnectionFactoryPosix::DispatchConnectionFailed(
    const IPEndpoint& remote_endpoint) {
  task_runner_->PostTask(
      [weak_this = weak_factory_.GetWeakPtr(), remote = remote_endpoint] {
        if (auto* self = weak_this.get()) {
          self->client_->OnConnectionFailed(self, remote);
        }
      });
}

void TlsConnectionFactoryPosix::DispatchError(Error error) {
  task_runner_->PostTask([weak_this = weak_factory_.GetWeakPtr(),
                          moved_error = std::move(error)]() mutable {
    if (auto* self = weak_this.get()) {
      self->client_->OnError(self, std::move(moved_error));
    }
  });
}

}  // namespace platform
}  // namespace openscreen
