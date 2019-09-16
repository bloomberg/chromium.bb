// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_TLS_CONNECTION_POSIX_H_
#define PLATFORM_IMPL_TLS_CONNECTION_POSIX_H_

#include <openssl/ssl.h>

#include <memory>
#include <string>
#include <vector>

#include "platform/api/task_runner.h"
#include "platform/api/tls_connection.h"
#include "platform/base/socket_state.h"
#include "platform/impl/ssl_context.h"
#include "platform/impl/stream_socket_posix.h"

namespace openscreen {
namespace platform {

class TlsConnectionPosix : public TlsConnection {
 public:
  TlsConnectionPosix(IPEndpoint local_address, TaskRunner* task_runner);
  TlsConnectionPosix(IPAddress::Version version, TaskRunner* task_runner);
  ~TlsConnectionPosix();

  // TlsConnection overrides
  void Write(const void* data, size_t len) override;
  const IPEndpoint& local_address() const override;
  const IPEndpoint& remote_address() const override;

 private:
  StreamSocketPosix socket_;
  bssl::UniquePtr<SSL> ssl_;

  friend class TlsConnectionFactoryPosix;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_TLS_CONNECTION_POSIX_H_
