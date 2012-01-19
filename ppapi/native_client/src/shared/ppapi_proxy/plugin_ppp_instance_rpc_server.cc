// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_view.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/ppapi_proxy/view_data.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppp_instance.h"

using nacl::scoped_ptr;
using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PluginInstanceData;
using ppapi_proxy::PluginResource;
using ppapi_proxy::PluginView;
using ppapi_proxy::PPPInstanceInterface;
using ppapi_proxy::ViewData;

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
    PP_Resource resource,
    nacl_abi_size_t view_size,
    char* view_bytes) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);

  if (view_size != sizeof(ViewData))
    return;
  ViewData view_data;
  memcpy(&view_data, view_bytes, sizeof(ViewData));

  ppapi_proxy::PluginInstanceData* instance_data =
      ppapi_proxy::PluginInstanceData::FromPP(instance_id);
  if (!instance_data)
    return;

  scoped_refptr<PluginView> view =
      PluginResource::AdoptAsWithNoBrowserCount<PluginView>(resource);
  view->Init(view_data);

  instance_data->set_last_view_data(view_data);

  PPPInstanceInterface()->DidChangeView(
      instance_id, resource, &view_data.viewport_rect, &view_data.clip_rect);
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
  PPPInstanceInterface()->DidChangeFocus(instance, PP_FromBool(has_focus));
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
