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
#include "native_client/src/shared/npruntime/pointer_translations.h"
#include "native_client/src/shared/npruntime/structure_translations.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

using nacl::NPCapability;
using nacl::NPNavigator;
using nacl::NPCapabilityToWireFormat;
using nacl::WireFormatToNPP;

namespace {

NPP TranslateOrSetLocalNPP(int32_t wire_npp, nacl::NPNavigator* nav) {
  // Look up the NaCl NPP copy corresponding to the wire_npp and set the local
  // copy if needed.
  NPP npp = WireFormatToNPP(wire_npp);
  if (NULL == npp) {
    // Allocate a new NPP for the NaCl module.
    npp = new(std::nothrow) NPP_t;
    if (NULL == npp) {
      nacl::DebugPrintf("No memory to allocate NPP.\n");
      return NULL;
    }
    // Attach the NPNavigator to the NPP.
    npp->ndata = static_cast<void*>(nav);
    // Set it to be the local copy used in translations.
    nacl::SetLocalNPP(wire_npp, npp);
  }
  return npp;
}

}  // namespace

//
// The following methods are the SRPC dispatchers for npnavigator.
//

NaClSrpcError NPNavigatorRpcServer::NP_SetUpcallServices(
    NaClSrpcChannel *channel,
    char* service_string) {
  const char* sd_string = (const char*) service_string;
  nacl::DebugPrintf("SetUpcallServices: %s\n", sd_string);
  NaClSrpcService* service = reinterpret_cast<NaClSrpcService*>(
      calloc(1, sizeof(*service)));
  if (NULL == service)
    return NACL_SRPC_RESULT_APP_ERROR;
  if (!NaClSrpcServiceStringCtor(service, sd_string)) {
    free(service);
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  channel->client = service;
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPNavigatorRpcServer::NP_Initialize(
    NaClSrpcChannel* channel,
    int32_t pid,
    NaClSrpcImcDescType upcall_channel_desc,
    int32_t* nacl_pid) {
  // There is only one NPNavigator per call to Initialize, and it is
  // remembered on the SRPC channel that called it.
  // Make sure it is only set once.
  if (NULL != channel->server_instance_data) {
    // Instance data already set.
    nacl::DebugPrintf("  Error: instance data already set\n");
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  NPNavigator* navigator =
      new(std::nothrow) NPNavigator(channel, pid);
  if (NULL == navigator) {
    // Out of memory.
    nacl::DebugPrintf("  Error: couldn't create navigator\n");
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  channel->server_instance_data = static_cast<void*>(navigator);

  *nacl_pid = GETPID();
  return navigator->Initialize(channel, upcall_channel_desc);
}

NaClSrpcError NPNavigatorRpcServer::NPP_New(NaClSrpcChannel* channel,
                                            char* mimetype,
                                            int32_t wire_npp,
                                            int32_t arg_count,
                                            nacl_abi_size_t argn_bytes,
                                            char* argn_in,
                                            nacl_abi_size_t argv_bytes,
                                            char* argv_in,
                                            int32_t* nperr) {
  NPNavigator* nav = NPNavigator::GetNavigator();

  NPP npp = TranslateOrSetLocalNPP(wire_npp, nav);
  if (NULL == npp) {
    *nperr = NPERR_OUT_OF_MEMORY_ERROR;
    return NACL_SRPC_RESULT_OK;
  }

  return nav->New(mimetype,
                  npp,
                  static_cast<uint32_t>(arg_count),
                  argn_bytes,
                  argn_in,
                  argv_bytes,
                  argv_in,
                  nperr);
}

NaClSrpcError NPNavigatorRpcServer::NPP_SetWindow(NaClSrpcChannel* channel,
                                                  int32_t wire_npp,
                                                  int32_t height,
                                                  int32_t width,
                                                  int32_t* nperr) {
  NPNavigator* nav = NPNavigator::GetNavigator();

  NPP npp = WireFormatToNPP(wire_npp);
  if (NULL == npp) {
    nacl::DebugPrintf("SetWindow called before nacl NPP was set.\n");
    *nperr = NPERR_GENERIC_ERROR;
    return NACL_SRPC_RESULT_OK;
  }

  *nperr = nav->SetWindow(npp, height, width);

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPNavigatorRpcServer::NPP_Destroy(NaClSrpcChannel* channel,
                                                int32_t wire_npp,
                                                int32_t* nperr) {
  NPNavigator* nav = NPNavigator::GetNavigator();

  *nperr = nav->Destroy(WireFormatToNPP(wire_npp));

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPNavigatorRpcServer::GetScriptableInstance(
    NaClSrpcChannel* channel,
    int32_t wire_npp,
    nacl_abi_size_t* capability_length,
    char* capability_bytes) {
  NPP npp = WireFormatToNPP(wire_npp);
  NPNavigator* nav = NPNavigator::GetNavigator();
  NPCapability* capability = nav->GetScriptableInstance(npp);
  if (!NPCapabilityToWireFormat(capability,
                                capability_bytes,
                                capability_length)) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPNavigatorRpcServer::NPP_HandleEvent(
    NaClSrpcChannel* channel,
    int32_t wire_npp,
    nacl_abi_size_t npevent_bytes,
    char* npevent,
    int32_t* return_int16) {
  NPNavigator* nav = NPNavigator::GetNavigator();

  return nav->HandleEvent(WireFormatToNPP(wire_npp),
                          npevent_bytes,
                          npevent,
                          return_int16);
}

NaClSrpcError NPNavigatorRpcServer::NPP_StreamAsFile(NaClSrpcChannel* channel,
                                                     int32_t wire_npp,
                                                     NaClSrpcImcDescType file,
                                                     char* url,
                                                     int32_t size) {
  nacl::DebugPrintf("NPP_URLNotify\n");

  NPNavigator* nav = NPNavigator::GetNavigator();
  nav->StreamAsFile(WireFormatToNPP(wire_npp),
                    file,
                    url,
                    static_cast<uint32_t>(size));

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPNavigatorRpcServer::NPP_URLNotify(NaClSrpcChannel* channel,
                                                  int32_t wire_npp,
                                                  NaClSrpcImcDescType str,
                                                  int32_t reason) {
  // TODO(sehr): what happens when we fail to get a URL?
  nacl::DebugPrintf("NPP_URLNotify\n");

  NPP npp = WireFormatToNPP(wire_npp);
  NPNavigator* nav = NPNavigator::GetNavigator();
  nav->URLNotify(npp, str, reason);

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPNavigatorRpcServer::DoAsyncCall(NaClSrpcChannel* channel,
                                                int32_t number) {
  NPNavigator* nav = NPNavigator::GetNavigator();

  nav->DoAsyncCall(number);

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPNavigatorRpcServer::AudioCallback(NaClSrpcChannel* channel,
                                                  int32_t number,
                                                  int shm_desc,
                                                  int32_t shm_size,
                                                  int sync_desc) {
  NPNavigator* nav = NPNavigator::GetNavigator();

  nav->AudioCallback(number, shm_desc, shm_size, sync_desc);

  return NACL_SRPC_RESULT_OK;
}
