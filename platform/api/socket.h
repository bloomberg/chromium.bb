// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_SOCKET_H_
#define PLATFORM_API_SOCKET_H_

#include "base/ip_address.h"

namespace openscreen {
namespace platform {

// Opaque type for the platform to implement.
struct UdpSocketIPv4Private;
using UdpSocketIPv4Ptr = UdpSocketIPv4Private*;
struct UdpSocketIPv6Private;
using UdpSocketIPv6Ptr = UdpSocketIPv6Private*;

UdpSocketIPv4Ptr CreateUdpSocketIPv4();
UdpSocketIPv6Ptr CreateUdpSocketIPv6();

// Closes the underlying platform socket and frees any allocated memory.
void DestroyUdpSocket(UdpSocketIPv4Ptr socket);
void DestroyUdpSocket(UdpSocketIPv6Ptr socket);

bool BindUdpSocketIPv4(UdpSocketIPv4Ptr socket,
                       const IPv4Endpoint& endpoint,
                       int32_t ifindex);
bool BindUdpSocketIPv6(UdpSocketIPv6Ptr socket,
                       const IPv6Endpoint& endpoint,
                       int32_t ifindex);
bool JoinUdpMulticastGroupIPv4(UdpSocketIPv4Ptr socket,
                               const IPv4Address& address,
                               int32_t ifindex);
bool JoinUdpMulticastGroupIPv6(UdpSocketIPv6Ptr socket,
                               const IPv6Address& address,
                               int32_t ifindex);

int64_t ReceiveUdpIPv4(UdpSocketIPv4Ptr socket,
                       void* data,
                       int64_t length,
                       IPv4Endpoint* src,
                       IPv4Endpoint* original_destination);
int64_t ReceiveUdpIPv6(UdpSocketIPv6Ptr socket,
                       void* data,
                       int64_t length,
                       IPv6Endpoint* src,
                       IPv6Endpoint* original_destination);
int64_t SendUdpIPv4(UdpSocketIPv4Ptr socket,
                    const void* data,
                    int64_t length,
                    const IPv4Endpoint& dest);
int64_t SendUdpIPv6(UdpSocketIPv6Ptr socket,
                    const void* data,
                    int64_t length,
                    const IPv6Endpoint& dest);

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_SOCKET_H_
