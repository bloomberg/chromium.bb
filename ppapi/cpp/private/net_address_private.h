// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_NET_ADDRESS_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_NET_ADDRESS_PRIVATE_H_

#include <string>

#include "ppapi/c/pp_stdint.h"

struct PP_NetAddress_Private;

namespace pp {

class NetAddressPrivate {
 public:
  static bool AreEqual(const PP_NetAddress_Private& addr1,
                       const PP_NetAddress_Private& addr2);
  static bool AreHostsEqual(const PP_NetAddress_Private& addr1,
                            const PP_NetAddress_Private& addr2);
  static std::string Describe(const PP_NetAddress_Private& addr,
                              bool include_port);
  static bool ReplacePort(const PP_NetAddress_Private& addr_in,
                          uint16_t port,
                          PP_NetAddress_Private* addr_out);
  static void GetAnyAddress(bool is_ipv6, PP_NetAddress_Private* addr);
};

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_NET_ADDRESS_PRIVATE_H_
