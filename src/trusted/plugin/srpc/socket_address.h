/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// A portable representation of a scriptable socket address object.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SOCKET_ADDRESS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SOCKET_ADDRESS_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/plugin/srpc/desc_based_handle.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

namespace plugin {

class Plugin;
class ServiceRuntime;
class ScriptableHandle;


// SocketAddress is used to represent socket address descriptors.
class SocketAddress : public DescBasedHandle {
 public:
  // Create SocketAddress objects.
  static SocketAddress* New(Plugin* plugin, nacl::DescWrapper* wrapper);

  // Connect to a socket address.
  virtual ScriptableHandle* Connect();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(SocketAddress);
  SocketAddress();
  ~SocketAddress();
  bool Init(Plugin* plugin, nacl::DescWrapper* wrapper);
  void LoadMethods();
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SOCKET_ADDRESS_H_
