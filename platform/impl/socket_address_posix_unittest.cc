// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/socket_address_posix.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace platform {

TEST(SocketAddressPosixTest, IPv4SocketAddressConvertsSuccessfully) {
  const SocketAddressPosix address(IPEndpoint{{1, 2, 3, 4}, 80});

  const sockaddr_in* v4_address =
      reinterpret_cast<const sockaddr_in*>(address.address());

  EXPECT_EQ(v4_address->sin_family, AF_INET);
  EXPECT_EQ(v4_address->sin_port, ntohs(80));

  // 67305985 == 1.2.3.4 in network byte order
  EXPECT_EQ(v4_address->sin_addr.s_addr, 67305985u);

  EXPECT_EQ(address.version(), IPAddress::Version::kV4);
}

TEST(SocketAddressPosixTest, IPv6SocketAddressConvertsSuccessfully) {
  const SocketAddressPosix address(
      IPEndpoint{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}, 80});

  const sockaddr_in6* v6_address =
      reinterpret_cast<const sockaddr_in6*>(address.address());
  EXPECT_EQ(v6_address->sin6_family, AF_INET6);
  EXPECT_EQ(v6_address->sin6_port, ntohs(80));
  EXPECT_EQ(v6_address->sin6_flowinfo, 0u);

  const unsigned char KExpectedAddress[16] = {1, 2,  3,  4,  5,  6,  7,  8,
                                              9, 10, 11, 12, 13, 14, 15, 16};
  EXPECT_THAT(v6_address->sin6_addr.s6_addr,
              testing::ElementsAreArray(KExpectedAddress));
  EXPECT_EQ(v6_address->sin6_scope_id, 0u);

  EXPECT_EQ(address.version(), IPAddress::Version::kV6);
}
}  // namespace platform
}  // namespace openscreen