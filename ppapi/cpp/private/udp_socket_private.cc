// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/udp_socket_private.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_UDPSocket_Private>() {
  return PPB_UDPSOCKET_PRIVATE_INTERFACE;
}

}  // namespace

UDPSocketPrivate::UDPSocketPrivate(Instance* instance) {
  if (has_interface<PPB_UDPSocket_Private>() && instance) {
    PassRefFromConstructor(get_interface<PPB_UDPSocket_Private>()->Create(
                               instance->pp_instance()));
  }
}

int32_t UDPSocketPrivate::Bind(const PP_NetAddress_Private* addr,
                               const CompletionCallback& callback) {
  if (!has_interface<PPB_UDPSocket_Private>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_UDPSocket_Private>()->Bind(
      pp_resource(), addr, callback.pp_completion_callback());
}

int32_t UDPSocketPrivate::RecvFrom(char* buffer,
                                   int32_t num_bytes,
                                   const CompletionCallback& callback) {
  if (!has_interface<PPB_UDPSocket_Private>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_UDPSocket_Private>()->RecvFrom(
      pp_resource(), buffer, num_bytes, callback.pp_completion_callback());
}

bool UDPSocketPrivate::GetRecvFromAddress(PP_NetAddress_Private* addr) {
  if (!has_interface<PPB_UDPSocket_Private>())
    return false;

  PP_Bool result = get_interface<PPB_UDPSocket_Private>()->GetRecvFromAddress(
      pp_resource(), addr);
  return PP_ToBool(result);
}

int32_t UDPSocketPrivate::SendTo(const char* buffer,
                                 int32_t num_bytes,
                                 const PP_NetAddress_Private* addr,
                                 const CompletionCallback& callback) {
  if (!has_interface<PPB_UDPSocket_Private>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_UDPSocket_Private>()->SendTo(
      pp_resource(), buffer, num_bytes, addr,
      callback.pp_completion_callback());
}

}  // namespace pp

