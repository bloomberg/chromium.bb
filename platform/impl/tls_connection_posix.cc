// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_connection_posix.h"

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
#include "platform/base/error.h"
#include "platform/impl/stream_socket.h"
#include "util/crypto/openssl_util.h"

namespace openscreen {
namespace platform {

// TODO(jophba, rwkeane): implement reading
// TODO(jophba, rwkeane): implement write blocking/unblocking
TlsConnectionPosix::TlsConnectionPosix(IPEndpoint local_address,
                                       IPEndpoint remote_address,
                                       TaskRunner* task_runner)
    : TlsConnection(task_runner),
      local_address_(local_address),
      remote_address_(remote_address),
      socket_(std::make_unique<StreamSocketPosix>(local_address)) {}

TlsConnectionPosix::~TlsConnectionPosix() = default;

void TlsConnectionPosix::Write(const void* data, size_t len) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

const IPEndpoint& TlsConnectionPosix::local_address() const {
  return local_address_;
}

const IPEndpoint& TlsConnectionPosix::remote_address() const {
  return remote_address_;
}

}  // namespace platform
}  // namespace openscreen
