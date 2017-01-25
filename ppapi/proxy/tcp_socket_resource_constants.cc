// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/tcp_socket_resource_constants.h"

namespace ppapi {
namespace proxy {

const int32_t TCPSocketResourceConstants::kMaxReadSize = 1024 * 1024;
const int32_t TCPSocketResourceConstants::kMaxWriteSize = 1024 * 1024;
const int32_t TCPSocketResourceConstants::kMaxSendBufferSize =
    1024 * TCPSocketResourceConstants::kMaxWriteSize;
const int32_t TCPSocketResourceConstants::kMaxReceiveBufferSize =
    1024 * TCPSocketResourceConstants::kMaxReadSize;

}  // namespace proxy
}  // namespace ppapi
