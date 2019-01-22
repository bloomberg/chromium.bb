// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/socket.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <memory>

#include "platform/api/logging.h"
#include "platform/posix/socket.h"

#include "third_party/abseil/src/absl/types/optional.h"

namespace openscreen {
namespace platform {
namespace {

constexpr bool IsPowerOf2(uint32_t x) {
  return (x > 0) && ((x & (x - 1)) == 0);
}

static_assert(IsPowerOf2(alignof(struct cmsghdr)),
              "std::align requires power-of-2 alignment");

using IPv4NetworkInterfaceIndex = decltype(ip_mreqn().imr_ifindex);
using IPv6NetworkInterfaceIndex = decltype(ipv6_mreq().ipv6mr_interface);

absl::optional<int> CreateNonBlockingUdpSocket(int domain) {
  const int fd = socket(domain, SOCK_DGRAM, 0);
  if (fd == -1) {
    return absl::nullopt;
  }
  // On non-Linux, the SOCK_NONBLOCK option is not available, so use the
  // more-portable method of calling fcntl() to set this behavior.
  if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) == -1) {
    close(fd);
    return absl::nullopt;
  }
  return fd;
}

}  // namespace

UdpSocketPtr CreateUdpSocketIPv4() {
  const absl::optional<int> fd = CreateNonBlockingUdpSocket(AF_INET);
  return fd ? new UdpSocketPrivate{fd.value(), UdpSocketPrivate::Version::kV4}
            : nullptr;
}

UdpSocketPtr CreateUdpSocketIPv6() {
  const absl::optional<int> fd = CreateNonBlockingUdpSocket(AF_INET6);
  return fd ? new UdpSocketPrivate{fd.value(), UdpSocketPrivate::Version::kV6}
            : nullptr;
}

bool IsIPv4Socket(UdpSocketPtr socket) {
  return socket && socket->version == UdpSocketPrivate::Version::kV4;
}

bool IsIPv6Socket(UdpSocketPtr socket) {
  return socket && socket->version == UdpSocketPrivate::Version::kV6;
}

void DestroyUdpSocket(UdpSocketPtr socket) {
  close(socket->fd);
  delete socket;
}

bool BindUdpSocket(UdpSocketPtr socket,
                   const IPEndpoint& endpoint,
                   NetworkInterfaceIndex ifindex) {
  OSP_DCHECK_GE(socket->fd, 0);
  if (socket->version == UdpSocketPrivate::Version::kV4) {
    if (ifindex > 0) {
      struct ip_mreqn multicast_properties;
      // Appropriate address is set based on |imr_ifindex| when set.
      multicast_properties.imr_address.s_addr = INADDR_ANY;
      multicast_properties.imr_multiaddr.s_addr = INADDR_ANY;
      multicast_properties.imr_ifindex =
          static_cast<IPv4NetworkInterfaceIndex>(ifindex);
      if (setsockopt(socket->fd, IPPROTO_IP, IP_MULTICAST_IF,
                     &multicast_properties,
                     sizeof(multicast_properties)) == -1) {
        return false;
      }
      // This is effectively a boolean passed to setsockopt() to allow a future
      // bind() on |socket| to succeed, even if the address is already in use.
      const int reuse_addr = 1;
      if (setsockopt(socket->fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr,
                     sizeof(reuse_addr)) == -1) {
        return false;
      }
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(endpoint.port);
    endpoint.address.CopyToV4(
        reinterpret_cast<uint8_t*>(&address.sin_addr.s_addr));
    return bind(socket->fd, reinterpret_cast<struct sockaddr*>(&address),
                sizeof(address)) != -1;
  } else {
    if (ifindex > 0) {
      const auto index = static_cast<IPv6NetworkInterfaceIndex>(ifindex);
      if (setsockopt(socket->fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &index,
                     sizeof(index)) == -1) {
        return false;
      }
      // This is effectively a boolean passed to setsockopt() to allow a future
      // bind() on |socket| to succeed, even if the address is already in use.
      const int reuse_addr = 1;
      if (setsockopt(socket->fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr,
                     sizeof(reuse_addr)) == -1) {
        return false;
      }
    }

    struct sockaddr_in6 address;
    address.sin6_family = AF_INET6;
    address.sin6_flowinfo = 0;
    address.sin6_port = htons(endpoint.port);
    endpoint.address.CopyToV6(reinterpret_cast<uint8_t*>(&address.sin6_addr));
    address.sin6_scope_id = 0;
    return bind(socket->fd, reinterpret_cast<struct sockaddr*>(&address),
                sizeof(address)) != -1;
  }
}

bool JoinUdpMulticastGroup(UdpSocketPtr socket,
                           const IPAddress& address,
                           NetworkInterfaceIndex ifindex) {
  OSP_DCHECK_GE(socket->fd, 0);
  if (socket->version == UdpSocketPrivate::Version::kV4) {
    // Passed as data to setsockopt().  1 means return IP_PKTINFO control data
    // in recvmsg() calls.
    const int enable_pktinfo = 1;
    if (setsockopt(socket->fd, IPPROTO_IP, IP_PKTINFO, &enable_pktinfo,
                   sizeof(enable_pktinfo)) == -1) {
      return false;
    }
    struct ip_mreqn multicast_properties;
    // Appropriate address is set based on |imr_ifindex| when set.
    multicast_properties.imr_address.s_addr = INADDR_ANY;
    multicast_properties.imr_ifindex =
        static_cast<IPv4NetworkInterfaceIndex>(ifindex);
    static_assert(sizeof(multicast_properties.imr_multiaddr) == 4u,
                  "IPv4 address requires exactly 4 bytes");
    address.CopyToV4(
        reinterpret_cast<uint8_t*>(&multicast_properties.imr_multiaddr));
    if (setsockopt(socket->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   &multicast_properties, sizeof(multicast_properties)) == -1) {
      return false;
    }
    return true;
  } else {
    // Passed as data to setsockopt().  1 means return IPV6_PKTINFO control data
    // in recvmsg() calls.
    const int enable_pktinfo = 1;
    if (setsockopt(socket->fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &enable_pktinfo,
                   sizeof(enable_pktinfo)) == -1) {
      return false;
    }
    struct ipv6_mreq multicast_properties = {
        {/* filled-in below */},
        static_cast<IPv6NetworkInterfaceIndex>(ifindex),
    };
    static_assert(sizeof(multicast_properties.ipv6mr_multiaddr) == 16u,
                  "IPv6 address requires exactly 16 bytes");
    address.CopyToV6(
        reinterpret_cast<uint8_t*>(&multicast_properties.ipv6mr_multiaddr));
    // Portability note: All platforms support IPV6_JOIN_GROUP, which is
    // synonymous with IPV6_ADD_MEMBERSHIP.
    if (setsockopt(socket->fd, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                   &multicast_properties, sizeof(multicast_properties)) == -1) {
      return false;
    }
    return true;
  }
}

absl::optional<int64_t> ReceiveUdp(UdpSocketPtr socket,
                                   void* data,
                                   int64_t length,
                                   IPEndpoint* src,
                                   IPEndpoint* original_destination) {
  OSP_DCHECK_GE(socket->fd, 0);
  struct iovec iov = {data, static_cast<size_t>(length)};
  char control_buf[1024];
  size_t cmsg_size = sizeof(control_buf) - sizeof(struct cmsghdr) + 1;
  void* cmsg_buf = control_buf;
  std::align(alignof(struct cmsghdr), sizeof(cmsg_buf), cmsg_buf, cmsg_size);
  if (socket->version == UdpSocketPrivate::Version::kV4) {
    struct sockaddr_in sa;
    struct msghdr msg;
    msg.msg_name = &sa;
    msg.msg_namelen = sizeof(sa);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = cmsg_size;
    msg.msg_flags = 0;

    int64_t len = recvmsg(socket->fd, &msg, 0);
    if (len < 0)
      return absl::nullopt;

    src->address =
        IPAddress(IPAddress::Version::kV4,
                  reinterpret_cast<const uint8_t*>(&sa.sin_addr.s_addr));
    src->port = ntohs(sa.sin_port);

    // For multicast sockets, the packet's original destination address may be
    // the host address (since we called bind()) but it may also be a multicast
    // address.  This may be relevant for handling multicast data; specifically,
    // mDNSResponder requires this information to work properly.
    if (original_destination) {
      *original_destination = IPEndpoint{{}, 0};
      if ((msg.msg_flags & MSG_CTRUNC) == 0) {
        for (struct cmsghdr* cmh = CMSG_FIRSTHDR(&msg); cmh;
             cmh = CMSG_NXTHDR(&msg, cmh)) {
          if (cmh->cmsg_level != IPPROTO_IP || cmh->cmsg_type != IP_PKTINFO)
            continue;

          struct sockaddr_in addr;
          socklen_t len = sizeof(addr);
          if (getsockname(socket->fd, reinterpret_cast<struct sockaddr*>(&addr),
                          &len) == -1) {
            break;
          }
          // |original_destination->port| will be 0 if this line isn't reached.
          original_destination->port = ntohs(addr.sin_port);

          struct in_pktinfo* pktinfo =
              reinterpret_cast<struct in_pktinfo*>(CMSG_DATA(cmh));
          original_destination->address =
              IPAddress(IPAddress::Version::kV4,
                        reinterpret_cast<const uint8_t*>(&pktinfo->ipi_addr));
          break;
        }
      }
    }

    return absl::optional<int>(len);
  } else {
    struct sockaddr_in6 sa;
    struct msghdr msg;
    msg.msg_name = &sa;
    msg.msg_namelen = sizeof(sa);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = cmsg_size;
    msg.msg_flags = 0;

    int64_t len = recvmsg(socket->fd, &msg, 0);
    if (len < 0)
      return len;

    src->address =
        IPAddress(IPAddress::Version::kV6,
                  reinterpret_cast<const uint8_t*>(&sa.sin6_addr.s6_addr));
    src->port = ntohs(sa.sin6_port);

    // For multicast sockets, the packet's original destination address may be
    // the host address (since we called bind()) but it may also be a multicast
    // address.  This may be relevant for handling multicast data; specifically,
    // mDNSResponder requires this information to work properly.
    if (original_destination) {
      *original_destination = IPEndpoint{{}, 0};
      if ((msg.msg_flags & MSG_CTRUNC) == 0) {
        for (struct cmsghdr* cmh = CMSG_FIRSTHDR(&msg); cmh;
             cmh = CMSG_NXTHDR(&msg, cmh)) {
          if (cmh->cmsg_level != IPPROTO_IPV6 ||
              cmh->cmsg_type != IPV6_PKTINFO) {
            continue;
          }
          struct sockaddr_in6 addr;
          socklen_t len = sizeof(addr);
          if (getsockname(socket->fd, reinterpret_cast<struct sockaddr*>(&addr),
                          &len) == -1) {
            break;
          }
          // |original_destination->port| will be 0 if this line isn't reached.
          original_destination->port = ntohs(addr.sin6_port);

          struct in6_pktinfo* pktinfo =
              reinterpret_cast<struct in6_pktinfo*>(CMSG_DATA(cmh));
          original_destination->address =
              IPAddress(IPAddress::Version::kV6,
                        reinterpret_cast<const uint8_t*>(&pktinfo->ipi6_addr));
          break;
        }
      }
    }

    return len;
  }
}

absl::optional<int64_t> SendUdp(UdpSocketPtr socket,
                                const void* data,
                                int64_t length,
                                const IPEndpoint& dest) {
  OSP_DCHECK_GE(socket->fd, 0);

  struct msghdr msg;
  msg.msg_iovlen = 1;
  msg.msg_control = nullptr;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;

  if (socket->version == UdpSocketPrivate::Version::kV4) {
    struct sockaddr_in sa = {
        .sin_family = AF_INET,
        .sin_port = htons(dest.port),
    };
    msg.msg_name = &sa;
    msg.msg_namelen = sizeof(sa);

    dest.address.CopyToV4(reinterpret_cast<uint8_t*>(&sa.sin_addr.s_addr));
    struct iovec iov = {const_cast<void*>(data), static_cast<size_t>(length)};
    msg.msg_iov = &iov;
  } else {
    struct sockaddr_in6 sa = {
        .sin6_family = AF_INET6,
        .sin6_port = htons(dest.port),
    };
    msg.msg_name = &sa;
    msg.msg_namelen = sizeof(sa);

    dest.address.CopyToV6(reinterpret_cast<uint8_t*>(&sa.sin6_addr.s6_addr));
    struct iovec iov = {const_cast<void*>(data), static_cast<size_t>(length)};
    msg.msg_iov = &iov;
  }

  const int64_t return_value = sendmsg(socket->fd, &msg, 0);
  return (return_value >= 0) ? absl::optional<int>(return_value)
                             : absl::optional<int>();
}

}  // namespace platform
}  // namespace openscreen
