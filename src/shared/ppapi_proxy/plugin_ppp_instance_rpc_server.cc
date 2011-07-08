// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_Instance functions.

#include <string.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "srpcgen/ppp_rpc.h"
#include "native_client/src/shared/ppapi_proxy/object_capability.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/plugin_instance_data.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/third_party/ppapi/c/ppp.h"
#include "native_client/src/third_party/ppapi/c/pp_resource.h"
#include "native_client/src/third_party/ppapi/c/pp_var.h"
#include "native_client/src/third_party/ppapi/c/ppp_instance.h"

using ppapi_proxy::DebugPrintf;
using nacl::scoped_ptr;

namespace {

// Several of the PPAPI methods return booleans.  These boolean values are done
// using SRPC's int32_t, so to be clearer, we define success and failure.
// TODO(sehr): fix SRPC's bool type.
const int32_t kMethodSuccess = 1;
const int32_t kMethodFailure = 0;

const PPP_Instance* GetInstanceInterface() {
  static const PPP_Instance* instance_interface = NULL;
  if (instance_interface == NULL) {
    instance_interface = reinterpret_cast<const PPP_Instance*>(
        ::PPP_GetInterface(PPP_INSTANCE_INTERFACE));
  }
  return instance_interface;
}

// Build vector of "count" char pointers from a string of NUL-separated tokens.
// Returns NULL on out of memory or parsing error.
const char** GetCharpArray(uint32_t count, char* str, uint32_t total_len) {
  const char** array = new const char *[count];
  if (array == NULL) {
    return NULL;
  }
  const char* p = const_cast<const char*>(str);
  for (uint32_t i = 0; i < count; ++i) {
    array[i] = p;
    // Find the end of the current array element.
    while ('\0' != *p) {
      // We know that p >= str, so the cast preserves sign.
      if (total_len <= static_cast<uint32_t>(p - str)) {
        // Reached the end of the string before finding NUL.
        delete[] array;
        return NULL;
      }
      ++p;
    }
    // And find the next starting point (if any).
    // We know that p >= str, so the cast preserves sign.
    if (total_len > static_cast<uint32_t>(p - str)) {
      ++p;
    }
  }
  return array;
}

}  // namespace

void PppInstanceRpcServer::PPP_Instance_DidCreate(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    int32_t argc,
    uint32_t argn_bytes, char* argn,
    uint32_t argv_bytes, char* argv,
    // outputs
    int32_t* success) {
  ppapi_proxy::PluginInstanceData::DidCreate(instance);

  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  *success = kMethodFailure;
  const PPP_Instance* instance_interface = GetInstanceInterface();
  if (instance_interface == NULL || instance_interface->DidCreate == NULL) {
    return;
  }
  // Deserialize the argv and argn strutures.
  scoped_ptr<const char*> argn_copy(GetCharpArray(argc, argn, argn_bytes));
  if (argn_copy.get() == NULL) {
    return;
  }
  scoped_ptr<const char*> argv_copy(GetCharpArray(argc, argv, argv_bytes));
  if (argv_copy.get() == NULL) {
    return;
  }
  bool created = instance_interface->DidCreate(instance,
                                               argc,
                                               argn_copy.get(),
                                               argv_copy.get());
  DebugPrintf("PPP_Instance::DidCreate: created=%d\n", created);
  *success = created ? kMethodSuccess : kMethodFailure;
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PppInstanceRpcServer::PPP_Instance_DidDestroy(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance) {
  ppapi_proxy::PluginInstanceData::DidDestroy(instance);

  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);
  const PPP_Instance* instance_interface = GetInstanceInterface();
  if (instance_interface == NULL || instance_interface->DidDestroy == NULL) {
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }
  instance_interface->DidDestroy(instance);
  DebugPrintf("PPP_Instance::DidDestroy\n");
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PppInstanceRpcServer::PPP_Instance_DidChangeView(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance_id,
    uint32_t position_count, int32_t* position,
    uint32_t clip_count, int32_t* clip) {
  const PP_Rect position_rect =
      PP_MakeRectFromXYWH(position[0], position[1], position[2], position[3]);
  const PP_Rect clip_rect =
      PP_MakeRectFromXYWH(clip[0], clip[1], clip[2], clip[3]);

  ppapi_proxy::PluginInstanceData::DidChangeView(instance_id,
                                                 position_rect,
                                                 clip_rect);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);
  const PPP_Instance* instance_interface = GetInstanceInterface();
  if (instance_interface == NULL ||
      instance_interface->DidChangeView == NULL) {
    return;
  }
  if (position_count != 4 || clip_count != 4) {
    return;
  }

  instance_interface->DidChangeView(instance_id, &position_rect, &clip_rect);
  DebugPrintf("PPP_Instance::DidChangeView\n");
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PppInstanceRpcServer::PPP_Instance_DidChangeFocus(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    bool has_focus) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);
  // FocusChanged() has a void return, so it always succeeds at this interface
  // level.
  const PPP_Instance* instance_interface = GetInstanceInterface();
  if (instance_interface != NULL &&
      instance_interface->DidChangeFocus != NULL) {
    instance_interface->DidChangeFocus(instance,
        static_cast<PP_Bool>(has_focus));
  }
  DebugPrintf("PPP_Instance::DidChangeFocus\n");
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PppInstanceRpcServer::PPP_Instance_HandleInputEvent(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    uint32_t event_data_bytes, char* event_data,
    // outputs
    int32_t* success) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);
  *success = kMethodFailure;
  const PPP_Instance* instance_interface = GetInstanceInterface();
  if (instance_interface == NULL ||
      instance_interface->HandleInputEvent == NULL) {
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }
  const PP_InputEvent* event =
      reinterpret_cast<const PP_InputEvent*>(event_data);
  bool handled = instance_interface->HandleInputEvent(instance, event);
  DebugPrintf("PPP_Instance::HandleInputEvent: handled=%d\n", handled);
  *success = handled ? kMethodSuccess : kMethodFailure;
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PppInstanceRpcServer::PPP_Instance_HandleDocumentLoad(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    PP_Resource url_loader,
    // outputs
    int32_t* success) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);
  *success = kMethodFailure;
  const PPP_Instance* instance_interface = GetInstanceInterface();
  if (instance_interface == NULL ||
      instance_interface->HandleDocumentLoad == NULL) {
    return;
  }
  bool handled = instance_interface->HandleDocumentLoad(instance, url_loader);
  DebugPrintf("PPP_Instance::HandleDocumentLoad: handled=%d\n", handled);
  *success = handled ? kMethodSuccess : kMethodFailure;
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PppInstanceRpcServer::PPP_Instance_GetInstanceObject(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    // outputs
    uint32_t* capability_bytes, char* capability) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);
#ifdef PPAPI_INSTANCE_REMOVE_SCRIPTING
  NACL_NOTREACHED();
#else
  const PPP_Instance* instance_interface = GetInstanceInterface();
  if (instance_interface == NULL ||
      instance_interface->GetInstanceObject == NULL ||
      *capability_bytes < sizeof(ppapi_proxy::ObjectCapability)) {
    return;
  }
  PP_Var instance_var = instance_interface->GetInstanceObject(instance);
  DebugPrintf("PPP_Instance::GetInstanceObject: type=%d\n", instance_var.type);
  if (instance_var.type != PP_VARTYPE_OBJECT) {
    return;
  }
  // Create the return capability.
  ppapi_proxy::ObjectCapability cap(GETPID(), instance_var.value.as_id);
  *reinterpret_cast<ppapi_proxy::ObjectCapability*>(capability) = cap;
  *capability_bytes = sizeof(ppapi_proxy::ObjectCapability);
  rpc->result = NACL_SRPC_RESULT_OK;
#endif
}
