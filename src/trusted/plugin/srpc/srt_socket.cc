/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <string.h>

#include "native_client/src/trusted/plugin/srpc/srt_socket.h"

#include "native_client/src/trusted/plugin/srpc/plugin.h"
#include "native_client/src/trusted/plugin/srpc/service_runtime_interface.h"
#include "native_client/src/trusted/plugin/srpc/srpc_client.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

namespace nacl_srpc {

int SrtSocket::kHardShutdownIdent;
int SrtSocket::kSetOriginIdent;
int SrtSocket::kStartModuleIdent;
int SrtSocket::kLogIdent;
int SrtSocket::kLoadModule;
int SrtSocket::kInitHandlePassing;

// NB: InitializeIdentifiers is not thread-safe.
void SrtSocket::InitializeIdentifiers(
    PortablePluginInterface* plugin_interface) {
  static bool initialized = false;

  UNREFERENCED_PARAMETER(plugin_interface);
  if (!initialized) {  // branch likely
    kHardShutdownIdent =
        PortablePluginInterface::GetStrIdentifierCallback("hard_shutdown");
    kSetOriginIdent =
        PortablePluginInterface::GetStrIdentifierCallback("set_origin");
    kStartModuleIdent =
        PortablePluginInterface::GetStrIdentifierCallback("start_module");
    kLogIdent = PortablePluginInterface::GetStrIdentifierCallback("log");
    kLoadModule =
        PortablePluginInterface::GetStrIdentifierCallback("load_module");
    kInitHandlePassing =
        PortablePluginInterface::GetStrIdentifierCallback(
            "init_handle_passing");
    initialized = true;
  }
}

SrtSocket::SrtSocket(ScriptableHandle<ConnectedSocket> *s,
                     PortablePluginInterface* plugin_interface)
    : connected_socket_(s),
      plugin_interface_(plugin_interface),
      is_shut_down_(false) {
  connected_socket_->AddRef();
  InitializeIdentifiers(plugin_interface_);  // inlineable tail call.
}

SrtSocket::~SrtSocket() {
  HardShutdown();
  connected_socket_->Unref();
}

bool SrtSocket::HardShutdown() {
  if (is_shut_down_) {
    dprintf(("SrtSocket::HardShutdown: already shut down.\n"));
    return true;
  }
  if (!(connected_socket()->HasMethod(kHardShutdownIdent, METHOD_CALL))) {
    dprintf(("No hard_shutdown method was found\n"));
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
  dprintf(("SrtSocket::HardShutdown %s\n",
           rpc_result ? "succeeded" : "failed"));
  if (rpc_result) {
    // invoke succeeded, so mark the module as shut down.
    is_shut_down_ = true;
  }
  return rpc_result;
}

bool SrtSocket::SetOrigin(std::string origin) {
  if (!(connected_socket()->HasMethod(kSetOriginIdent, METHOD_CALL))) {
    dprintf(("No set_origin method was found\n"));
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

  params.Input(0)->u.sval =
      PortablePluginInterface::MemAllocStrdup(origin.c_str());
  bool rpc_result = (connected_socket()->Invoke(kSetOriginIdent,
                                                METHOD_CALL,
                                                &params));
  return rpc_result;
}


bool SrtSocket::LoadModule(NaClSrpcImcDescType desc) {
  if (!(connected_socket()->HasMethod(kLoadModule, METHOD_CALL))) {
    dprintf(("No load_module method was found\n"));
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

  params.Input(0)->u.hval = desc;

  bool rpc_result = connected_socket()->Invoke(kLoadModule,
                                               METHOD_CALL,
                                               &params);
  return rpc_result;
}
#if NACL_WINDOWS && !defined(NACL_STANDALONE)
bool SrtSocket::InitHandlePassing(NaClSrpcImcDescType desc,
                                  nacl::Handle sel_ldr_handle) {
  if (!(connected_socket()->HasMethod(kInitHandlePassing, METHOD_CALL))) {
    dprintf(("No load_module method was found\n"));
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

  params.Input(0)->u.hval = desc;
  params.Input(1)->u.ival = my_pid;
  params.Input(2)->u.ival = reinterpret_cast<int>(my_handle_in_selldr);

  bool rpc_result = connected_socket()->Invoke(kInitHandlePassing,
                                               METHOD_CALL,
                                               &params);
  return rpc_result;
}
#endif

// make the start_module rpc
bool SrtSocket::StartModule(int *load_status) {
  if (!(connected_socket()->HasMethod(kStartModuleIdent, METHOD_CALL))) {
    dprintf(("No start_module method was found\n"));
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
    if (NACL_SRPC_ARG_TYPE_INT == params.Output(0)->tag) {
      int status = params.Output(0)->u.ival;
      dprintf(("StartModule: start_module RPC returned status code %d\n",
               status));
      if (NULL != load_status) {
        *load_status = status;
      }
    }
  }
  return rpc_result;
}

bool SrtSocket::Log(int severity, std::string msg) {
  if (!connected_socket()->HasMethod(kLogIdent, METHOD_CALL)) {
    dprintf(("No log method was found\n"));
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
  params.Input(0)->u.ival = severity;
  params.Input(1)->u.sval =
      PortablePluginInterface::MemAllocStrdup(msg.c_str());
  bool rpc_result = (connected_socket()->Invoke(kLogIdent,
                                                METHOD_CALL,
                                                &params));
  return rpc_result;
}

}  // namespace nacl_srpc
