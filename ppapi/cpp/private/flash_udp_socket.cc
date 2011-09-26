// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/flash_udp_socket.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Flash_UDPSocket>() {
  return PPB_FLASH_UDPSOCKET_INTERFACE;
}

}  // namespace

namespace flash {

UDPSocket::UDPSocket(Instance* instance) {
  if (has_interface<PPB_Flash_UDPSocket>() && instance) {
    PassRefFromConstructor(get_interface<PPB_Flash_UDPSocket>()->Create(
                               instance->pp_instance()));
  }
}

int32_t UDPSocket::Bind(const PP_Flash_NetAddress* addr,
                        const CompletionCallback& callback) {
  if (!has_interface<PPB_Flash_UDPSocket>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_Flash_UDPSocket>()->Bind(
      pp_resource(), addr, callback.pp_completion_callback());
}

int32_t UDPSocket::RecvFrom(char* buffer,
                            int32_t num_bytes,
                            const CompletionCallback& callback) {
  if (!has_interface<PPB_Flash_UDPSocket>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_Flash_UDPSocket>()->RecvFrom(
      pp_resource(), buffer, num_bytes, callback.pp_completion_callback());
}

bool UDPSocket::GetRecvFromAddress(PP_Flash_NetAddress* addr) {
  if (!has_interface<PPB_Flash_UDPSocket>())
    return false;

  PP_Bool result = get_interface<PPB_Flash_UDPSocket>()->GetRecvFromAddress(
      pp_resource(), addr);
  return PP_ToBool(result);
}

int32_t UDPSocket::SendTo(const char* buffer,
                          int32_t num_bytes,
                          const struct PP_Flash_NetAddress* addr,
                          const CompletionCallback& callback) {
  if (!has_interface<PPB_Flash_UDPSocket>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_Flash_UDPSocket>()->SendTo(
      pp_resource(), buffer, num_bytes, addr,
      callback.pp_completion_callback());
}

}  // namespace flash
}  // namespace pp

