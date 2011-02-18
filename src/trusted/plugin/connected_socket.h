/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// Portable representation of a scriptable connected socket.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_CONNECTED_SOCKET_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_CONNECTED_SOCKET_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/desc_based_handle.h"
#include "native_client/src/trusted/plugin/utility.h"

namespace plugin {

class Plugin;
class ServiceRuntime;
class SrpcClient;

// ConnectedSocket represents a connected socket that results from loading
// a NativeClient module or doing a connect on a received descriptor
// (SocketAddress).
class ConnectedSocket : public DescBasedHandle {
 public:
  static ConnectedSocket* New(Plugin* plugin, nacl::DescWrapper* desc);

  virtual bool InvokeEx(uintptr_t method_id,
                        CallType call_type,
                        SrpcParams* params);
  virtual bool HasMethodEx(uintptr_t method_id, CallType call_type);
  virtual bool InitParamsEx(uintptr_t method_id,
                            CallType call_type,
                            SrpcParams* params);
  void StartJSObjectProxy(Plugin* plugin);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ConnectedSocket);
  ConnectedSocket();
  virtual ~ConnectedSocket();
  bool Init(Plugin* plugin, nacl::DescWrapper* desc);
  SrpcClient* srpc_client_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_CONNECTED_SOCKET_H_
