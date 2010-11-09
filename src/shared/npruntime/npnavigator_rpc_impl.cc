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

void NPNavigatorRpcServer::NP_SetUpcallServices(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    char* service_string) {
  const char* sd_string = (const char*) service_string;
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  nacl::DebugPrintf("SetUpcallServices: %s\n", sd_string);
  NaClSrpcService* service = reinterpret_cast<NaClSrpcService*>(
      calloc(1, sizeof(*service)));
  if (NULL == service)
    return;
  if (!NaClSrpcServiceStringCtor(service, sd_string)) {
    free(service);
    return;
  }
  rpc->channel->client = service;
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPNavigatorRpcServer::NP_Initialize(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t pid,
    NaClSrpcImcDescType upcall_channel_desc,
    int32_t* nacl_pid) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  // There is only one NPNavigator per call to Initialize, and it is
  // remembered on the SRPC channel that called it.
  // Make sure it is only set once.
  if (NULL != rpc->channel->server_instance_data) {
    // Instance data already set.
    nacl::DebugPrintf("  Error: instance data already set\n");
    return;
  }
  NPNavigator* navigator =
      new(std::nothrow) NPNavigator(rpc->channel, pid);
  if (NULL == navigator || !navigator->is_valid()) {
    // Out of memory.
    nacl::DebugPrintf("  Error: couldn't create navigator\n");
    return;
  }
  rpc->channel->server_instance_data = static_cast<void*>(navigator);

  *nacl_pid = GETPID();
  rpc->result = navigator->Initialize(rpc->channel, upcall_channel_desc);
}

void NPNavigatorRpcServer::NPP_New(NaClSrpcRpc* rpc,
                                   NaClSrpcClosure* done,
                                   char* mimetype,
                                   int32_t wire_npp,
                                   int32_t arg_count,
                                   nacl_abi_size_t argn_bytes,
                                   char* argn_in,
                                   nacl_abi_size_t argv_bytes,
                                   char* argv_in,
                                   int32_t* nperr) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPNavigator* nav = NPNavigator::GetNavigator();

  NPP npp = TranslateOrSetLocalNPP(wire_npp, nav);
  if (NULL == npp) {
    *nperr = NPERR_OUT_OF_MEMORY_ERROR;
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }

  rpc->result = nav->New(mimetype,
                         npp,
                         static_cast<uint32_t>(arg_count),
                         argn_bytes,
                         argn_in,
                         argv_bytes,
                         argv_in,
                         nperr);
}

void NPNavigatorRpcServer::NPP_SetWindow(NaClSrpcRpc* rpc,
                                         NaClSrpcClosure* done,
                                         int32_t wire_npp,
                                         int32_t height,
                                         int32_t width,
                                         int32_t* nperr) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPNavigator* nav = NPNavigator::GetNavigator();

  NPP npp = WireFormatToNPP(wire_npp);
  if (NULL == npp) {
    nacl::DebugPrintf("SetWindow called before nacl NPP was set.\n");
    *nperr = NPERR_GENERIC_ERROR;
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }

  *nperr = nav->SetWindow(npp, height, width);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPNavigatorRpcServer::NPP_Destroy(NaClSrpcRpc* rpc,
                                                NaClSrpcClosure* done,
                                                int32_t wire_npp,
                                                int32_t* nperr) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPNavigator* nav = NPNavigator::GetNavigator();

  *nperr = nav->Destroy(WireFormatToNPP(wire_npp));
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPNavigatorRpcServer::GetScriptableInstance(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t wire_npp,
    nacl_abi_size_t* capability_length,
    char* capability_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPP npp = WireFormatToNPP(wire_npp);
  NPNavigator* nav = NPNavigator::GetNavigator();
  NPCapability* capability = nav->GetScriptableInstance(npp);
  if (!NPCapabilityToWireFormat(capability,
                                capability_bytes,
                                capability_length)) {
    return;
  }
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPNavigatorRpcServer::NPP_HandleEvent(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t wire_npp,
    nacl_abi_size_t npevent_bytes,
    char* npevent,
    int32_t* return_int16) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPNavigator* nav = NPNavigator::GetNavigator();

  rpc->result = nav->HandleEvent(WireFormatToNPP(wire_npp),
                                 npevent_bytes,
                                 npevent,
                                 return_int16);
}

void NPNavigatorRpcServer::NPP_StreamAsFile(NaClSrpcRpc* rpc,
                                            NaClSrpcClosure* done,
                                            int32_t wire_npp,
                                            NaClSrpcImcDescType file,
                                            char* url,
                                            int32_t size) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  nacl::DebugPrintf("NPP_StreamAsFile\n");

  NPNavigator* nav = NPNavigator::GetNavigator();
  nav->StreamAsFile(WireFormatToNPP(wire_npp),
                    file,
                    url,
                    static_cast<uint32_t>(size));
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPNavigatorRpcServer::NPP_URLNotify(NaClSrpcRpc* rpc,
                                         NaClSrpcClosure* done,
                                         int32_t wire_npp,
                                         char* url,
                                         int32_t reason,
                                         int32_t notify_data) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  nacl::DebugPrintf("NPP_URLNotify\n");

  NPP npp = WireFormatToNPP(wire_npp);
  NPNavigator* nav = NPNavigator::GetNavigator();
  nav->URLNotify(npp, url, reason, reinterpret_cast<void*>(notify_data));
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPNavigatorRpcServer::DoAsyncCall(NaClSrpcRpc* rpc,
                                       NaClSrpcClosure* done,
                                       int32_t number) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPNavigator* nav = NPNavigator::GetNavigator();

  nav->DoAsyncCall(number);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPNavigatorRpcServer::AudioCallback(NaClSrpcRpc* rpc,
                                         NaClSrpcClosure* done,
                                         int32_t number,
                                         int shm_desc,
                                         int32_t shm_size,
                                         int sync_desc) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPNavigator* nav = NPNavigator::GetNavigator();

  nav->AudioCallback(number, shm_desc, shm_size, sync_desc);
  rpc->result = NACL_SRPC_RESULT_OK;
}
