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
  Error connect_error = connection->socket_.Connect(remote_address);
  if (!connect_error.ok()) {
    TRACE_SET_RESULT(connect_error.error());
    OnConnectionFailed(remote_address);
  }

  ErrorOr<bssl::UniquePtr<SSL>> ssl_or_error = GetSslConnection();
  if (ssl_or_error.is_error()) {
    OnError(ssl_or_error.error());
    TRACE_SET_RESULT(ssl_or_error.error());
    return;
  }

  bssl::UniquePtr<SSL> ssl = ssl_or_error.MoveValue();
  if (!SSL_set_fd(ssl.get(), connection->socket_.socket_handle().fd)) {
    OnConnectionFailed(remote_address);
    TRACE_SET_RESULT(Error(Error::Code::kSocketBindFailure));
    return;
  }

  int connection_status = SSL_connect(ssl.get());
  if (connection_status != 1) {
    OnConnectionFailed(remote_address);
    TRACE_SET_RESULT(GetSSLError(ssl.get(), connection_status));
    return;
  }

  connection->ssl_.swap(ssl);
  OnConnected(std::move(connection));
}

void TlsConnectionFactoryPosix::Listen(const IPEndpoint& local_address,
                                       const TlsCredentials& credentials,
                                       const TlsListenOptions& options) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
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

  auto result = bssl::UniquePtr<SSL>(ssl);
  SSL_CTX_up_ref(ssl_context_.get());
  return result;
}

void TlsConnectionFactoryPosix::Initialize() {
  EnsureOpenSSLInit();
  const SSL_METHOD* ssl_method = TLS_method();
  SSL_CTX* context = SSL_CTX_new(ssl_method);
  if (context == nullptr) {
    return;
  }
  ssl_context_.reset(context);
}

}  // namespace platform
}  // namespace openscreen
