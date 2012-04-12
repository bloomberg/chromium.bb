// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_TCP_SOCKET_PRIVATE_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_TCP_SOCKET_PRIVATE_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/private/ppb_tcp_socket_private.h"

namespace ppapi_proxy {

// Implements the untrusted side of the PPB_TCPSocket_Private interface.
class PluginTCPSocketPrivate {
 public:
  static const PPB_TCPSocket_Private_0_3* GetInterface0_3();
  static const PPB_TCPSocket_Private_0_4* GetInterface0_4();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginTCPSocketPrivate);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_TCP_SOCKET_PRIVATE_H_
