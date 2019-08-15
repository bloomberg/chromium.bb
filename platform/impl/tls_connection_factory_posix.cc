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
#include "platform/base/error.h"
#include "platform/impl/stream_socket.h"
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
    : TlsConnectionFactory(client, task_runner) {}

TlsConnectionFactoryPosix::~TlsConnectionFactoryPosix() = default;

void TlsConnectionFactoryPosix::Connect(const IPEndpoint& remote_address,
                                        const TlsConnectOptions& options) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

void TlsConnectionFactoryPosix::Listen(const IPEndpoint& local_address,
                                       const TlsCredentials& credentials,
                                       const TlsListenOptions& options) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}
}  // namespace platform
}  // namespace openscreen
