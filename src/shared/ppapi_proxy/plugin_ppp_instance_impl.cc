// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "gen/native_client/src/shared/ppapi_proxy/ppp_rpc.h"
#include "native_client/src/shared/ppapi_proxy/object_capability.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppp_instance.h"

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
static const char** GetCharpArray(uint32_t count,
                                  char* str,
                                  uint32_t total_len) {
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

//
// The following methods are the SRPC dispatchers for ppapi/c/ppp_instance.h.
//

NaClSrpcError PppInstanceRpcServer::PPP_Instance_DidCreate(
    NaClSrpcChannel* channel,
    int64_t instance,
    int32_t argc,
    uint32_t argn_bytes,
    char* argn,
    uint32_t argv_bytes,
    char* argv,
    int32_t* success) {
  *success = kMethodFailure;
  const PPP_Instance* instance_interface = GetInstanceInterface();
  if (instance_interface == NULL || instance_interface->DidCreate == NULL) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  // Deserialize the argv and argn strutures.
  scoped_ptr<const char*> argn_copy(GetCharpArray(argc, argn, argn_bytes));
  if (argn_copy.get() == NULL) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  scoped_ptr<const char*> argv_copy(GetCharpArray(argc, argv, argv_bytes));
  if (argv_copy.get() == NULL) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  bool initialize_result = instance_interface->DidCreate(instance,
                                                         argc,
                                                         argn_copy.get(),
                                                         argv_copy.get());
  *success = initialize_result ? kMethodSuccess : kMethodFailure;
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError PppInstanceRpcServer::PPP_Instance_DidDestroy(
    NaClSrpcChannel* channel,
    int64_t instance) {
  const PPP_Instance* instance_interface = GetInstanceInterface();
  if (instance_interface == NULL || instance_interface->DidDestroy == NULL) {
    return NACL_SRPC_RESULT_OK;
  }
  instance_interface->DidDestroy(instance);
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError PppInstanceRpcServer::PPP_Instance_DidChangeView(
    NaClSrpcChannel* channel,
    int64_t instance,
    uint32_t position_count,
    int32_t* position,
    uint32_t clip_count,
    int32_t* clip) {
  const PPP_Instance* instance_interface = GetInstanceInterface();
  if (instance_interface == NULL || instance_interface->DidChangeView == NULL) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  if (position_count != 4 || clip_count != 4) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  const PP_Rect position_rect =
      PP_MakeRectFromXYWH(position[0], position[1], position[2], position[3]);
  const PP_Rect clip_rect =
      PP_MakeRectFromXYWH(clip[0], clip[1], clip[2], clip[3]);

  instance_interface->DidChangeView(instance, &position_rect, &clip_rect);
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError PppInstanceRpcServer::PPP_Instance_DidChangeFocus(
    NaClSrpcChannel* channel,
    int64_t instance,
    bool has_focus) {
  // FocusChanged() has a void return, so it always succeeds at this interface
  // level.
  const PPP_Instance* instance_interface = GetInstanceInterface();
  if (instance_interface != NULL &&
      instance_interface->DidChangeFocus != NULL) {
    instance_interface->DidChangeFocus(instance,
        static_cast<PP_Bool>(has_focus));
  }
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError PppInstanceRpcServer::PPP_Instance_HandleInputEvent(
    NaClSrpcChannel* channel,
    int64_t instance,
    uint32_t event_data_bytes,
    char* event_data,
    int32_t* success) {
  *success = kMethodFailure;
  const PPP_Instance* instance_interface = GetInstanceInterface();
  if (instance_interface == NULL ||
      instance_interface->HandleInputEvent == NULL) {
    return NACL_SRPC_RESULT_OK;
  }
  const PP_InputEvent* event =
      reinterpret_cast<const PP_InputEvent*>(event_data);
  bool event_result = instance_interface->HandleInputEvent(instance, event);
  *success = event_result ? kMethodSuccess : kMethodFailure;
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError PppInstanceRpcServer::PPP_Instance_HandleDocumentLoad(
    NaClSrpcChannel* channel,
    int64_t instance,
    int64_t url_loader,
    int32_t* success) {
  *success = kMethodFailure;
  const PPP_Instance* instance_interface = GetInstanceInterface();
  if (instance_interface == NULL ||
      instance_interface->HandleDocumentLoad == NULL) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  bool handle_result = instance_interface->HandleDocumentLoad(instance,
                                                              url_loader);
  *success = handle_result ? kMethodSuccess : kMethodFailure;
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError PppInstanceRpcServer::PPP_Instance_GetInstanceObject(
    NaClSrpcChannel* channel,
    int64_t instance,
    uint32_t* capability_bytes,
    char* capability) {
  const PPP_Instance* instance_interface = GetInstanceInterface();
  if (instance_interface == NULL ||
      instance_interface->GetInstanceObject == NULL ||
      *capability_bytes < sizeof(ppapi_proxy::ObjectCapability)) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  PP_Var instance_var = instance_interface->GetInstanceObject(instance);
  if (instance_var.type != PP_VARTYPE_OBJECT) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  // Create the return capability.
  ppapi_proxy::ObjectCapability cap(GETPID(), instance_var.value.as_id);
  *reinterpret_cast<ppapi_proxy::ObjectCapability*>(capability) = cap;
  *capability_bytes = sizeof(ppapi_proxy::ObjectCapability);
  return NACL_SRPC_RESULT_OK;
}

