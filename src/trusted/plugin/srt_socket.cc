/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include <string.h>

#include "native_client/src/trusted/plugin/srt_socket.h"

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/service_runtime.h"
#include "native_client/src/trusted/plugin/srpc_client.h"
#include "native_client/src/trusted/plugin/utility.h"

namespace plugin {

SrtSocket::SrtSocket(ScriptableHandle* s, BrowserInterface* browser_interface)
    : connected_socket_(s),
      browser_interface_(browser_interface) {
}

SrtSocket::~SrtSocket() {
  connected_socket_->Unref();
}

bool SrtSocket::ReverseSetup(NaClSrpcImcDescType *out_conn) {
  const uintptr_t kReverseChannelIdent =
      browser_interface_->StringToIdentifier("reverse_setup");
  if (!(connected_socket()->HasMethod(kReverseChannelIdent, METHOD_CALL))) {
    PLUGIN_PRINTF(("SrtSocket::ReverseSetup"
                   " (no reverse_setup method found)\n"));
    return false;
  }
  SrpcParams params;
  bool success = connected_socket()->InitParams(kReverseChannelIdent,
                                                METHOD_CALL,
                                                &params);
  if (!success) {
    return false;
  }
  bool rpc_result = connected_socket()->Invoke(kReverseChannelIdent,
                                               METHOD_CALL,
                                               &params);
  if (rpc_result) {
    if (NACL_SRPC_ARG_TYPE_HANDLE == params.outs()[0]->tag) {
      *out_conn = params.outs()[0]->u.hval;
    }
  }
  return rpc_result;
}

bool SrtSocket::LoadModule(NaClSrpcImcDescType desc) {
  const uintptr_t kLoadModuleIdent =
      browser_interface_->StringToIdentifier("load_module");
  if (!(connected_socket()->HasMethod(kLoadModuleIdent, METHOD_CALL))) {
    PLUGIN_PRINTF(("SrtSocket::LoadModule (no load_module method found)\n"));
    return false;
  }
  SrpcParams params;
  bool success = connected_socket()->InitParams(kLoadModuleIdent,
                                                METHOD_CALL,
                                                &params);
  if (!success) {
    return false;
  }

  params.ins()[0]->u.hval = desc;
  params.ins()[1]->arrays.str = strdup("place holder");
  if (NULL == params.ins()[1]->arrays.str) {
    return false;
  }

  bool rpc_result = connected_socket()->Invoke(kLoadModuleIdent,
                                               METHOD_CALL,
                                               &params);
  return rpc_result;
}
#if NACL_WINDOWS && !defined(NACL_STANDALONE)
bool SrtSocket::InitHandlePassing(NaClDesc* desc,
                                  nacl::Handle sel_ldr_handle) {
  const uintptr_t kInitHandlePassingIdent =
      browser_interface_->StringToIdentifier("init_handle_passing");
  if (!(connected_socket()->HasMethod(kInitHandlePassingIdent, METHOD_CALL))) {
    PLUGIN_PRINTF(("SrtSocket::InitHandlePassing "
                   "(no load_module method found)\n"));
    return false;
  }
  SrpcParams params;
  DWORD my_pid = GetCurrentProcessId();
  nacl::Handle my_handle = GetCurrentProcess();
  nacl::Handle my_handle_in_selldr;

  if (!DuplicateHandle(GetCurrentProcess(),
                       my_handle,
                       sel_ldr_handle,
                       &my_handle_in_selldr,
                       PROCESS_DUP_HANDLE,
                       FALSE,
                       0)) {
    return false;
  }

  bool success = connected_socket()->InitParams(kInitHandlePassingIdent,
                                                METHOD_CALL,
                                                &params);
  if (!success) {
    return false;
  }

  params.ins()[0]->u.hval = desc;
  params.ins()[1]->u.ival = my_pid;
  params.ins()[2]->u.ival = reinterpret_cast<int>(my_handle_in_selldr);

  bool rpc_result = connected_socket()->Invoke(kInitHandlePassingIdent,
                                               METHOD_CALL,
                                               &params);
  return rpc_result;
}
#endif

// make the start_module rpc
bool SrtSocket::StartModule(int *load_status) {
  const uintptr_t kStartModuleIdent =
      browser_interface_->StringToIdentifier("start_module");
  if (!(connected_socket()->HasMethod(kStartModuleIdent, METHOD_CALL))) {
    PLUGIN_PRINTF(("No start_module method was found\n"));
    return false;
  }
  SrpcParams params;
  bool success;
  success = connected_socket()->InitParams(kStartModuleIdent,
                                           METHOD_CALL,
                                           &params);
  if (!success) {
    return false;
  }
  bool rpc_result = connected_socket()->Invoke(kStartModuleIdent,
                                               METHOD_CALL,
                                               &params);
  if (rpc_result) {
    if (NACL_SRPC_ARG_TYPE_INT == params.outs()[0]->tag) {
      int status = params.outs()[0]->u.ival;
      PLUGIN_PRINTF(("StartModule: start_module RPC returned status code %d\n",
                     status));
      if (NULL != load_status) {
        *load_status = status;
      }
    }
  }
  return rpc_result;
}

bool SrtSocket::Log(int severity, nacl::string msg) {
  const uintptr_t kLogIdent = browser_interface_->StringToIdentifier("log");
  if (!connected_socket()->HasMethod(kLogIdent, METHOD_CALL)) {
    PLUGIN_PRINTF(("No log method was found\n"));
    return false;
  }
  SrpcParams params;
  bool success;
  success = connected_socket()->InitParams(kLogIdent,
                                           METHOD_CALL,
                                           &params);
  if (!success) {
    return false;
  }
  params.ins()[0]->u.ival = severity;
  params.ins()[1]->arrays.str = strdup(msg.c_str());
  if (NULL == params.ins()[1]->arrays.str) {
    return false;
  }
  bool rpc_result = (connected_socket()->Invoke(kLogIdent,
                                                METHOD_CALL,
                                                &params));
  return rpc_result;
}

}  // namespace plugin
