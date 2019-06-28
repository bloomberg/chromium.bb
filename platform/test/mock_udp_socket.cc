// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/test/mock_udp_socket.h"

namespace openscreen {
namespace platform {

MockUdpSocket::MockUdpSocket(Version version) : version_(version) {}

bool MockUdpSocket::IsIPv4() const {
  return version_ == UdpSocket::Version::kV4;
}

bool MockUdpSocket::IsIPv6() const {
  return version_ == UdpSocket::Version::kV6;
}

}  // namespace platform
}  // namespace openscreen
