// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(viettrungluu): This (and the .cc file) contain C++ wrappers for things
// in ppapi/c/private/ppb_flash_net_address.h. This is currently not used in
// (or even compiled with) Chromium.

#ifndef PPAPI_CPP_PRIVATE_FLASH_NET_ADDRESS_H_
#define PPAPI_CPP_PRIVATE_FLASH_NET_ADDRESS_H_

#include <string>

#include "ppapi/c/pp_stdint.h"

struct PP_Flash_NetAddress;

namespace pp {
namespace flash {

class NetAddress {
 public:
  static bool AreEqual(const PP_Flash_NetAddress& addr1,
                       const PP_Flash_NetAddress& addr2);
  static bool AreHostsEqual(const PP_Flash_NetAddress& addr1,
                            const PP_Flash_NetAddress& addr2);
  static std::string Describe(const PP_Flash_NetAddress& addr,
                              bool include_port);
  static bool ReplacePort(const PP_Flash_NetAddress& addr_in,
                          uint16_t port,
                          PP_Flash_NetAddress* addr_out);
  static void GetAnyAddress(bool is_ipv6, PP_Flash_NetAddress* addr);
};

}  // namespace flash
}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_FLASH_NET_ADDRESS_H_
