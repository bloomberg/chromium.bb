// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_tcp_socket_private.h"
#include "ppapi/thunk/common.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_tcp_socket_private_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateTCPSocketPrivate(instance);
}

PP_Bool IsTCPSocket(PP_Resource resource) {
  EnterResource<PPB_TCPSocket_Private_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Connect(PP_Resource tcp_socket,
                const char* host,
                uint16_t port,
                PP_CompletionCallback callback) {
  EnterResource<PPB_TCPSocket_Private_API> enter(tcp_socket, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Connect(host, port, callback);
  return MayForceCallback(callback, result);
}

int32_t ConnectWithNetAddress(PP_Resource tcp_socket,
                              const PP_NetAddress_Private* addr,
                              PP_CompletionCallback callback) {
  EnterResource<PPB_TCPSocket_Private_API> enter(tcp_socket, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->ConnectWithNetAddress(addr, callback);
  return MayForceCallback(callback, result);
}

PP_Bool GetLocalAddress(PP_Resource tcp_socket,
                        PP_NetAddress_Private* local_addr) {
  EnterResource<PPB_TCPSocket_Private_API> enter(tcp_socket, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetLocalAddress(local_addr);
}

PP_Bool GetRemoteAddress(PP_Resource tcp_socket,
                         PP_NetAddress_Private* remote_addr) {
  EnterResource<PPB_TCPSocket_Private_API> enter(tcp_socket, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetRemoteAddress(remote_addr);
}

int32_t SSLHandshake(PP_Resource tcp_socket,
                     const char* server_name,
                     uint16_t server_port,
                     PP_CompletionCallback callback) {
  EnterResource<PPB_TCPSocket_Private_API> enter(tcp_socket, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->SSLHandshake(server_name, server_port,
                                                callback);
  return MayForceCallback(callback, result);
}

int32_t Read(PP_Resource tcp_socket,
             char* buffer,
             int32_t bytes_to_read,
             PP_CompletionCallback callback) {
  EnterResource<PPB_TCPSocket_Private_API> enter(tcp_socket, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Read(buffer, bytes_to_read, callback);
  return MayForceCallback(callback, result);
}

int32_t Write(PP_Resource tcp_socket,
              const char* buffer,
              int32_t bytes_to_write,
              PP_CompletionCallback callback) {
  EnterResource<PPB_TCPSocket_Private_API> enter(tcp_socket, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Write(buffer, bytes_to_write, callback);
  return MayForceCallback(callback, result);
}

void Disconnect(PP_Resource tcp_socket) {
  EnterResource<PPB_TCPSocket_Private_API> enter(tcp_socket, true);
  if (enter.succeeded())
    enter.object()->Disconnect();
}

const PPB_TCPSocket_Private g_ppb_tcp_socket_thunk = {
  &Create,
  &IsTCPSocket,
  &Connect,
  &ConnectWithNetAddress,
  &GetLocalAddress,
  &GetRemoteAddress,
  &SSLHandshake,
  &Read,
  &Write,
  &Disconnect
};

}  // namespace

const PPB_TCPSocket_Private* GetPPB_TCPSocket_Private_Thunk() {
  return &g_ppb_tcp_socket_thunk;
}

}  // namespace thunk
}  // namespace ppapi

