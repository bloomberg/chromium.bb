// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_InputEvent functions.

#include <cstring>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/ppapi_proxy/input_event_data.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_var.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_input_event.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource_tracker.h"
#include "native_client/src/shared/ppapi_proxy/untrusted/srpcgen/ppp_rpc.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_input_event.h"

using nacl::scoped_ptr;
using ppapi_proxy::DebugPrintf;
using ppapi_proxy::DeserializeTo;
using ppapi_proxy::InputEventData;
using ppapi_proxy::PPPInputEventInterface;
using ppapi_proxy::PluginInputEvent;
using ppapi_proxy::PluginResource;
using ppapi_proxy::PluginResourceTracker;


void PppInputEventRpcServer::PPP_InputEvent_HandleInputEvent(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    PP_Resource browser_resource,
    uint32_t event_data_size, char* event_data,
    uint32_t character_text_size, char* character_text_bytes,
    // outputs
    int32_t* handled) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);
  *handled = static_cast<int32_t>(PP_FALSE);

  // The browser will immediately release this resource when we return, hence
  // we use a browser count of 0. If the NaCl instance calls AddRefResource,
  // the ResourceTracker will inform the browser synchronously, so the resource
  // will be retained.
  scoped_refptr<PluginInputEvent> input_event =
      PluginResource::AdoptAsWithNoBrowserCount<PluginInputEvent>(
          browser_resource);
  // Now deserialize the input event data and initialize the local
  // InputEventData with it. Note that we don't actually use the provided
  // PP_Resource for anything; we have all the data sent to us immediately to
  // avoid too much RPC in input event handling calls. Using the browser-
  // provided resource id, however, guarantees our PP_Resource is unique (since
  // all other resource ids used in the untrusted side were also provided by
  // the browser).
  InputEventData data;
  std::memcpy(&data, event_data, event_data_size);
  PP_Var character_text_var = PP_MakeUndefined();
  DeserializeTo(rpc->channel, character_text_bytes, character_text_size, 1,
                &character_text_var);
  input_event->Init(data, character_text_var);

  *handled = static_cast<int32_t>(
      PPPInputEventInterface()->HandleInputEvent(instance, browser_resource));
  DebugPrintf("PPP_InputEvent::HandleInputEvent: handled=%d\n", handled);
  // Now release the input, to match the behavior of the browser. If the NaCl
  // instance wants to retain the input event for any reason (e.g. to share it
  // to another thread), it must have called AddRefResource by now. If it has
  // not, then the input event resource will be deleted from
  // PluginResourceTracker's cache, and because the browser refcount was set to
  // 0, the resource tracker will *not* tell the browser. When we return, the
  // browser decrements its local reference count for us.
  PluginResourceTracker::Get()->UnrefResource(browser_resource);

  rpc->result = NACL_SRPC_RESULT_OK;
}
