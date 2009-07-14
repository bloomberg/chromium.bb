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

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SOCKET_ADDRESS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SOCKET_ADDRESS_H_

#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/trusted/plugin/srpc/desc_based_handle.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

namespace nacl_srpc {

// Class declarations.
class Plugin;
class ConnectedSocket;
class ServiceRuntimeInterface;
template <typename HandleType>
class ScriptableHandle;


typedef DescHandleInitializer SocketAddressInitializer;

// SocketAddress is used to represent socket address descriptors.
class SocketAddress : public DescBasedHandle {
 public:
   explicit SocketAddress();
   ~SocketAddress();

  // Connect to a SocketAddress.
  static bool RpcConnect(void* obj, SrpcParams* params);
  ScriptableHandle<ConnectedSocket>* Connect(ServiceRuntimeInterface* sri);
  bool Init(struct PortableHandleInitializer* init_info);
  // override - this class implements a different set of methods
  void LoadMethods();

  static int number_alive() { return number_alive_counter; }

 private:
  // TODO(gregoryd) - what about the SignalHandler?
  static void SignalHandler(int value);
 private:
  static int number_alive_counter;
};

}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SOCKET_ADDRESS_H_
