// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/private/ppb_flash_net_connector.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/common.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_flash_net_connector_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateFlashNetConnector(instance);
}

PP_Bool IsFlashNetConnector(PP_Resource resource) {
  EnterResource<PPB_Flash_NetConnector_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t ConnectTcp(PP_Resource resource,
                   const char* host,
                   uint16_t port,
                   PP_FileHandle* socket_out,
                   PP_NetAddress_Private* local_addr_out,
                   PP_NetAddress_Private* remote_addr_out,
                   PP_CompletionCallback callback) {
  EnterResource<PPB_Flash_NetConnector_API> enter(resource, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result =
      enter.object()->ConnectTcp(host, port, socket_out, local_addr_out,
                                 remote_addr_out, callback);
  return MayForceCallback(callback, result);
}

int32_t ConnectTcpAddress(PP_Resource resource,
                          const PP_NetAddress_Private* addr,
                          PP_FileHandle* socket_out,
                          PP_NetAddress_Private* local_addr_out,
                          PP_NetAddress_Private* remote_addr_out,
                          PP_CompletionCallback callback) {
  EnterResource<PPB_Flash_NetConnector_API> enter(resource, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result =
      enter.object()->ConnectTcpAddress(addr, socket_out, local_addr_out,
                                           remote_addr_out, callback);
  return MayForceCallback(callback, result);
}

const PPB_Flash_NetConnector g_ppb_flash_net_connector_thunk = {
  &Create,
  &IsFlashNetConnector,
  &ConnectTcp,
  &ConnectTcpAddress
};

}  // namespace

const PPB_Flash_NetConnector* GetPPB_Flash_NetConnector_Thunk() {
  return &g_ppb_flash_net_connector_thunk;
}

}  // namespace thunk
}  // namespace ppapi
