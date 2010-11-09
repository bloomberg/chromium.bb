/*
 * Copyright (c) 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <limits>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/checked_cast.h"
#include "native_client/src/shared/npruntime/npmodule.h"

#include "gen/native_client/src/shared/npruntime/npmodule_rpc.h"
#include "gen/native_client/src/shared/npruntime/npnavigator_rpc.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npobject_proxy.h"
#include "native_client/src/shared/npruntime/npobject_stub.h"
#include "native_client/src/shared/npruntime/pointer_translations.h"
#include "native_client/src/shared/npruntime/structure_translations.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/trusted/desc/nacl_desc_invalid.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/origin.h"
#include "native_client/src/trusted/plugin/npapi/browser_impl_npapi.h"
#include "native_client/src/trusted/plugin/npapi/closure.h"
#include "native_client/src/trusted/plugin/utility.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

using nacl::NPIdentifierToWireFormat;
using nacl::NPModule;
using nacl::NPObjectToWireFormat;
using nacl::NPVariantsToWireFormat;
using nacl::WireFormatToNPIdentifier;
using nacl::WireFormatToNPP;
using nacl::WireFormatToNPObject;
using nacl::WireFormatToNPString;

void NPModuleRpcServer::NPN_GetValueBoolean(NaClSrpcRpc* rpc,
                                            NaClSrpcClosure* done,
                                            int32_t wire_npp,
                                            int32_t var,
                                            int32_t* nperr,
                                            int32_t* boolean_result) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPNVariable variable = static_cast<NPNVariable>(var);
  NPP npp = WireFormatToNPP(wire_npp);

  // There are two variables that are booleans.
  if (NPNVisOfflineBool != variable && NPNVprivateModeBool != variable) {
    *nperr = NPERR_INVALID_PARAM;
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }
  int32_t value;
  *nperr = ::NPN_GetValue(npp,
                          variable,
                          reinterpret_cast<void*>(&value));
  *boolean_result = static_cast<NPBool>(value);
  *nperr = NPERR_NO_ERROR;
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPModuleRpcServer::NPN_GetValueObject(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t wire_npp,
    int32_t var,
    int32_t* nperr,
    nacl_abi_size_t* result_length,
    char* result_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPNVariable variable = static_cast<NPNVariable>(var);
  NPP npp = WireFormatToNPP(wire_npp);

  if (NPNVWindowNPObject != variable &&
      NPNVPluginElementNPObject != variable) {
    *nperr = NPERR_INVALID_PARAM;
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }
  // There are two variables that are NPObjects.
  NPObject* object;
  *nperr = ::NPN_GetValue(npp, variable, reinterpret_cast<void*>(&object));
  if (NULL == object) {
    *result_length = 0;
  } else {
    if (!NPObjectToWireFormat(npp, object, result_bytes, result_length)) {
      return;
    }
  }
  *nperr = NPERR_NO_ERROR;
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPModuleRpcServer::NPN_Evaluate(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t wire_npp,
    nacl_abi_size_t obj_length,
    char* obj_bytes,
    nacl_abi_size_t str_length,
    char* str_bytes,
    int32_t* success,
    nacl_abi_size_t* result_length,
    char* result_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPP npp = WireFormatToNPP(wire_npp);

  // Initialize to report error
  *success = 0;
  // Get the object.
  NPObject* object = WireFormatToNPObject(npp, obj_bytes, obj_length);
  // Get the string.
  NPString str;
  if (!WireFormatToNPString(str_bytes, str_length, &str)) {
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }
  NPVariant result;
  *success = ::NPN_Evaluate(npp, object, &str, &result);
  if (!NPVariantsToWireFormat(npp, &result, 1, result_bytes, result_length)) {
    return;
  }
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPModuleRpcServer::NPN_SetStatus(NaClSrpcRpc* rpc,
                                      NaClSrpcClosure* done,
                                      int32_t wire_npp,
                                      char* status) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (NULL != status) {
    ::NPN_Status(WireFormatToNPP(wire_npp), status);
  }
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPModuleRpcServer::NPN_GetURL(NaClSrpcRpc* rpc,
                                   NaClSrpcClosure* done,
                                   int32_t wire_npp,
                                   char* url,
                                   char* target,
                                   int32_t notify_data,
                                   int32_t call_url_notify,
                                   int32_t* nperr) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (NULL == url || NULL == target || NULL == nperr) {
    return;
  }
  if (0 == strlen(url) || 0 != strlen(target)) {
    *nperr = NPERR_GENERIC_ERROR;
  } else {
    NPModule* module = NPModule::GetModule(wire_npp);
    nacl::string url_origin = nacl::UrlToOrigin(url);

    plugin::NpGetUrlClosure* closure = new(std::nothrow)
        plugin::NpGetUrlClosure(WireFormatToNPP(wire_npp),
                                module,
                                url,
                                notify_data,
                                (call_url_notify != 0));
    if (NULL == closure) {
      *nperr = NPERR_GENERIC_ERROR;
    }
    closure->StartDownload();
    *nperr = NPERR_NO_ERROR;
  }

  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPModuleRpcServer::NPN_UserAgent(NaClSrpcRpc* rpc,
                                      NaClSrpcClosure* done,
                                      int32_t wire_npp,
                                      char** strval) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  if (NULL == strval) {
    return;
  }
  *strval = const_cast<char*>(::NPN_UserAgent(WireFormatToNPP(wire_npp)));
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPModuleRpcServer::NPN_GetIntIdentifier(NaClSrpcRpc* rpc,
                                             NaClSrpcClosure* done,
                                             int32_t intval,
                                             int32_t* id) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPIdentifier identifier;

  if (NPModule::IsWebkit()) {
    // Webkit needs to look up integer IDs as strings.
    char index[11];
    SNPRINTF(index, sizeof(index), "%u", static_cast<unsigned>(intval));
    identifier = ::NPN_GetStringIdentifier(index);
  } else {
    identifier = ::NPN_GetIntIdentifier(intval);
  }
  *id = NPIdentifierToWireFormat(identifier);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPModuleRpcServer::NPN_IntFromIdentifier(NaClSrpcRpc* rpc,
                                              NaClSrpcClosure* done,
                                              int32_t id,
                                              int32_t* intval) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *intval = ::NPN_IntFromIdentifier(WireFormatToNPIdentifier(id));
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPModuleRpcServer::NPN_GetStringIdentifier(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    char* strval,
    int32_t* id) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  NPIdentifier identifier = NULL;
  if (plugin::IsValidIdentifierString(strval, NULL)) {
    identifier = ::NPN_GetStringIdentifier(strval);
  }
  *id = NPIdentifierToWireFormat(identifier);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPModuleRpcServer::NPN_IdentifierIsString(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t id,
    int32_t* isstring) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  *isstring = ::NPN_IdentifierIsString(WireFormatToNPIdentifier(id));
  rpc->result = NACL_SRPC_RESULT_OK;
}

void NPModuleRpcServer::NPN_UTF8FromIdentifier(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t id,
    int32_t* success,
    char** str) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  char* name = ::NPN_UTF8FromIdentifier(WireFormatToNPIdentifier(id));
  if (NULL == name) {
    *success = NPERR_GENERIC_ERROR;
    *str = strdup("");
  } else {
    *success = NPERR_NO_ERROR;
    // Need to use NPN_MemFree on returned value, whereas srpc will do free().
    *str = strdup(name);
    NPN_MemFree(name);
  }
  rpc->result = NACL_SRPC_RESULT_OK;
}

void Device2DRpcServer::Device2DInitialize(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t wire_npp,
    NaClSrpcImcDescType* shm_desc,
    int32_t* stride,
    int32_t* left,
    int32_t* top,
    int32_t* right,
    int32_t* bottom) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPModule* module = NPModule::GetModule(wire_npp);

  rpc->result = module->Device2DInitialize(WireFormatToNPP(wire_npp),
                                           shm_desc,
                                           stride,
                                           left,
                                           top,
                                           right,
                                           bottom);
}

void Device2DRpcServer::Device2DFlush(NaClSrpcRpc* rpc,
                                      NaClSrpcClosure* done,
                                      int32_t wire_npp,
                                      int32_t* stride,
                                      int32_t* left,
                                      int32_t* top,
                                      int32_t* right,
                                      int32_t* bottom) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPModule* module = NPModule::GetModule(wire_npp);

  rpc->result = module->Device2DFlush(WireFormatToNPP(wire_npp),
                                      stride,
                                      left,
                                      top,
                                      right,
                                      bottom);
}

void Device2DRpcServer::Device2DDestroy(NaClSrpcRpc* rpc,
                                        NaClSrpcClosure* done,
                                        int32_t wire_npp) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPModule* module = NPModule::GetModule(wire_npp);

  rpc->result = module->Device2DDestroy(WireFormatToNPP(wire_npp));
}

void Device2DRpcServer::Device2DGetState(NaClSrpcRpc* rpc,
                                         NaClSrpcClosure* done,
                                         int32_t wire_npp,
                                         int32_t state,
                                         int32_t* value) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPModule* module = NPModule::GetModule(wire_npp);

  rpc->result = module->Device2DGetState(WireFormatToNPP(wire_npp),
                                         state,
                                         value);
}

void Device2DRpcServer::Device2DSetState(NaClSrpcRpc* rpc,
                                         NaClSrpcClosure* done,
                                         int32_t wire_npp,
                                         int32_t state,
                                         int32_t value) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPModule* module = NPModule::GetModule(wire_npp);

  rpc->result = module->Device2DSetState(WireFormatToNPP(wire_npp),
                                         state,
                                         value);
}

void Device3DRpcServer::Device3DInitialize(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t wire_npp,
    int32_t entries_requested,
    NaClSrpcImcDescType* shm_desc,
    int32_t* entries_obtained,
    int32_t* get_offset,
    int32_t* put_offset) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPModule* module = NPModule::GetModule(wire_npp);

  rpc->result = module->Device3DInitialize(WireFormatToNPP(wire_npp),
                                           entries_requested,
                                           shm_desc,
                                           entries_obtained,
                                           get_offset,
                                           put_offset);
}

void Device3DRpcServer::Device3DDestroy(NaClSrpcRpc* rpc,
                                        NaClSrpcClosure* done,
                                        int32_t wire_npp) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPModule* module = NPModule::GetModule(wire_npp);

  rpc->result = module->Device3DDestroy(WireFormatToNPP(wire_npp));
}

void Device3DRpcServer::Device3DGetState(NaClSrpcRpc* rpc,
                                         NaClSrpcClosure* done,
                                         int32_t wire_npp,
                                         int32_t state,
                                         int32_t* value) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPModule* module = NPModule::GetModule(wire_npp);

  rpc->result = module->Device3DGetState(WireFormatToNPP(wire_npp),
                                         state,
                                         value);
}

void Device3DRpcServer::Device3DSetState(NaClSrpcRpc* rpc,
                                         NaClSrpcClosure* done,
                                         int32_t wire_npp,
                                         int32_t state,
                                         int32_t value) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPModule* module = NPModule::GetModule(wire_npp);

  rpc->result = module->Device3DSetState(WireFormatToNPP(wire_npp),
                                         state,
                                         value);
}

void Device3DRpcServer::Device3DCreateBuffer(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t wire_npp,
    int32_t size,
    NaClSrpcImcDescType* shm_desc,
    int32_t* id) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPModule* module = NPModule::GetModule(wire_npp);

  rpc->result = module->Device3DCreateBuffer(WireFormatToNPP(wire_npp),
                                             size,
                                             shm_desc,
                                             id);
}

void Device3DRpcServer::Device3DDestroyBuffer(NaClSrpcRpc* rpc,
                                              NaClSrpcClosure* done,
                                              int32_t wire_npp,
                                              int32_t id) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPModule* module = NPModule::GetModule(wire_npp);

  rpc->result = module->Device3DDestroyBuffer(WireFormatToNPP(wire_npp), id);
}

void AudioRpcServer::AudioInitialize(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t wire_npp,
    int32_t closure_number,
    int32_t sample_rate,
    int32_t sample_type,
    int32_t output_channel_map,
    int32_t input_channel_map,
    int32_t sample_frame_count,
    int32_t flags) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPModule* module = NPModule::GetModule(wire_npp);

  rpc->result = module->AudioInitialize(WireFormatToNPP(wire_npp),
                                        closure_number,
                                        sample_rate,
                                        sample_type,
                                        output_channel_map,
                                        input_channel_map,
                                        sample_frame_count,
                                        flags);
}

void AudioRpcServer::AudioDestroy(NaClSrpcRpc* rpc,
                                  NaClSrpcClosure* done,
                                  int32_t wire_npp) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPModule* module = NPModule::GetModule(wire_npp);

  rpc->result = module->AudioDestroy(WireFormatToNPP(wire_npp));
}

void AudioRpcServer::AudioGetState(NaClSrpcRpc* rpc,
                                   NaClSrpcClosure* done,
                                   int32_t wire_npp,
                                   int32_t state,
                                   int32_t* value) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPModule* module = NPModule::GetModule(wire_npp);

  rpc->result = module->AudioGetState(WireFormatToNPP(wire_npp), state, value);
}

void AudioRpcServer::AudioSetState(NaClSrpcRpc* rpc,
                                   NaClSrpcClosure* done,
                                   int32_t wire_npp,
                                   int32_t state,
                                   int32_t value) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NPModule* module = NPModule::GetModule(wire_npp);

  rpc->result = module->AudioSetState(WireFormatToNPP(wire_npp), state, value);
}
