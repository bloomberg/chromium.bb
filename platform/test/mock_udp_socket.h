// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_TEST_MOCK_UDP_SOCKET_H_
#define PLATFORM_TEST_MOCK_UDP_SOCKET_H_

#include <algorithm>
#include <memory>

#include "gmock/gmock.h"
#include "platform/api/logging.h"
#include "platform/api/udp_socket.h"

namespace openscreen {
namespace platform {

class MockUdpSocket : public UdpSocket {
 public:
  MockUdpSocket(Version version);
  ~MockUdpSocket() override = default;

  bool IsIPv4() const override;
  bool IsIPv6() const override;

  MOCK_METHOD1(Bind, Error(const IPEndpoint&));
  MOCK_METHOD1(SetMulticastOutboundInterface, Error(NetworkInterfaceIndex));
  MOCK_METHOD2(JoinMulticastGroup,
               Error(const IPAddress&, NetworkInterfaceIndex));
  MOCK_METHOD0(ReceiveMessage, ErrorOr<UdpPacket>());
  MOCK_METHOD3(SendMessage, Error(const void*, size_t, const IPEndpoint&));
  MOCK_METHOD1(SetDscp, Error(DscpMode));

 private:
  Version version_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_TEST_MOCK_UDP_SOCKET_H_
