// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface

#include <stdarg.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "gen/native_client/src/shared/npruntime/npmodule_rpc.h"
#include "gen/native_client/src/shared/npruntime/npnavigator_rpc.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npbridge.h"
#include "native_client/src/shared/npruntime/npnavigator.h"
#include "native_client/src/shared/npruntime/npobject_proxy.h"
#include "native_client/src/shared/npruntime/npobject_stub.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

using nacl::RpcArg;
using nacl::NPObjectStub;
using nacl::NPBridge;
using nacl::NPNavigator;
using nacl::NPCapability;

//
// The following methods are the SRPC dispatchers for npnavigator.
//

NaClSrpcError NPNavigatorRpcServer::NP_SetUpcallServices(
    NaClSrpcChannel *channel,
    char* service_string) {
  const char* sd_string = (const char*) service_string;
  printf("SetUpcallServices: %s\n", sd_string);
  NaClSrpcService* service = new(std::nothrow) NaClSrpcService;
  if (NULL == service)
    return NACL_SRPC_RESULT_APP_ERROR;
  if (!NaClSrpcServiceStringCtor(service, sd_string)) {
    delete service;
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  channel->client = service;
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPNavigatorRpcServer::NP_Initialize(
    NaClSrpcChannel* channel,
    int32_t pid,
    int32_t npvariant,
    NaClSrpcImcDescType upcall_channel_desc) {
  // There is only one NPNavigator per call to Initialize, and it is
  // remembered on the SRPC channel that called it.
  // Make sure it is only set once.
  if (NULL != channel->server_instance_data) {
    // Instance data already set.
    nacl::DebugPrintf("  Error: instance data already set\n");
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  NPNavigator* navigator =
      new(std::nothrow) NPNavigator(channel, pid, npvariant);
  if (NULL == navigator) {
    // Out of memory.
    nacl::DebugPrintf("  Error: couldn't create navigator\n");
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  channel->server_instance_data = static_cast<void*>(navigator);

  return navigator->Initialize(channel, upcall_channel_desc);
}

NaClSrpcError NPNavigatorRpcServer::NPP_New(NaClSrpcChannel* channel,
                                            char* mimetype,
                                            int32_t int_npp,
                                            int32_t arg_count,
                                            nacl_abi_size_t argn_bytes,
                                            char* argn_in,
                                            nacl_abi_size_t argv_bytes,
                                            char* argv_in,
                                            int32_t* nperr) {
  NPNavigator* nav = NPNavigator::GetNavigator();

  return nav->New(mimetype,
                  NPNavigator::GetNaClNPP(int_npp, true),
                  static_cast<uint32_t>(arg_count),
                  argn_bytes,
                  argn_in,
                  argv_bytes,
                  argv_in,
                  nperr);
}

NaClSrpcError NPNavigatorRpcServer::NPP_SetWindow(NaClSrpcChannel* channel,
                                                  int32_t int_npp,
                                                  int32_t height,
                                                  int32_t width,
                                                  int32_t* nperr) {
  NPNavigator* nav = NPNavigator::GetNavigator();

  *nperr =
      nav->SetWindow(NPNavigator::GetNaClNPP(int_npp, true),
                     height,
                     width);

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPNavigatorRpcServer::NPP_Destroy(NaClSrpcChannel* channel,
                                                int32_t int_npp,
                                                int32_t* nperr) {
  NPNavigator* nav = NPNavigator::GetNavigator();

  *nperr =
      nav->Destroy(NPNavigator::GetNaClNPP(int_npp, false));

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPNavigatorRpcServer::NPP_GetScriptableInstance(
    NaClSrpcChannel* channel,
    int32_t int_npp,
    nacl_abi_size_t* cap_bytes,
    char* cap) {
  NPP npp = NPNavigator::GetNaClNPP(int_npp, false);
  NPNavigator* nav = NPNavigator::GetNavigator();
  NPCapability* capability = nav->GetScriptableInstance(npp);
  RpcArg ret(npp, cap, *cap_bytes);
  ret.PutCapability(capability);
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPNavigatorRpcServer::NPP_HandleEvent(
    NaClSrpcChannel* channel,
    int32_t int_npp,
    nacl_abi_size_t npevent_bytes,
    char* npevent,
    int32_t* return_int16) {
  NPP npp = NPNavigator::GetNaClNPP(int_npp, false);
  NPNavigator* nav = NPNavigator::GetNavigator();

  return nav->HandleEvent(npp, npevent_bytes, npevent, return_int16);
}

NaClSrpcError NPNavigatorRpcServer::NPP_URLNotify(NaClSrpcChannel* channel,
                                                  int32_t int_npp,
                                                  NaClSrpcImcDescType str,
                                                  int32_t reason) {
  // TODO(sehr): what happens when we fail to get a URL?
  nacl::DebugPrintf("NPP_URLNotify\n");

  NPP npp = NPNavigator::GetNaClNPP(int_npp, false);
  NPNavigator* nav = NPNavigator::GetNavigator();
  nav->URLNotify(npp, str, reason);

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPNavigatorRpcServer::NPP_DoAsyncCall(NaClSrpcChannel* channel,
                                                    int32_t number) {
  NPNavigator* nav = NPNavigator::GetNavigator();

  nav->DoAsyncCall(number);

  return NACL_SRPC_RESULT_OK;
}
