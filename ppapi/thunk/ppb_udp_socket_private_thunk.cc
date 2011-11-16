// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "ppapi/thunk/common.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_udp_socket_private_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateUDPSocketPrivate(instance);
}

PP_Bool IsUDPSocket(PP_Resource resource) {
  EnterResource<PPB_UDPSocket_Private_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Bind(PP_Resource udp_socket,
             const PP_NetAddress_Private *addr,
             PP_CompletionCallback callback) {
  EnterResource<PPB_UDPSocket_Private_API> enter(udp_socket, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Bind(addr, callback);
  return MayForceCallback(callback, result);
}

int32_t RecvFrom(PP_Resource udp_socket,
                 char* buffer,
                 int32_t num_bytes,
                 PP_CompletionCallback callback) {
  EnterResource<PPB_UDPSocket_Private_API> enter(udp_socket, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->RecvFrom(buffer,
                                            num_bytes,
                                            callback);
  return MayForceCallback(callback, result);
}

PP_Bool GetRecvFromAddress(PP_Resource udp_socket,
                           PP_NetAddress_Private* addr) {
  EnterResource<PPB_UDPSocket_Private_API> enter(udp_socket, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetRecvFromAddress(addr);
}

int32_t SendTo(PP_Resource udp_socket,
               const char* buffer,
               int32_t num_bytes,
               const PP_NetAddress_Private* addr,
               PP_CompletionCallback callback) {
  EnterResource<PPB_UDPSocket_Private_API> enter(udp_socket, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->SendTo(buffer, num_bytes, addr, callback);
  return MayForceCallback(callback, result);
}

void Close(PP_Resource udp_socket) {
  EnterResource<PPB_UDPSocket_Private_API> enter(udp_socket, true);
  if (enter.succeeded())
    enter.object()->Close();
}

const PPB_UDPSocket_Private g_ppb_udp_socket_thunk = {
  &Create,
  &IsUDPSocket,
  &Bind,
  &RecvFrom,
  &GetRecvFromAddress,
  &SendTo,
  &Close
};

}  // namespace

const PPB_UDPSocket_Private* GetPPB_UDPSocket_Private_Thunk() {
  return &g_ppb_udp_socket_thunk;
}

}  // namespace thunk
}  // namespace ppapi

