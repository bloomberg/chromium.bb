// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/udp_socket_resource_constants.h"

namespace ppapi {
namespace proxy {

const int32_t UDPSocketResourceConstants::kMaxWriteSize = 128 * 1024;
const int32_t UDPSocketResourceConstants::kMaxReadSize = 128 * 1024;

const int32_t UDPSocketResourceConstants::kMaxSendBufferSize =
    1024 * UDPSocketResourceConstants::kMaxWriteSize;
const int32_t UDPSocketResourceConstants::kMaxReceiveBufferSize =
    1024 * UDPSocketResourceConstants::kMaxReadSize;

const size_t UDPSocketResourceConstants::kPluginSendBufferSlots = 8u;
const size_t UDPSocketResourceConstants::kPluginReceiveBufferSlots = 32u;

}  // namespace proxy
}  // namespace ppapi
