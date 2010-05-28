/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
  bool Init(BrowserInterface* portable_plugin, ConnectedSocket* socket);

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

  typedef std::map<uintptr_t, MethodInfo*> Methods;
  Methods methods_;
  NaClSrpcChannel srpc_channel_;
  BrowserInterface* portable_plugin_;
  bool can_use_proxied_npapi_;

  static int number_alive_counter;
  static PLUGIN_JMPBUF srpc_env;
};

}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SRPC_CLIENT_H_
