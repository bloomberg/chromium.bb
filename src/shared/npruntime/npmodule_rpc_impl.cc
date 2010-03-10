// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/checked_cast.h"
#include "native_client/src/shared/npruntime/npmodule.h"

#include "gen/native_client/src/shared/npruntime/npmodule_rpc.h"
#include "gen/native_client/src/shared/npruntime/npnavigator_rpc.h"
#include "gpu/command_buffer/common/command_buffer.h"
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
#include "native_client/src/trusted/plugin/srpc/closure.h"
#include "third_party/npapi/bindings/npapi_extensions.h"
#ifndef NACL_STANDALONE
#include "base/shared_memory.h"
#endif  // NACL_STANDALONE

using nacl::NPIdentifierToWireFormat;
using nacl::NPModule;
using nacl::NPObjectToWireFormat;
using nacl::WireFormatToNPIdentifier;
using nacl::WireFormatToNPP;

NaClSrpcError NPModuleRpcServer::NPN_GetValueBoolean(NaClSrpcChannel* channel,
                                                     int32_t wire_npp,
                                                     int32_t var,
                                                     int32_t* nperr,
                                                     int32_t* boolean_result) {
  UNREFERENCED_PARAMETER(channel);
  NPNVariable variable = static_cast<NPNVariable>(var);
  NPP npp = WireFormatToNPP(wire_npp);

  // There are two variables that are booleans.
  if (NPNVisOfflineBool != variable && NPNVprivateModeBool != variable) {
    *nperr = NPERR_INVALID_PARAM;
    return NACL_SRPC_RESULT_OK;
  }
  int32_t value;
  *nperr = ::NPN_GetValue(npp,
                          variable,
                          reinterpret_cast<void*>(&value));
  *boolean_result = static_cast<NPBool>(value);
  *nperr = NPERR_NO_ERROR;
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPModuleRpcServer::NPN_GetValueObject(
    NaClSrpcChannel* channel,
    int32_t wire_npp,
    int32_t var,
    int32_t* nperr,
    nacl_abi_size_t* result_length,
    char* result_bytes) {
  UNREFERENCED_PARAMETER(channel);
  NPNVariable variable = static_cast<NPNVariable>(var);
  NPP npp = WireFormatToNPP(wire_npp);

  if (NPNVWindowNPObject != variable && NPNVPluginElementNPObject != variable) {
    *nperr = NPERR_INVALID_PARAM;
    return NACL_SRPC_RESULT_OK;
  }
  // There are two variables that are NPObjects.
  NPObject* object;
  *nperr = ::NPN_GetValue(npp,
                          variable,
                          reinterpret_cast<void*>(&object));
  if (NULL == object) {
    *result_length = 0;
  } else {
    if (!NPObjectToWireFormat(npp, object, result_bytes, result_length)) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
  }
  *nperr = NPERR_NO_ERROR;
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPModuleRpcServer::NPN_SetStatus(NaClSrpcChannel* channel,
                                               int32_t wire_npp,
                                               char* status) {
  UNREFERENCED_PARAMETER(channel);

  if (NULL != status) {
    ::NPN_Status(WireFormatToNPP(wire_npp), status);
  }
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPModuleRpcServer::NPN_CreateArray(NaClSrpcChannel* channel,
                                                 int32_t wire_npp,
                                                 int32_t* success,
                                                 nacl_abi_size_t* object_length,
                                                 char* object_bytes) {
  UNREFERENCED_PARAMETER(channel);
  NPP npp = WireFormatToNPP(wire_npp);
  *success = 0;

  // Get the window object and create a new array.
  NPObject* window;
  if (NPERR_NO_ERROR != ::NPN_GetValue(npp, NPNVWindowNPObject, &window)) {
    return NACL_SRPC_RESULT_OK;
  }
  NPString script;
  const char scriptText[] = "new Array();";
  script.UTF8Characters = scriptText;
  script.UTF8Length = nacl::assert_cast<uint32>(strlen(scriptText));
  NPVariant result;
  VOID_TO_NPVARIANT(result);
  if (::NPN_Evaluate(npp, window, &script, &result) &&
      NPVARIANT_IS_OBJECT(result) &&
      NULL != NPVARIANT_TO_OBJECT(result)) {
    char* serialized_result = NPObjectToWireFormat(npp,
                                                   NPVARIANT_TO_OBJECT(result),
                                                   object_bytes,
                                                   object_length);
    // TODO(sehr): we should decrement a local reference count here to avoid
    // leaking result. It appears we need a NPN_ReleaseVariantValue(result).
    if (NULL == serialized_result) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
    *success = 1;
  } else {
    // Don't send anything back other than the success code.
    *object_length = 0;
  }
  NPN_ReleaseObject(window);

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPModuleRpcServer::NPN_GetURL(NaClSrpcChannel* channel,
                                            int32_t wire_npp,
                                            char* url,
                                            char* target,
                                            int32_t* nperr) {
  UNREFERENCED_PARAMETER(channel);

  if (NULL == url || NULL == target || NULL == nperr) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  if (0 == strlen(url) || 0 != strlen(target)) {
    *nperr = NPERR_GENERIC_ERROR;
  } else {
    NPModule* module = NPModule::GetModule(wire_npp);
    std::string url_origin = nacl::UrlToOrigin(url);

    nacl_srpc::NpGetUrlClosure* closure = new(std::nothrow)
        nacl_srpc::NpGetUrlClosure(WireFormatToNPP(wire_npp),
                                   module,
                                   url);
    if (NULL == closure) {
      *nperr = NPERR_GENERIC_ERROR;
    }
    closure->StartDownload();
    *nperr = NPERR_NO_ERROR;
  }

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPModuleRpcServer::NPN_GetIntIdentifier(NaClSrpcChannel* channel,
                                                      int32_t intval,
                                                      int32_t* id) {
  UNREFERENCED_PARAMETER(channel);
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

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPModuleRpcServer::NPN_IntFromIdentifier(NaClSrpcChannel* channel,
                                                       int32_t id,
                                                       int32_t* intval) {
  UNREFERENCED_PARAMETER(channel);

  *intval = ::NPN_IntFromIdentifier(WireFormatToNPIdentifier(id));

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPModuleRpcServer::NPN_GetStringIdentifier(
    NaClSrpcChannel* channel,
    char* strval,
    int32_t* id) {
  UNREFERENCED_PARAMETER(channel);

  *id = NPIdentifierToWireFormat(::NPN_GetStringIdentifier(strval));

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPModuleRpcServer::NPN_IdentifierIsString(
    NaClSrpcChannel* channel,
    int32_t id,
    int32_t* isstring) {
  UNREFERENCED_PARAMETER(channel);

  *isstring = ::NPN_IdentifierIsString(WireFormatToNPIdentifier(id));

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPModuleRpcServer::NPN_UTF8FromIdentifier(
    NaClSrpcChannel* channel,
    int32_t id,
    int32_t* success,
    char** str) {
  UNREFERENCED_PARAMETER(channel);

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

  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError Device2DRpcServer::Device2DInitialize(
    NaClSrpcChannel* channel,
    int32_t wire_npp,
    NaClSrpcImcDescType* shm_desc,
    int32_t* stride,
    int32_t* left,
    int32_t* top,
    int32_t* right,
    int32_t* bottom) {
  UNREFERENCED_PARAMETER(channel);
  NPModule* module = NPModule::GetModule(wire_npp);

  return module->Device2DInitialize(WireFormatToNPP(wire_npp),
                                    shm_desc,
                                    stride,
                                    left,
                                    top,
                                    right,
                                    bottom);
}

NaClSrpcError Device2DRpcServer::Device2DDestroy(NaClSrpcChannel* channel,
                                                 int32_t wire_npp) {
  UNREFERENCED_PARAMETER(channel);
  NPModule* module = NPModule::GetModule(wire_npp);

  return module->Device2DDestroy(WireFormatToNPP(wire_npp));
}

NaClSrpcError Device3DRpcServer::Device3DInitialize(
    NaClSrpcChannel* channel,
    int32_t wire_npp,
    int32_t entries_requested,
    NaClSrpcImcDescType* shm_desc,
    int32_t* entries_obtained,
    int32_t* get_offset,
    int32_t* put_offset) {
  UNREFERENCED_PARAMETER(channel);
  NPModule* module = NPModule::GetModule(wire_npp);

  return module->Device3DInitialize(WireFormatToNPP(wire_npp),
                                    entries_requested,
                                    shm_desc,
                                    entries_obtained,
                                    get_offset,
                                    put_offset);
}

NaClSrpcError Device3DRpcServer::Device3DDestroy(NaClSrpcChannel* channel,
                                                 int32_t wire_npp) {
  UNREFERENCED_PARAMETER(channel);
  NPModule* module = NPModule::GetModule(wire_npp);

  return module->Device3DDestroy(WireFormatToNPP(wire_npp));
}

NaClSrpcError Device3DRpcServer::Device3DGetState(NaClSrpcChannel* channel,
                                                  int32_t wire_npp,
                                                  int32_t state,
                                                  int32_t* value) {
  UNREFERENCED_PARAMETER(channel);
  NPModule* module = NPModule::GetModule(wire_npp);

  return module->Device3DGetState(WireFormatToNPP(wire_npp),
                                  state,
                                  value);
}

NaClSrpcError Device3DRpcServer::Device3DSetState(NaClSrpcChannel* channel,
                                                  int32_t wire_npp,
                                                  int32_t state,
                                                  int32_t value) {
  UNREFERENCED_PARAMETER(channel);
  NPModule* module = NPModule::GetModule(wire_npp);

  return module->Device3DSetState(WireFormatToNPP(wire_npp),
                                  state,
                                  value);
}

NaClSrpcError Device3DRpcServer::Device3DCreateBuffer(
    NaClSrpcChannel* channel,
    int32_t wire_npp,
    int32_t size,
    NaClSrpcImcDescType* shm_desc,
    int32_t* id) {
  UNREFERENCED_PARAMETER(channel);
  NPModule* module = NPModule::GetModule(wire_npp);

  return module->Device3DCreateBuffer(WireFormatToNPP(wire_npp),
                                      size,
                                      shm_desc,
                                      id);
}

NaClSrpcError Device3DRpcServer::Device3DDestroyBuffer(NaClSrpcChannel* channel,
                                                       int32_t wire_npp,
                                                       int32_t id) {
  UNREFERENCED_PARAMETER(channel);
  NPModule* module = NPModule::GetModule(wire_npp);

  return module->Device3DDestroyBuffer(WireFormatToNPP(wire_npp), id);
}

NaClSrpcError AudioRpcServer::AudioInitialize(
    NaClSrpcChannel* channel,
    int32_t wire_npp,
    int32_t closure_number,
    int32_t sample_rate,
    int32_t sample_type,
    int32_t output_channel_map,
    int32_t input_channel_map,
    int32_t sample_frame_count,
    int32_t flags) {
  UNREFERENCED_PARAMETER(channel);
  NPModule* module = NPModule::GetModule(wire_npp);

  return module->AudioInitialize(WireFormatToNPP(wire_npp),
                                 closure_number,
                                 sample_rate,
                                 sample_type,
                                 output_channel_map,
                                 input_channel_map,
                                 sample_frame_count,
                                 flags);
}

NaClSrpcError AudioRpcServer::AudioDestroy(NaClSrpcChannel* channel,
                                           int32_t wire_npp) {
  UNREFERENCED_PARAMETER(channel);
  NPModule* module = NPModule::GetModule(wire_npp);

  return module->AudioDestroy(WireFormatToNPP(wire_npp));
}

NaClSrpcError AudioRpcServer::AudioGetState(NaClSrpcChannel* channel,
                                            int32_t wire_npp,
                                            int32_t state,
                                            int32_t* value) {
  UNREFERENCED_PARAMETER(channel);
  NPModule* module = NPModule::GetModule(wire_npp);

  return module->AudioGetState(WireFormatToNPP(wire_npp), state, value);
}

NaClSrpcError AudioRpcServer::AudioSetState(NaClSrpcChannel* channel,
                                            int32_t wire_npp,
                                            int32_t state,
                                            int32_t value) {
  UNREFERENCED_PARAMETER(channel);
  NPModule* module = NPModule::GetModule(wire_npp);

  return module->AudioSetState(WireFormatToNPP(wire_npp), state, value);
}
