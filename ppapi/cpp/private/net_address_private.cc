// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/net_address_private.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"
#include "ppapi/c/private/ppb_net_address_private.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_NetAddress_Private>() {
  return PPB_NETADDRESS_PRIVATE_INTERFACE;
}

}  // namespace

// static
bool NetAddressPrivate::IsAvailable() {
  return has_interface<PPB_NetAddress_Private>();
}

// static
bool NetAddressPrivate::AreEqual(const PP_NetAddress_Private& addr1,
                                 const PP_NetAddress_Private& addr2) {
  if (!has_interface<PPB_NetAddress_Private>())
    return false;
  return !!get_interface<PPB_NetAddress_Private>()->AreEqual(&addr1, &addr2);
}

// static
bool NetAddressPrivate::AreHostsEqual(const PP_NetAddress_Private& addr1,
                                      const PP_NetAddress_Private& addr2) {
  if (!has_interface<PPB_NetAddress_Private>())
    return false;
  return !!get_interface<PPB_NetAddress_Private>()->AreHostsEqual(&addr1,
                                                                  &addr2);
}

// static
std::string NetAddressPrivate::Describe(const PP_NetAddress_Private& addr,
                                        bool include_port) {
  if (!has_interface<PPB_NetAddress_Private>())
    return std::string();

  Module* module = Module::Get();
  if (!module)
    return std::string();

  Var result(Var::PassRef(),
             get_interface<PPB_NetAddress_Private>()->Describe(
                 module->pp_module(),
                 &addr,
                 PP_FromBool(include_port)));
  return result.is_string() ? result.AsString() : std::string();
}

// static
bool NetAddressPrivate::ReplacePort(const PP_NetAddress_Private& addr_in,
                                    uint16_t port,
                                    PP_NetAddress_Private* addr_out) {
  if (!has_interface<PPB_NetAddress_Private>())
    return false;
  return !!get_interface<PPB_NetAddress_Private>()->ReplacePort(&addr_in,
                                                                port,
                                                                addr_out);
}

// static
void NetAddressPrivate::GetAnyAddress(bool is_ipv6,
                                      PP_NetAddress_Private* addr) {
  if (!has_interface<PPB_NetAddress_Private>())
    return;
  get_interface<PPB_NetAddress_Private>()->GetAnyAddress(PP_FromBool(is_ipv6),
                                                         addr);
}

}  // namespace pp
