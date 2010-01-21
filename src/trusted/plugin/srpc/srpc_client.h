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

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SRPC_CLIENT_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SRPC_CLIENT_H_

// TODO(gregoryd): there are still too many includes here, I think.
#include <map>
#include "native_client/src/trusted/plugin/srpc/connected_socket.h"
#include "native_client/src/trusted/plugin/srpc/plugin.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

namespace nacl_srpc {

//  SrpcClients are implemented over ConnectedSockets.
class ConnectedSocket;

//  MethodInfo is used by ServiceRuntimeInterface to maintain an efficient
//  lookup for method id numbers and type strings.
class MethodInfo;

//  SrpcClient represents an SRPC connection to a client.
class SrpcClient {
 public:
  explicit SrpcClient(bool can_use_proxied_npapi);
  //  Init is passed a ConnectedSocket.  It performs service
  //  discovery and provides the interface for future rpcs.
  bool Init(PortablePluginInterface* portable_plugin, ConnectedSocket* socket);

  //  The destructor closes the connection to sel_ldr.
  ~SrpcClient();

  //  Test whether the SRPC service has a given method.
  bool HasMethod(uintptr_t method_id);
  //  Invoke an SRPC method.
  bool Invoke(uintptr_t method_id, SrpcParams* params);
  bool InitParams(uintptr_t method_id, SrpcParams* params);

  // Build the signature list object, for use in display, etc.
  NaClSrpcArg* GetSignatureObject();

 private:
  // TODO(sehr): We need a DISALLOW_COPY_AND_ASSIGN macro.
  SrpcClient(const SrpcClient&);
  void operator=(const SrpcClient&);

  static void SignalHandler(int value);
  void GetMethods();

  std::map<int, MethodInfo*> methods_;
  NaClSrpcChannel srpc_channel_;
  PortablePluginInterface* portable_plugin_;
  bool can_use_proxied_npapi_;

  static int number_alive_counter;
  static PLUGIN_JMPBUF srpc_env;
};

}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SRPC_CLIENT_H_
