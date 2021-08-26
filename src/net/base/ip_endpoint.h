// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_IP_ENDPOINT_H_
#define NET_BASE_IP_ENDPOINT_H_

#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "net/base/address_family.h"
#include "net/base/ip_address.h"
#include "net/base/net_export.h"

#if defined(OS_WIN)
// Replicate these from Windows headers to avoid pulling net/sys_addrinfo.h.
// Doing that transitively brings in windows.h. Including windows.h pollutes the
// global namespace with thousands of macro definitions. This file is
// transitively included in enough files that including windows.h potentially
// impacts build performance.
struct sockaddr;
typedef int socklen_t;
#else
#include "net/base/sys_addrinfo.h"
#endif

namespace net {

// An IPEndPoint represents the address of a transport endpoint:
//  * IP address (either v4 or v6)
//  * Port
class NET_EXPORT IPEndPoint {
 public:
  IPEndPoint();
  ~IPEndPoint();
  IPEndPoint(const IPAddress& address, uint16_t port);
  IPEndPoint(const IPEndPoint& endpoint);

  const IPAddress& address() const { return address_; }

  // Returns the IPv4/IPv6 port if it has been set by the constructor or
  // `FromSockAddr`. This function will crash if the IPEndPoint is for a
  // Bluetooth socket.
  uint16_t port() const;

  // Returns AddressFamily of the address. Returns ADDRESS_FAMILY_UNSPECIFIED if
  // this is the IPEndPoint for a Bluetooth socket.
  AddressFamily GetFamily() const;

  // Returns the sockaddr family of the address, AF_INET or AF_INET6. Returns
  // AF_BTH if this is the IPEndPoint for a Bluetooth socket.
  int GetSockAddrFamily() const;

  // Convert to a provided sockaddr struct. This function will crash if the
  // IPEndPoint is for a Bluetooth socket.
  // |address| is the sockaddr to copy into.  Should be at least
  //    sizeof(struct sockaddr_storage) bytes.
  // |address_length| is an input/output parameter.  On input, it is the
  //    size of data in |address| available.  On output, it is the size of
  //    the address that was copied into |address|.
  // Returns true on success, false on failure.
  bool ToSockAddr(struct sockaddr* address,
                  socklen_t* address_length) const WARN_UNUSED_RESULT;

  // Convert from a sockaddr struct.
  // |address| is the address.
  // |address_length| is the length of |address|.
  // Returns true on success, false on failure.
  bool FromSockAddr(const struct sockaddr* address,
                    socklen_t address_length) WARN_UNUSED_RESULT;

  // Returns value as a string (e.g. "127.0.0.1:80"). Returns the empty string
  // when |address_| is invalid (the port will be ignored). This function will
  // crash if the IPEndPoint is for a Bluetooth socket.
  std::string ToString() const;

  // As above, but without port. Returns the empty string when address_ is
  // invalid. The function will crash if the IPEndPoint is for a Bluetooth
  // socket.
  std::string ToStringWithoutPort() const;

  bool operator<(const IPEndPoint& that) const;
  bool operator==(const IPEndPoint& that) const;
  bool operator!=(const IPEndPoint& that) const;

 private:
  IPAddress address_;
  uint16_t port_ = 0;
};

}  // namespace net

#endif  // NET_BASE_IP_ENDPOINT_H_
