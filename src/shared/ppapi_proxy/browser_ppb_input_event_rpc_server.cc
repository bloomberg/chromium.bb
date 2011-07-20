// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_InputEvent functions.

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp_input_event.h"
#include "native_client/src/shared/ppapi_proxy/trusted/srpcgen/ppb_rpc.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/third_party/ppapi/c/pp_errors.h"
#include "native_client/src/third_party/ppapi/c/ppb_input_event.h"


using ppapi_proxy::DebugPrintf;

void PpbInputEventRpcServer::PPB_InputEvent_RequestInputEvents(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t event_classes,
    int32_t filtering,
    int32_t* success)  {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  *success = PP_ERROR_FAILED;
  DebugPrintf("PPB_InputEvent::RequestInputEvents: instance=%"NACL_PRIu32", "
              "event_classes=%"NACL_PRIu32", filtering=%"NACL_PRId32"\n",
              instance, static_cast<uint32_t>(event_classes), filtering);
  const PPB_InputEvent* input_event_if = ppapi_proxy::PPBInputEventInterface();
  if (!input_event_if) {
    *success = PP_ERROR_NOTSUPPORTED;
    DebugPrintf("PPB_InputEvent::RequestInputEvents: success=%"NACL_PRId32"\n",
                *success);
    return;
  }
  if (filtering) {
    *success = input_event_if->RequestFilteringInputEvents(
        instance,
        static_cast<uint32_t>(event_classes));
  } else {
    *success = input_event_if->RequestInputEvents(
        instance,
        static_cast<uint32_t>(event_classes));
  }
  DebugPrintf("PPB_InputEvent::RequestInputEvents: success=%"NACL_PRId32"\n",
              *success);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbInputEventRpcServer::PPB_InputEvent_ClearInputEventRequest(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t event_classes)  {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  DebugPrintf("PPB_InputEvent::ClearInputEventRequest: instance=%"NACL_PRIu32
              ", event_classes=%"NACL_PRIu32"\n",
              instance, static_cast<uint32_t>(event_classes));
  const PPB_InputEvent* input_event_if = ppapi_proxy::PPBInputEventInterface();
  if (!input_event_if) {
    return;
  }
  input_event_if->ClearInputEventRequest(instance,
                                         static_cast<uint32_t>(event_classes));
  rpc->result = NACL_SRPC_RESULT_OK;
}

