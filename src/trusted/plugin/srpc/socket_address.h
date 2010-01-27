/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
  // Return a string from a SocketAddress.
  static bool RpcToString(void* obj, SrpcParams* params);
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
