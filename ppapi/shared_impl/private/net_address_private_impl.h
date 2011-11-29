// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PRIVATE_NET_ADDRESS_PRIVATE_IMPL_H_
#define PPAPI_SHARED_IMPL_PRIVATE_NET_ADDRESS_PRIVATE_IMPL_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

struct PP_NetAddress_Private;
struct sockaddr;

namespace net {
class AddressList;
class IPEndPoint;
}

namespace ppapi {

class PPAPI_SHARED_EXPORT NetAddressPrivateImpl {
 public:
  static bool ValidateNetAddress(const PP_NetAddress_Private& addr);

  static bool SockaddrToNetAddress(const sockaddr* sa,
                                   uint32_t sa_length,
                                   PP_NetAddress_Private* net_addr);

  static bool IPEndPointToNetAddress(const net::IPEndPoint& ip,
                                     PP_NetAddress_Private* net_addr);

  // Converts the first address to a PP_NetAddress_Private.
  static bool AddressListToNetAddress(const net::AddressList& address_list,
                                      PP_NetAddress_Private* net_addr);

  static bool NetAddressToIPEndPoint(const PP_NetAddress_Private& net_addr,
                                     net::IPEndPoint* ip_end_point);

  static bool NetAddressToAddressList(const PP_NetAddress_Private& net_addr,
                                      net::AddressList* address_list);

  static const PP_NetAddress_Private kInvalidNetAddress;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(NetAddressPrivateImpl);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PRIVATE_NET_ADDRESS_PRIVATE_IMPL_H_
