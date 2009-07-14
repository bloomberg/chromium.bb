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


// NPAPI Simple RPC Interface

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_CONNECTED_SOCKET_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_CONNECTED_SOCKET_H_

// TODO(gregoryd): reduce the headers needed for this.
#include <setjmp.h>
#include "native_client/src/trusted/plugin/srpc/desc_based_handle.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

namespace nacl_srpc {

// Forward declarations for externals.
class Plugin;
class ServiceRuntimeInterface;
class SharedMemory;
class SrpcClient;
struct DescHandleInitializer;
class DescBasedHandle;

struct ConnectedSocketInitializer: DescHandleInitializer {
  bool is_srpc_client_;
  ServiceRuntimeInterface* serv_rtm_info_;
  bool is_command_channel_;
  ConnectedSocketInitializer(PortablePluginInterface* plugin_interface,
                             struct NaClDesc* desc,
                             Plugin* plugin,
                             bool is_srpc_client,
                             ServiceRuntimeInterface* serv_rtm_info):
      DescHandleInitializer(plugin_interface, desc, plugin),
      is_srpc_client_(is_srpc_client),
      serv_rtm_info_(serv_rtm_info) {}
};


// ConnectedSocket represents a connected socket that results from loading
// a NativeClient module or doing a connect on a received descriptor
// (SocketAddress).
class ConnectedSocket : public DescBasedHandle {
 public:
  bool Init(struct PortableHandleInitializer* init_info);
  void LoadMethods();
  virtual bool InvokeEx(int method_id, CallType call_type, SrpcParams* params);
  virtual bool HasMethodEx(int method_id, CallType call_type);
  virtual bool InitParamsEx(int method_id,
                            CallType call_type,
                            SrpcParams* params);

  explicit ConnectedSocket();
  // ServiceRuntimeInterface instances need to delete an instance.
  ~ConnectedSocket();

  static int number_alive() { return number_alive_counter; }

 private:
  static void SignalHandler(int value);
  NaClSrpcArg* GetSignatureObject();

  // registered methods
  static bool SignaturesGetProperty(void* obj, SrpcParams* params);

 private:
  static int number_alive_counter;
  NaClSrpcArg* signatures_;
  ServiceRuntimeInterface* service_runtime_info_;
  SrpcClient* srpc_client_;
  static PLUGIN_JMPBUF socket_env;
};

}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_CONNECTED_SOCKET_H_
