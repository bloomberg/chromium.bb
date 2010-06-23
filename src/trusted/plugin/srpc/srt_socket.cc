/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <string.h>

#include "native_client/src/trusted/plugin/srpc/srt_socket.h"

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/plugin/srpc/browser_interface.h"
#include "native_client/src/trusted/plugin/srpc/plugin.h"
#include "native_client/src/trusted/plugin/srpc/service_runtime.h"
#include "native_client/src/trusted/plugin/srpc/srpc_client.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

namespace {

// Not really constants.  Do not modify.  Use only after at least
// one SrtSocket instance has been constructed.
uintptr_t kHardShutdownIdent;
uintptr_t kSetOriginIdent;
uintptr_t kStartModuleIdent;
uintptr_t kLogIdent;
uintptr_t kLoadModule;
uintptr_t kInitHandlePassing;

// NB: InitializeIdentifiers is not thread-safe.
void InitializeIdentifiers(plugin::BrowserInterface* browser_interface) {
  static bool initialized = false;

  if (!initialized) {  // branch likely
    kHardShutdownIdent =
        browser_interface->StringToIdentifier("hard_shutdown");
    kSetOriginIdent =
        browser_interface->StringToIdentifier("set_origin");
    kStartModuleIdent =
        browser_interface->StringToIdentifier("start_module");
    kLogIdent = browser_interface->StringToIdentifier("log");
    kLoadModule =
        browser_interface->StringToIdentifier("load_module");
    kInitHandlePassing =
        browser_interface->StringToIdentifier("init_handle_passing");
    initialized = true;
  }
}

}  // namespace

namespace plugin {

SrtSocket::SrtSocket(ScriptableHandle* s, BrowserInterface* browser_interface)
    : connected_socket_(s),
      browser_interface_(browser_interface),
      is_shut_down_(false) {
  connected_socket_->AddRef();
  InitializeIdentifiers(browser_interface_);  // inlineable tail call.
}

SrtSocket::~SrtSocket() {
  HardShutdown();
  connected_socket_->Unref();
}

bool SrtSocket::HardShutdown() {
  if (is_shut_down_) {
    PLUGIN_PRINTF(("SrtSocket::HardShutdown: already shut down.\n"));
    return true;
  }
  if (!(connected_socket()->HasMethod(kHardShutdownIdent, METHOD_CALL))) {
    PLUGIN_PRINTF(("No hard_shutdown method was found\n"));
    return false;
  }
  SrpcParams params;
  bool success;
  success = connected_socket()->InitParams(kHardShutdownIdent,
                                           METHOD_CALL,
                                           &params);
  if (!success) {
    return false;
  }
  bool rpc_result = (connected_socket()->Invoke(kHardShutdownIdent,
                                                METHOD_CALL,
                                                &params));
  PLUGIN_PRINTF(("SrtSocket::HardShutdown %s\n",
                 rpc_result ? "succeeded" : "failed"));
  if (rpc_result) {
    // invoke succeeded, so mark the module as shut down.
    is_shut_down_ = true;
  }
  return rpc_result;
}

bool SrtSocket::SetOrigin(nacl::string origin) {
  if (!(connected_socket()->HasMethod(kSetOriginIdent, METHOD_CALL))) {
    PLUGIN_PRINTF(("No set_origin method was found\n"));
    return false;
  }
  SrpcParams params;
  bool success;
  success = connected_socket()->InitParams(kSetOriginIdent,
                                           METHOD_CALL,
                                           &params);
  if (!success) {
    return false;
  }

  params.ins()[0]->u.sval = strdup(origin.c_str());
  bool rpc_result = (connected_socket()->Invoke(kSetOriginIdent,
                                                METHOD_CALL,
                                                &params));
  return rpc_result;
}


bool SrtSocket::LoadModule(NaClSrpcImcDescType desc) {
  if (!(connected_socket()->HasMethod(kLoadModule, METHOD_CALL))) {
    PLUGIN_PRINTF(("No load_module method was found\n"));
    return false;
  }
  SrpcParams params;
  bool success;
  success = connected_socket()->InitParams(kLoadModule,
                                           METHOD_CALL,
                                           &params);
  if (!success) {
    return false;
  }

  params.ins()[0]->u.hval = desc;

  bool rpc_result = connected_socket()->Invoke(kLoadModule,
                                               METHOD_CALL,
                                               &params);
  return rpc_result;
}
#if NACL_WINDOWS && !defined(NACL_STANDALONE)
bool SrtSocket::InitHandlePassing(NaClDesc* desc,
                                  nacl::Handle sel_ldr_handle) {
  if (!(connected_socket()->HasMethod(kInitHandlePassing, METHOD_CALL))) {
    PLUGIN_PRINTF(("No load_module method was found\n"));
    return false;
  }
  SrpcParams params;
  bool success;
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

  success = connected_socket()->InitParams(kInitHandlePassing,
                                           METHOD_CALL,
                                           &params);
  if (!success) {
    return false;
  }

  params.ins()[0]->u.hval = desc;
  params.ins()[1]->u.ival = my_pid;
  params.ins()[2]->u.ival = reinterpret_cast<int>(my_handle_in_selldr);

  bool rpc_result = connected_socket()->Invoke(kInitHandlePassing,
                                               METHOD_CALL,
                                               &params);
  return rpc_result;
}
#endif

// make the start_module rpc
bool SrtSocket::StartModule(int *load_status) {
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
  params.ins()[1]->u.sval = strdup(msg.c_str());
  bool rpc_result = (connected_socket()->Invoke(kLogIdent,
                                                METHOD_CALL,
                                                &params));
  return rpc_result;
}

}  // namespace plugin
