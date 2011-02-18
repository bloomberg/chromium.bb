/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <string.h>
#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/connected_socket.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"
#include "native_client/src/trusted/plugin/socket_address.h"
#include "native_client/src/trusted/plugin/utility.h"

namespace {

bool RpcConnect(void* obj, plugin::SrpcParams *params) {
  plugin::SocketAddress* socket_addr =
      reinterpret_cast<plugin::SocketAddress*>(obj);
  plugin::ScriptableHandle* connected_socket = socket_addr->Connect();
  if (NULL == connected_socket) {
    return false;
  }
  params->outs()[0]->tag = NACL_SRPC_ARG_TYPE_OBJECT;
  params->outs()[0]->arrays.oval = connected_socket;
  return true;
}

}  // namespace

namespace plugin {

SocketAddress* SocketAddress::New(Plugin* plugin, nacl::DescWrapper* wrapper) {
  PLUGIN_PRINTF(("SocketAddress::New (plugin=%p)\n",
                 static_cast<void*>(plugin)));
  SocketAddress* socket_address = new(std::nothrow) SocketAddress();
  if (socket_address == NULL || !socket_address->Init(plugin, wrapper)) {
    delete socket_address;
    return NULL;
  }
  return socket_address;
}

bool SocketAddress::Init(Plugin* plugin, nacl::DescWrapper* wrapper) {
  if (!DescBasedHandle::Init(plugin, wrapper)) {
    return false;
  }
  LoadMethods();
  return true;
}

void SocketAddress::LoadMethods() {
  // Methods implemented by SocketAddresses.
  AddMethodCall(RpcConnect, "connect", "", "h");
}

SocketAddress::SocketAddress() {
  PLUGIN_PRINTF(("SocketAddress::SocketAddress (this=%p)\n",
                 static_cast<void*>(this)));
}

SocketAddress::~SocketAddress() {
  PLUGIN_PRINTF(("SocketAddress::~SocketAddress (this=%p)\n",
                 static_cast<void*>(this)));
}

// Returns a connected socket for the address.
ScriptableHandle* SocketAddress::Connect() {
  PLUGIN_PRINTF(("SocketAddress::Connect ()\n"));
  nacl::DescWrapper* connect_desc = wrapper()->Connect();
  if (NULL == connect_desc) {
    PLUGIN_PRINTF(("SocketAddress::Connect (connect failed)\n"));
    return NULL;
  } else {
    PLUGIN_PRINTF(("SocketAddress::Connect (conect_desc=%p)\n",
                   static_cast<void*>(connect_desc)));
    ConnectedSocket* portable_connected_socket =
        ConnectedSocket::New(plugin(), connect_desc);
    ScriptableHandle* connected_socket =
        plugin()->browser_interface()->NewScriptableHandle(
            portable_connected_socket);
    PLUGIN_PRINTF(("SocketAddress::Connect (connected_socket=%p)\n",
                   static_cast<void*>(connected_socket)));
    return connected_socket;
  }
}

}  // namespace plugin
