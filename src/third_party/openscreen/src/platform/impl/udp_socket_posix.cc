// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/udp_socket_posix.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <memory>

#include "absl/types/optional.h"
#include "platform/api/logging.h"
#include "platform/base/error.h"

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

ErrorOr<int> CreateNonBlockingUdpSocket(int domain) {
  int fd = socket(domain, SOCK_DGRAM, 0);
  if (fd == -1) {
    return Error(Error::Code::kInitializationFailure, strerror(errno));
  }
  // On non-Linux, the SOCK_NONBLOCK option is not available, so use the
  // more-portable method of calling fcntl() to set this behavior.
  if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) == -1) {
    close(fd);
    return Error(Error::Code::kInitializationFailure, strerror(errno));
  }
  return fd;
}

}  // namespace

UdpSocketPosix::UdpSocketPosix(TaskRunner* task_runner,
                               Client* client,
                               int fd,
                               const IPEndpoint& local_endpoint)
    : UdpSocket(task_runner, client), fd_(fd), local_endpoint_(local_endpoint) {
  OSP_DCHECK(local_endpoint_.address.IsV4() || local_endpoint_.address.IsV6());
}

UdpSocketPosix::~UdpSocketPosix() {
  close(fd_);
}

// static
ErrorOr<UdpSocketUniquePtr> UdpSocket::Create(TaskRunner* task_runner,
                                              Client* client,
                                              const IPEndpoint& endpoint) {
  int domain;
  switch (endpoint.address.version()) {
    case Version::kV4:
      domain = AF_INET;
      break;
    case Version::kV6:
      domain = AF_INET6;
      break;
  }
  const ErrorOr<int> fd = CreateNonBlockingUdpSocket(domain);
  if (!fd) {
    return fd.error();
  }
  return UdpSocketUniquePtr(static_cast<UdpSocket*>(
      new UdpSocketPosix(task_runner, client, fd.value(), endpoint)));
}

bool UdpSocketPosix::IsIPv4() const {
  return local_endpoint_.address.IsV4();
}

bool UdpSocketPosix::IsIPv6() const {
  return local_endpoint_.address.IsV6();
}

IPEndpoint UdpSocketPosix::GetLocalEndpoint() const {
  if (local_endpoint_.port == 0) {
    // Note: If the getsockname() call fails, just assume that's because the
    // socket isn't bound yet. In this case, leave the original value in-place.
    switch (local_endpoint_.address.version()) {
      case UdpSocket::Version::kV4: {
        struct sockaddr_in address;
        socklen_t address_len = sizeof(address);
        if (getsockname(fd_, reinterpret_cast<struct sockaddr*>(&address),
                        &address_len) == 0) {
          OSP_DCHECK_EQ(address.sin_family, AF_INET);
          local_endpoint_.address =
              IPAddress(IPAddress::Version::kV4,
                        reinterpret_cast<uint8_t*>(&address.sin_addr.s_addr));
          local_endpoint_.port = ntohs(address.sin_port);
        }
        break;
      }

      case UdpSocket::Version::kV6: {
        struct sockaddr_in6 address;
        socklen_t address_len = sizeof(address);
        if (getsockname(fd_, reinterpret_cast<struct sockaddr*>(&address),
                        &address_len) == 0) {
          OSP_DCHECK_EQ(address.sin6_family, AF_INET6);
          local_endpoint_.address =
              IPAddress(IPAddress::Version::kV6,
                        reinterpret_cast<uint8_t*>(&address.sin6_addr));
          local_endpoint_.port = ntohs(address.sin6_port);
        }
        break;
      }
    }
  }

  return local_endpoint_;
}

Error UdpSocketPosix::Bind() {
  // This is effectively a boolean passed to setsockopt() to allow a future
  // bind() on the same socket to succeed, even if the address is already in
  // use. This is pretty much universally the desired behavior.
  const int reuse_addr = 1;
  if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &reuse_addr,
                 sizeof(reuse_addr)) == -1) {
    return Error(Error::Code::kSocketOptionSettingFailure, strerror(errno));
  }

  switch (local_endpoint_.address.version()) {
    case UdpSocket::Version::kV4: {
      struct sockaddr_in address;
      address.sin_family = AF_INET;
      address.sin_port = htons(local_endpoint_.port);
      local_endpoint_.address.CopyToV4(
          reinterpret_cast<uint8_t*>(&address.sin_addr.s_addr));
      if (bind(fd_, reinterpret_cast<struct sockaddr*>(&address),
               sizeof(address)) == -1) {
        return Error(Error::Code::kSocketBindFailure, strerror(errno));
      }
      return Error::Code::kNone;
    }

    case UdpSocket::Version::kV6: {
      struct sockaddr_in6 address;
      address.sin6_family = AF_INET6;
      address.sin6_flowinfo = 0;
      address.sin6_port = htons(local_endpoint_.port);
      local_endpoint_.address.CopyToV6(
          reinterpret_cast<uint8_t*>(&address.sin6_addr));
      address.sin6_scope_id = 0;
      if (bind(fd_, reinterpret_cast<struct sockaddr*>(&address),
               sizeof(address)) == -1) {
        return Error(Error::Code::kSocketBindFailure, strerror(errno));
      }
      return Error::Code::kNone;
    }
  }

  OSP_NOTREACHED();
  return Error::Code::kUnknownError;
}

Error UdpSocketPosix::SetMulticastOutboundInterface(
    NetworkInterfaceIndex ifindex) {
  switch (local_endpoint_.address.version()) {
    case UdpSocket::Version::kV4: {
      struct ip_mreqn multicast_properties;
      // Appropriate address is set based on |imr_ifindex| when set.
      multicast_properties.imr_address.s_addr = INADDR_ANY;
      multicast_properties.imr_multiaddr.s_addr = INADDR_ANY;
      multicast_properties.imr_ifindex =
          static_cast<IPv4NetworkInterfaceIndex>(ifindex);
      if (setsockopt(fd_, IPPROTO_IP, IP_MULTICAST_IF, &multicast_properties,
                     sizeof(multicast_properties)) == -1) {
        return Error(Error::Code::kSocketOptionSettingFailure, strerror(errno));
      }
      return Error::Code::kNone;
    }

    case UdpSocket::Version::kV6: {
      const auto index = static_cast<IPv6NetworkInterfaceIndex>(ifindex);
      if (setsockopt(fd_, IPPROTO_IPV6, IPV6_MULTICAST_IF, &index,
                     sizeof(index)) == -1) {
        return Error(Error::Code::kSocketOptionSettingFailure, strerror(errno));
      }
      return Error::Code::kNone;
    }
  }

  OSP_NOTREACHED();
  return Error::Code::kUnknownError;
}

Error UdpSocketPosix::JoinMulticastGroup(const IPAddress& address,
                                         NetworkInterfaceIndex ifindex) {
  switch (local_endpoint_.address.version()) {
    case UdpSocket::Version::kV4: {
      // Passed as data to setsockopt().  1 means return IP_PKTINFO control data
      // in recvmsg() calls.
      const int enable_pktinfo = 1;
      if (setsockopt(fd_, IPPROTO_IP, IP_PKTINFO, &enable_pktinfo,
                     sizeof(enable_pktinfo)) == -1) {
        return Error(Error::Code::kSocketOptionSettingFailure, strerror(errno));
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
      if (setsockopt(fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &multicast_properties,
                     sizeof(multicast_properties)) == -1) {
        return Error(Error::Code::kSocketOptionSettingFailure, strerror(errno));
      }
      return Error::Code::kNone;
    }

    case UdpSocket::Version::kV6: {
      // Passed as data to setsockopt().  1 means return IPV6_PKTINFO control
      // data in recvmsg() calls.
      const int enable_pktinfo = 1;
      if (setsockopt(fd_, IPPROTO_IPV6, IPV6_RECVPKTINFO, &enable_pktinfo,
                     sizeof(enable_pktinfo)) == -1) {
        return Error(Error::Code::kSocketOptionSettingFailure, strerror(errno));
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
      if (setsockopt(fd_, IPPROTO_IPV6, IPV6_JOIN_GROUP, &multicast_properties,
                     sizeof(multicast_properties)) == -1) {
        return Error(Error::Code::kSocketOptionSettingFailure, strerror(errno));
      }
      return Error::Code::kNone;
    }
  }

  OSP_NOTREACHED();
  return Error::Code::kUnknownError;
}

namespace {

// Examine |posix_errno| to determine whether the specific cause of a failure
// was transient or hard, and return the appropriate error response.
Error ChooseError(decltype(errno) posix_errno, Error::Code hard_error_code) {
  if (posix_errno == EAGAIN || posix_errno == EWOULDBLOCK ||
      posix_errno == ENOBUFS) {
    return Error(Error::Code::kAgain, strerror(errno));
  }
  return Error(hard_error_code, strerror(errno));
}

IPAddress GetIPAddressFromSockAddr(const sockaddr_in& sa) {
  static_assert(IPAddress::kV4Size == sizeof(sa.sin_addr.s_addr),
                "IPv4 address size mismatch.");
  return IPAddress(IPAddress::Version::kV4,
                   reinterpret_cast<const uint8_t*>(&sa.sin_addr.s_addr));
}

IPAddress GetIPAddressFromPktInfo(const in_pktinfo& pktinfo) {
  static_assert(IPAddress::kV4Size == sizeof(pktinfo.ipi_addr),
                "IPv4 address size mismatch.");
  return IPAddress(IPAddress::Version::kV4,
                   reinterpret_cast<const uint8_t*>(&pktinfo.ipi_addr));
}

uint16_t GetPortFromFromSockAddr(const sockaddr_in& sa) {
  return ntohs(sa.sin_port);
}

IPAddress GetIPAddressFromSockAddr(const sockaddr_in6& sa) {
  return IPAddress(sa.sin6_addr.s6_addr);
}

IPAddress GetIPAddressFromPktInfo(const in6_pktinfo& pktinfo) {
  return IPAddress(pktinfo.ipi6_addr.s6_addr);
}

uint16_t GetPortFromFromSockAddr(const sockaddr_in6& sa) {
  return ntohs(sa.sin6_port);
}

template <class PktInfoType>
bool IsPacketInfo(cmsghdr* cmh);

template <>
bool IsPacketInfo<in_pktinfo>(cmsghdr* cmh) {
  return cmh->cmsg_level == IPPROTO_IP && cmh->cmsg_type == IP_PKTINFO;
}

template <>
bool IsPacketInfo<in6_pktinfo>(cmsghdr* cmh) {
  return cmh->cmsg_level == IPPROTO_IPV6 && cmh->cmsg_type == IPV6_PKTINFO;
}

template <class SockAddrType, class PktInfoType>
Error ReceiveMessageInternal(int fd, UdpPacket* packet) {
  SockAddrType sa;
  iovec iov = {packet->data(), packet->size()};
  alignas(alignof(cmsghdr)) uint8_t control_buffer[1024];
  msghdr msg;
  msg.msg_name = &sa;
  msg.msg_namelen = sizeof(sa);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = control_buffer;
  msg.msg_controllen = sizeof(control_buffer);
  msg.msg_flags = 0;

  ssize_t bytes_received = recvmsg(fd, &msg, 0);
  if (bytes_received == -1) {
    return ChooseError(errno, Error::Code::kSocketReadFailure);
  }

  OSP_DCHECK_EQ(static_cast<size_t>(bytes_received), packet->size());

  IPEndpoint source_endpoint = {.address = GetIPAddressFromSockAddr(sa),
                                .port = GetPortFromFromSockAddr(sa)};
  packet->set_source(std::move(source_endpoint));

  // For multicast sockets, the packet's original destination address may be
  // the host address (since we called bind()) but it may also be a
  // multicast address.  This may be relevant for handling multicast data;
  // specifically, mDNSResponder requires this information to work properly.

  socklen_t sa_len = sizeof(sa);
  if (((msg.msg_flags & MSG_CTRUNC) != 0) ||
      (getsockname(fd, reinterpret_cast<sockaddr*>(&sa), &sa_len) == -1)) {
    return Error::Code::kNone;
  }
  for (cmsghdr* cmh = CMSG_FIRSTHDR(&msg); cmh; cmh = CMSG_NXTHDR(&msg, cmh)) {
    if (IsPacketInfo<PktInfoType>(cmh)) {
      PktInfoType* pktinfo = reinterpret_cast<PktInfoType*>(CMSG_DATA(cmh));
      IPEndpoint destination_endpoint = {
          .address = GetIPAddressFromPktInfo(*pktinfo),
          .port = GetPortFromFromSockAddr(sa)};
      packet->set_destination(std::move(destination_endpoint));
      break;
    }
  }
  return Error::Code::kNone;
}

}  // namespace

ErrorOr<UdpPacket> UdpSocketPosix::ReceiveMessage() {
  ssize_t bytes_available = recv(fd_, nullptr, 0, MSG_PEEK | MSG_TRUNC);
  if (bytes_available == -1) {
    return ChooseError(errno, Error::Code::kSocketReadFailure);
  }
  UdpPacket packet(bytes_available);
  packet.set_socket(this);
  Error result = Error::Code::kUnknownError;
  switch (local_endpoint_.address.version()) {
    case UdpSocket::Version::kV4: {
      result = ReceiveMessageInternal<sockaddr_in, in_pktinfo>(fd_, &packet);
      break;
    }
    case UdpSocket::Version::kV6: {
      result = ReceiveMessageInternal<sockaddr_in6, in6_pktinfo>(fd_, &packet);
      break;
    }
    default: {
      OSP_NOTREACHED();
    }
  }
  return result.ok() ? ErrorOr<UdpPacket>(std::move(packet))
                     : ErrorOr<UdpPacket>(std::move(result));
}

// TODO(yakimakha): Consider changing the interface to accept UdpPacket as
// an input parameter.
Error UdpSocketPosix::SendMessage(const void* data,
                                  size_t length,
                                  const IPEndpoint& dest) {
  struct iovec iov = {const_cast<void*>(data), length};
  struct msghdr msg;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = nullptr;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;

  ssize_t num_bytes_sent = -2;
  switch (local_endpoint_.address.version()) {
    case UdpSocket::Version::kV4: {
      struct sockaddr_in sa = {
          .sin_family = AF_INET,
          .sin_port = htons(dest.port),
      };
      dest.address.CopyToV4(reinterpret_cast<uint8_t*>(&sa.sin_addr.s_addr));
      msg.msg_name = &sa;
      msg.msg_namelen = sizeof(sa);
      num_bytes_sent = sendmsg(fd_, &msg, 0);
      break;
    }

    case UdpSocket::Version::kV6: {
      struct sockaddr_in6 sa = {};
      sa.sin6_family = AF_INET6;
      sa.sin6_flowinfo = 0;
      sa.sin6_scope_id = 0;
      sa.sin6_port = htons(dest.port);
      dest.address.CopyToV6(reinterpret_cast<uint8_t*>(&sa.sin6_addr.s6_addr));
      msg.msg_name = &sa;
      msg.msg_namelen = sizeof(sa);
      num_bytes_sent = sendmsg(fd_, &msg, 0);
      break;
    }
  }

  if (num_bytes_sent == -1) {
    return ChooseError(errno, Error::Code::kSocketSendFailure);
  }
  // Sanity-check: UDP datagram sendmsg() is all or nothing.
  OSP_DCHECK_EQ(static_cast<size_t>(num_bytes_sent), length);
  return Error::Code::kNone;
}

Error UdpSocketPosix::SetDscp(UdpSocket::DscpMode state) {
  constexpr auto kSettingLevel = IPPROTO_IP;
  uint8_t code_array[1] = {static_cast<uint8_t>(state)};
  auto code =
      setsockopt(fd_, kSettingLevel, IP_TOS, code_array, sizeof(uint8_t));

  if (code == EBADF || code == ENOTSOCK || code == EFAULT) {
    OSP_VLOG << "BAD SOCKET PROVIDED. CODE: " << code;
    return Error::Code::kSocketOptionSettingFailure;
  } else if (code == EINVAL) {
    OSP_VLOG << "INVALID DSCP INFO PROVIDED";
    return Error::Code::kSocketOptionSettingFailure;
  } else if (code == ENOPROTOOPT) {
    OSP_VLOG << "INVALID DSCP SETTING LEVEL PROVIDED: " << kSettingLevel;
    return Error::Code::kSocketOptionSettingFailure;
  }

  return Error::Code::kNone;
}

}  // namespace platform
}  // namespace openscreen
