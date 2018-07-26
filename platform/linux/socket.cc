// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/socket.h"

#include <linux/in6.h>
#include <linux/ipv6.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <memory>

#include "platform/api/logging.h"
#include "platform/posix/socket.h"

namespace openscreen {
namespace platform {
namespace {

constexpr bool IsPowerOf2(uint32_t x) {
  return (x > 0) && ((x & (x - 1)) == 0);
}

static_assert(IsPowerOf2(alignof(struct cmsghdr)),
              "std::align requires power-of-2 alignment");

}  // namespace

UdpSocketIPv4Ptr CreateUdpSocketIPv4() {
  const int fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
  if (fd == -1) {
    return nullptr;
  }
  return new UdpSocketIPv4Private{fd};
}

UdpSocketIPv6Ptr CreateUdpSocketIPv6() {
  const int fd = socket(AF_INET6, SOCK_DGRAM | SOCK_NONBLOCK, 0);
  if (fd == -1) {
    return nullptr;
  }
  return new UdpSocketIPv6Private{fd};
}

void DestroyUdpSocket(UdpSocketIPv4Ptr socket) {
  DCHECK(socket);
  close(socket->fd);
  delete socket;
}

void DestroyUdpSocket(UdpSocketIPv6Ptr socket) {
  DCHECK(socket);
  close(socket->fd);
  delete socket;
}

bool BindUdpSocketIPv4(UdpSocketIPv4Ptr socket,
                       const IPv4Endpoint& endpoint,
                       int32_t ifindex) {
  DCHECK(socket);
  DCHECK(socket->fd >= 0);

  if (ifindex > 0) {
    struct ip_mreqn multicast_properties;
    // Appropriate address is set based on |imr_ifindex| when set.
    multicast_properties.imr_address.s_addr = INADDR_ANY;
    multicast_properties.imr_multiaddr.s_addr = INADDR_ANY;
    multicast_properties.imr_ifindex = ifindex;
    if (setsockopt(socket->fd, IPPROTO_IP, IP_MULTICAST_IF,
                   &multicast_properties, sizeof(multicast_properties)) == -1) {
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
  address.sin_addr.s_addr = INADDR_ANY;
  return bind(socket->fd, reinterpret_cast<struct sockaddr*>(&address),
              sizeof(address)) != -1;
}

bool BindUdpSocketIPv6(UdpSocketIPv6Ptr socket,
                       const IPv6Endpoint& endpoint,
                       int32_t ifindex) {
  DCHECK(socket);
  DCHECK(socket->fd >= 0);

  if (ifindex > 0) {
    if (setsockopt(socket->fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex,
                   sizeof(ifindex)) == -1) {
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
  address.sin6_addr = IN6ADDR_ANY_INIT;
  address.sin6_scope_id = 0;
  return bind(socket->fd, reinterpret_cast<struct sockaddr*>(&address),
              sizeof(address)) != -1;
}

bool JoinUdpMulticastGroupIPv4(UdpSocketIPv4Ptr socket,
                               const IPv4Address& address,
                               int32_t ifindex) {
  DCHECK(socket);
  DCHECK(socket->fd >= 0);
  // Passed as data to setsockopt().  1 means return IP_PKTINFO control data in
  // recvmsg() calls.
  const int enable_pktinfo = 1;
  if (setsockopt(socket->fd, IPPROTO_IP, IP_PKTINFO, &enable_pktinfo,
                 sizeof(enable_pktinfo)) == -1) {
    return false;
  }
  struct ip_mreqn multicast_properties;
  // Appropriate address is set based on |imr_ifindex| when set.
  multicast_properties.imr_address.s_addr = INADDR_ANY;
  multicast_properties.imr_ifindex = ifindex;
  std::memcpy(&multicast_properties.imr_multiaddr, address.bytes.data(),
              sizeof(multicast_properties.imr_multiaddr));
  if (setsockopt(socket->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                 &multicast_properties, sizeof(multicast_properties)) == -1) {
    return false;
  }
  return true;
}

bool JoinUdpMulticastGroupIPv6(UdpSocketIPv6Ptr socket,
                               const IPv6Address& address,
                               int32_t ifindex) {
  DCHECK(socket);
  DCHECK(socket->fd >= 0);
  // Passed as data to setsockopt().  1 means return IPV6_PKTINFO control data
  // in recvmsg() calls.
  const int enable_pktinfo = 1;
  if (setsockopt(socket->fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &enable_pktinfo,
                 sizeof(enable_pktinfo)) == -1) {
    return false;
  }
  struct ipv6_mreq multicast_properties;
  multicast_properties.ipv6mr_ifindex = ifindex;
  std::memcpy(&multicast_properties.ipv6mr_multiaddr, address.bytes.data(),
              sizeof(multicast_properties.ipv6mr_multiaddr));
  if (setsockopt(socket->fd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP,
                 &multicast_properties, sizeof(multicast_properties)) == -1) {
    return false;
  }
  return true;
}

int64_t ReceiveUdpIPv4(UdpSocketIPv4Ptr socket,
                       void* data,
                       int64_t length,
                       IPv4Endpoint* src,
                       IPv4Endpoint* original_destination) {
  DCHECK(socket);
  DCHECK(socket->fd >= 0);
  DCHECK(src);
  struct iovec iov = {data, static_cast<size_t>(length)};
  struct sockaddr_in sa;
  char control_buf[1024];
  size_t cmsg_size = sizeof(control_buf) - sizeof(struct cmsghdr) + 1;
  void* cmsg_buf = control_buf;
  std::align(alignof(struct cmsghdr), sizeof(cmsg_buf), cmsg_buf, cmsg_size);

  struct msghdr msg;
  msg.msg_name = &sa;
  msg.msg_namelen = sizeof(sa);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsg_buf;
  msg.msg_controllen = cmsg_size;
  msg.msg_flags = 0;

  int64_t len = recvmsg(socket->fd, &msg, 0);
  if (len < 0) {
    return len;
  }
  std::memcpy(src->address.bytes.data(), &sa.sin_addr.s_addr,
              sizeof(sa.sin_addr.s_addr));
  src->port = ntohs(sa.sin_port);

  // For multicast sockets, the packet's original destination address may be the
  // host address (since we called bind()) but it may also be a multicast
  // address.  This may be relevant for handling multicast data; specifically,
  // mDNSResponder requires this information to work properly.
  if (original_destination) {
    *original_destination = IPv4Endpoint{{}, 0};
    if ((msg.msg_flags & MSG_CTRUNC) == 0) {
      for (struct cmsghdr* cmh = CMSG_FIRSTHDR(&msg); cmh;
           cmh = CMSG_NXTHDR(&msg, cmh)) {
        if (cmh->cmsg_level != IPPROTO_IP || cmh->cmsg_type != IP_PKTINFO) {
          continue;
        }
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
        std::memcpy(original_destination->address.bytes.data(),
                    &pktinfo->ipi_addr, sizeof(pktinfo->ipi_addr));
        break;
      }
    }
  }

  return len;
}

int64_t ReceiveUdpIPv6(UdpSocketIPv6Ptr socket,
                       void* data,
                       int64_t length,
                       IPv6Endpoint* src,
                       IPv6Endpoint* original_destination) {
  DCHECK(socket);
  DCHECK(socket->fd >= 0);
  DCHECK(src);
  struct iovec iov = {data, static_cast<size_t>(length)};
  struct sockaddr_in6 sa;
  char control_buf[1024];
  size_t cmsg_size = sizeof(control_buf) - sizeof(struct cmsghdr) + 1;
  void* cmsg_buf = control_buf;
  std::align(alignof(struct cmsghdr), sizeof(cmsg_buf), cmsg_buf, cmsg_size);

  struct msghdr msg;
  msg.msg_name = &sa;
  msg.msg_namelen = sizeof(sa);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsg_buf;
  msg.msg_controllen = cmsg_size;
  msg.msg_flags = 0;

  int64_t len = recvmsg(socket->fd, &msg, 0);
  if (len < 0) {
    return len;
  }
  std::memcpy(src->address.bytes.data(), &sa.sin6_addr.s6_addr,
              sizeof(sa.sin6_addr.s6_addr));
  src->port = ntohs(sa.sin6_port);

  // For multicast sockets, the packet's original destination address may be the
  // host address (since we called bind()) but it may also be a multicast
  // address.  This may be relevant for handling multicast data; specifically,
  // mDNSResponder requires this information to work properly.
  if (original_destination) {
    *original_destination = IPv6Endpoint{{}, 0};
    if ((msg.msg_flags & MSG_CTRUNC) == 0) {
      for (struct cmsghdr* cmh = CMSG_FIRSTHDR(&msg); cmh;
           cmh = CMSG_NXTHDR(&msg, cmh)) {
        if (cmh->cmsg_level != IPPROTO_IPV6 || cmh->cmsg_type != IPV6_PKTINFO) {
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
        std::memcpy(original_destination->address.bytes.data(),
                    &pktinfo->ipi6_addr, sizeof(pktinfo->ipi6_addr));
        break;
      }
    }
  }

  return len;
}

int64_t SendUdpIPv4(UdpSocketIPv4Ptr socket,
                    const void* data,
                    int64_t length,
                    const IPv4Endpoint& dest) {
  DCHECK(socket);
  DCHECK(socket->fd >= 0);
  struct sockaddr_in sa = {
      .sin_family = AF_INET,
      .sin_port = htons(dest.port),
  };
  std::memcpy(&sa.sin_addr.s_addr, dest.address.bytes.data(),
              sizeof(sa.sin_addr.s_addr));
  struct iovec iov = {const_cast<void*>(data), static_cast<size_t>(length)};
  struct msghdr msg;
  msg.msg_name = &sa;
  msg.msg_namelen = sizeof(sa);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = nullptr;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;
  return sendmsg(socket->fd, &msg, 0);
}

int64_t SendUdpIPv6(UdpSocketIPv6Ptr socket,
                    const void* data,
                    int64_t length,
                    const IPv6Endpoint& dest) {
  DCHECK(socket);
  DCHECK(socket->fd >= 0);
  struct sockaddr_in6 sa = {
      .sin6_family = AF_INET6,
      .sin6_port = htons(dest.port),
  };
  std::memcpy(&sa.sin6_addr.s6_addr, dest.address.bytes.data(),
              sizeof(sa.sin6_addr.s6_addr));
  struct iovec iov = {const_cast<void*>(data), static_cast<size_t>(length)};
  struct msghdr msg;
  msg.msg_name = &sa;
  msg.msg_namelen = sizeof(sa);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = nullptr;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;
  return sendmsg(socket->fd, &msg, 0);
}

}  // namespace platform
}  // namespace openscreen
