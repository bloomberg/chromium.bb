// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_Instance functions.

#include <string.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "srpcgen/ppp_rpc.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_instance_data.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppp_instance.h"

using nacl::scoped_ptr;
using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PPPInstanceInterface;

namespace {

// Several of the PPAPI methods return booleans.  These boolean values are done
// using SRPC's int32_t, so to be clearer, we define success and failure.
// TODO(sehr): fix SRPC's bool type.
const int32_t kMethodSuccess = 1;
const int32_t kMethodFailure = 0;

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
  // Deserialize the argv and argn strutures.
  scoped_ptr<const char*> argn_copy(GetCharpArray(argc, argn, argn_bytes));
  if (argn_copy.get() == NULL) {
    return;
  }
  scoped_ptr<const char*> argv_copy(GetCharpArray(argc, argv, argv_bytes));
  if (argv_copy.get() == NULL) {
    return;
  }
  bool created = PPPInstanceInterface()->DidCreate(instance,
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
  PPPInstanceInterface()->DidDestroy(instance);
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
  if (position_count != 4 || clip_count != 4) {
    return;
  }

  PPPInstanceInterface()->DidChangeView(
      instance_id, &position_rect, &clip_rect);
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
  PPPInstanceInterface()->DidChangeFocus(instance,
                                         static_cast<PP_Bool>(has_focus));
  DebugPrintf("PPP_Instance::DidChangeFocus\n");
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
  bool handled =
      PPPInstanceInterface()->HandleDocumentLoad(instance, url_loader);
  DebugPrintf("PPP_Instance::HandleDocumentLoad: handled=%d\n", handled);
  *success = handled ? kMethodSuccess : kMethodFailure;
  rpc->result = NACL_SRPC_RESULT_OK;
}
