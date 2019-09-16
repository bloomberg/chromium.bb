// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/socket_address_posix.h"

#include <vector>

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

SocketAddressPosix::SocketAddressPosix(const struct sockaddr& address) {
  if (address.sa_family == AF_INET) {
    memcpy(&internal_address_, &address, sizeof(struct sockaddr_in));
    endpoint_.address = IPAddress(IPAddress::Version::kV4,
                                  reinterpret_cast<const uint8_t*>(
                                      &internal_address_.v4.sin_addr.s_addr));
    endpoint_.port = ntohs(internal_address_.v4.sin_port);
  } else if (address.sa_family == AF_INET6) {
    memcpy(&internal_address_, &address, sizeof(struct sockaddr_in6));
    endpoint_.address = IPAddress(internal_address_.v6.sin6_addr.s6_addr);
    endpoint_.port = ntohs(internal_address_.v6.sin6_port);
  } else {
    OSP_NOTREACHED() << "Unknown address type";
  }
}

SocketAddressPosix::SocketAddressPosix(const IPEndpoint& endpoint)
    : endpoint_(endpoint) {
  if (endpoint.address.IsV4()) {
    internal_address_.v4.sin_family = AF_INET;
    internal_address_.v4.sin_port = htons(endpoint.port);
    endpoint.address.CopyToV4(
        reinterpret_cast<uint8_t*>(&internal_address_.v4.sin_addr.s_addr));
  } else {
    OSP_DCHECK(endpoint.address.IsV6());

    internal_address_.v6.sin6_family = AF_INET6;
    internal_address_.v6.sin6_flowinfo = 0;
    internal_address_.v6.sin6_scope_id = 0;
    internal_address_.v6.sin6_port = htons(endpoint.port);
    endpoint.address.CopyToV6(
        reinterpret_cast<uint8_t*>(&internal_address_.v6.sin6_addr));
  }
}

struct sockaddr* SocketAddressPosix::address() {
  switch (version()) {
    case IPAddress::Version::kV4:
      return reinterpret_cast<struct sockaddr*>(&internal_address_.v4);
    case IPAddress::Version::kV6:
      return reinterpret_cast<struct sockaddr*>(&internal_address_.v6);
    default:
      OSP_NOTREACHED();
      return nullptr;
  };
}

const struct sockaddr* SocketAddressPosix::address() const {
  switch (version()) {
    case IPAddress::Version::kV4:
      return reinterpret_cast<const struct sockaddr*>(&internal_address_.v4);
    case IPAddress::Version::kV6:
      return reinterpret_cast<const struct sockaddr*>(&internal_address_.v6);
    default:
      OSP_NOTREACHED();
      return nullptr;
  }
}

socklen_t SocketAddressPosix::size() const {
  switch (version()) {
    case IPAddress::Version::kV4:
      return sizeof(struct sockaddr_in);
    case IPAddress::Version::kV6:
      return sizeof(struct sockaddr_in6);
    default:
      OSP_NOTREACHED();
      return 0;
  }
}
}  // namespace platform
}  // namespace openscreen
