// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/flash_net_connector.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Flash_NetConnector>() {
  return PPB_FLASH_NETCONNECTOR_INTERFACE;
}

}  // namespace

namespace flash {

NetConnector::NetConnector(const Instance& instance) {
  if (has_interface<PPB_Flash_NetConnector>()) {
    PassRefFromConstructor(get_interface<PPB_Flash_NetConnector>()->Create(
        instance.pp_instance()));
  }
}

int32_t NetConnector::ConnectTcp(const char* host,
                                 uint16_t port,
                                 PP_FileHandle* socket_out,
                                 PP_NetAddress_Private* local_addr_out,
                                 PP_NetAddress_Private* remote_addr_out,
                                 const CompletionCallback& cc) {
  if (!has_interface<PPB_Flash_NetConnector>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_Flash_NetConnector>()->ConnectTcp(
      pp_resource(),
      host, port,
      socket_out, local_addr_out, remote_addr_out,
      cc.pp_completion_callback());
}

int32_t NetConnector::ConnectTcpAddress(const PP_NetAddress_Private* addr,
                                        PP_FileHandle* socket_out,
                                        PP_NetAddress_Private* local_addr_out,
                                        PP_NetAddress_Private* remote_addr_out,
                                        const CompletionCallback& cc) {
  if (!has_interface<PPB_Flash_NetConnector>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_Flash_NetConnector>()->ConnectTcpAddress(
      pp_resource(),
      addr,
      socket_out, local_addr_out, remote_addr_out,
      cc.pp_completion_callback());
}

}  // namespace flash
}  // namespace pp
