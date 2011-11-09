// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(viettrungluu): See the comment in corresponding .h file.

#include "ppapi/cpp/private/flash_net_address.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"
#include "ppapi/c/private/ppb_flash_net_address.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Flash_NetAddress>() {
  return PPB_FLASH_NETADDRESS_INTERFACE;
}

}  // namespace

namespace flash {

// static
bool NetAddress::AreEqual(const PP_Flash_NetAddress& addr1,
                          const PP_Flash_NetAddress& addr2) {
  if (!has_interface<PPB_Flash_NetAddress>())
    return false;
  return !!get_interface<PPB_Flash_NetAddress>()->AreEqual(&addr1, &addr2);
}

// static
bool NetAddress::AreHostsEqual(const PP_Flash_NetAddress& addr1,
                               const PP_Flash_NetAddress& addr2) {
  if (!has_interface<PPB_Flash_NetAddress>())
    return false;
  return !!get_interface<PPB_Flash_NetAddress>()->AreHostsEqual(&addr1, &addr2);
}

// static
std::string NetAddress::Describe(const PP_Flash_NetAddress& addr,
                                 bool include_port) {
  if (!has_interface<PPB_Flash_NetAddress>())
    return std::string();

  Module* module = Module::Get();
  if (!module)
    return std::string();

  Var result(Var::PassRef(),
             get_interface<PPB_Flash_NetAddress>()->Describe(
                 module->pp_module(),
                 &addr,
                 PP_FromBool(include_port)));
  return result.is_string() ? result.AsString() : std::string();
}

// static
bool NetAddress::ReplacePort(const PP_Flash_NetAddress& addr_in,
                             uint16_t port,
                             PP_Flash_NetAddress* addr_out) {
  if (!has_interface<PPB_Flash_NetAddress>())
    return false;
  return !!get_interface<PPB_Flash_NetAddress>()->ReplacePort(&addr_in,
                                                              port,
                                                              addr_out);
}

// static
void NetAddress::GetAnyAddress(bool is_ipv6, PP_Flash_NetAddress* addr) {
  if (!has_interface<PPB_Flash_NetAddress>())
    return;
  get_interface<PPB_Flash_NetAddress>()->GetAnyAddress(PP_FromBool(is_ipv6),
                                                       addr);
}

}  // namespace flash
}  // namespace pp
