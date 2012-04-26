// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/udp_socket_private.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_UDPSocket_Private_0_3>() {
  return PPB_UDPSOCKET_PRIVATE_INTERFACE_0_3;
}

}  // namespace

UDPSocketPrivate::UDPSocketPrivate(const InstanceHandle& instance) {
  if (has_interface<PPB_UDPSocket_Private_0_3>()) {
    PassRefFromConstructor(get_interface<PPB_UDPSocket_Private_0_3>()->Create(
                               instance.pp_instance()));
  }
}

// static
bool UDPSocketPrivate::IsAvailable() {
  return has_interface<PPB_UDPSocket_Private_0_3>();
}

int32_t UDPSocketPrivate::Bind(const PP_NetAddress_Private* addr,
                               const CompletionCallback& callback) {
  if (!has_interface<PPB_UDPSocket_Private_0_3>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_UDPSocket_Private_0_3>()->Bind(
      pp_resource(), addr, callback.pp_completion_callback());
}

bool UDPSocketPrivate::GetBoundAddress(PP_NetAddress_Private* addr) {
  if (!has_interface<PPB_UDPSocket_Private_0_3>())
    return false;

  PP_Bool result = get_interface<PPB_UDPSocket_Private_0_3>()->GetBoundAddress(
      pp_resource(), addr);
  return PP_ToBool(result);
}

int32_t UDPSocketPrivate::RecvFrom(char* buffer,
                                   int32_t num_bytes,
                                   const CompletionCallback& callback) {
  if (!has_interface<PPB_UDPSocket_Private_0_3>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_UDPSocket_Private_0_3>()->RecvFrom(
      pp_resource(), buffer, num_bytes, callback.pp_completion_callback());
}

bool UDPSocketPrivate::GetRecvFromAddress(PP_NetAddress_Private* addr) {
  if (!has_interface<PPB_UDPSocket_Private_0_3>())
    return false;

  PP_Bool result =
      get_interface<PPB_UDPSocket_Private_0_3>()->GetRecvFromAddress(
          pp_resource(), addr);
  return PP_ToBool(result);
}

int32_t UDPSocketPrivate::SendTo(const char* buffer,
                                 int32_t num_bytes,
                                 const PP_NetAddress_Private* addr,
                                 const CompletionCallback& callback) {
  if (!has_interface<PPB_UDPSocket_Private_0_3>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_UDPSocket_Private_0_3>()->SendTo(
      pp_resource(), buffer, num_bytes, addr,
      callback.pp_completion_callback());
}

void UDPSocketPrivate::Close() {
  if (!has_interface<PPB_UDPSocket_Private_0_3>())
    return;
  return get_interface<PPB_UDPSocket_Private_0_3>()->Close(pp_resource());
}
}  // namespace pp
