/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// A representation of an SRPC connection.  These can be either to the
// service runtime or to untrusted NaCl threads.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_CLIENT_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_CLIENT_H_

#include <map>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/plugin/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

namespace nacl {
class DescWrapper;
}  // namespace nacl

namespace plugin {

class BrowserInterface;
class ErrorInfo;
class MethodInfo;
class Plugin;
class SrpcParams;

//  SrpcClient represents an SRPC connection to a client.
class SrpcClient {
 public:
  // Factory method for creating SrpcClients.
  static SrpcClient* New(Plugin* plugin, nacl::DescWrapper* wrapper);

  //  Init is passed a DescWrapper.  The SrpcClient performs service
  //  discovery and provides the interface for future rpcs.
  bool Init(BrowserInterface* browser_interface, nacl::DescWrapper* socket);

  //  The destructor closes the connection to sel_ldr.
  ~SrpcClient();

  bool StartJSObjectProxy(Plugin* plugin, ErrorInfo* error_info);
  //  Test whether the SRPC service has a given method.
  bool HasMethod(uintptr_t method_id);
  //  Invoke an SRPC method.
  bool Invoke(uintptr_t method_id, SrpcParams* params);
  bool InitParams(uintptr_t method_id, SrpcParams* params);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(SrpcClient);
  SrpcClient();
  void GetMethods();
  typedef std::map<uintptr_t, MethodInfo*> Methods;
  Methods methods_;
  NaClSrpcChannel srpc_channel_;
  bool srpc_channel_initialised_;
  BrowserInterface* browser_interface_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_CLIENT_H_
