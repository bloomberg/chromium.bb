// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_SOCKET_H_
#define PLATFORM_API_SOCKET_H_

#include "base/ip_address.h"
#include "platform/api/network_interface.h"
#include "third_party/abseil/src/absl/types/optional.h"

namespace openscreen {
namespace platform {

// Opaque type for the platform to implement.
struct UdpSocketPrivate;
using UdpSocketPtr = UdpSocketPrivate*;

// TODO(miu): These should return std::unique_ptr<>, so that code structure
// auto-scopes the lifetime of the instance (i.e., auto-closing the socket too).
UdpSocketPtr CreateUdpSocketIPv4();
UdpSocketPtr CreateUdpSocketIPv6();

// Returns true if |socket| is not null and it belongs to the IPv4/IPv6 address
// family.
bool IsIPv4Socket(UdpSocketPtr socket);
bool IsIPv6Socket(UdpSocketPtr socket);

// Closes the underlying platform socket and frees any allocated memory.
void DestroyUdpSocket(UdpSocketPtr socket);

bool BindUdpSocket(UdpSocketPtr socket,
                   const IPEndpoint& endpoint,
                   InterfaceIndex ifindex);
bool JoinUdpMulticastGroup(UdpSocketPtr socket,
                           const IPAddress& address,
                           InterfaceIndex ifindex);

absl::optional<int64_t> ReceiveUdp(UdpSocketPtr socket,
                                   void* data,
                                   int64_t length,
                                   IPEndpoint* src,
                                   IPEndpoint* original_destination);
absl::optional<int64_t> SendUdp(UdpSocketPtr socket,
                                const void* data,
                                int64_t length,
                                const IPEndpoint& dest);

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_SOCKET_H_
