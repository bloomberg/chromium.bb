/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// A representation of an SRPC connection.  These can be either to the
// service runtime or to untrusted NaCl threads.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_CLIENT_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_CLIENT_H_

#include <map>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/plugin/utility.h"

namespace plugin {

class ConnectedSocket;
class MethodInfo;

//  SrpcClient represents an SRPC connection to a client.
class SrpcClient {
 public:
  //  Init is passed a ConnectedSocket.  It performs service
  //  discovery and provides the interface for future rpcs.
  bool Init(BrowserInterface* browser_interface, ConnectedSocket* socket);

  explicit SrpcClient();
  //  The destructor closes the connection to sel_ldr.
  ~SrpcClient();

  void StartJSObjectProxy(Plugin* plugin);
  //  Test whether the SRPC service has a given method.
  bool HasMethod(uintptr_t method_id);
  //  Invoke an SRPC method.
  bool Invoke(uintptr_t method_id, SrpcParams* params);
  bool InitParams(uintptr_t method_id, SrpcParams* params);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(SrpcClient);
  void GetMethods();
  typedef std::map<uintptr_t, MethodInfo*> Methods;
  Methods methods_;
  NaClSrpcChannel srpc_channel_;
  BrowserInterface* browser_interface_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_CLIENT_H_
