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
                                       TaskRunner* task_runner)
    : TlsConnection(task_runner), socket_(local_address), buffer_(this) {}

TlsConnectionPosix::TlsConnectionPosix(IPAddress::Version version,
                                       TaskRunner* task_runner)
    : TlsConnection(task_runner), socket_(version), buffer_(this) {}

TlsConnectionPosix::~TlsConnectionPosix() = default;

void TlsConnectionPosix::Write(const void* data, size_t len) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

const IPEndpoint& TlsConnectionPosix::local_address() const {
  const absl::optional<IPEndpoint> endpoint = socket_.local_address();
  OSP_DCHECK(endpoint.has_value());
  return endpoint.value();
}

const IPEndpoint& TlsConnectionPosix::remote_address() const {
  const absl::optional<IPEndpoint> endpoint = socket_.remote_address();
  OSP_DCHECK(endpoint.has_value());
  return endpoint.value();
}

void TlsConnectionPosix::NotifyWriteBufferFill(double fraction) {
  constexpr double kBlockBufferPercentage = 0.5;
  if (fraction > kBlockBufferPercentage && !is_buffer_blocked_) {
    OnWriteBlocked();
    is_buffer_blocked_ = true;
  } else if (fraction < kBlockBufferPercentage && is_buffer_blocked_) {
    OnWriteUnblocked();
    is_buffer_blocked_ = false;
  } else if (fraction >= 0.99 && is_buffer_blocked_) {
    OnError(Error::Code::kInsufficientBuffer);
  }
}

}  // namespace platform
}  // namespace openscreen
