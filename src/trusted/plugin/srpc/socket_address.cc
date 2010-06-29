/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <signal.h>
#include <string.h>
#include "native_client/src/trusted/plugin/srpc/browser_interface.h"
#include "native_client/src/trusted/plugin/srpc/connected_socket.h"
#include "native_client/src/trusted/plugin/srpc/plugin.h"
#include "native_client/src/trusted/plugin/srpc/socket_address.h"

#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"

#include "native_client/src/trusted/plugin/srpc/utility.h"

namespace {

bool RpcConnect(void* obj, plugin::SrpcParams *params) {
  plugin::SocketAddress* socket_addr =
      reinterpret_cast<plugin::SocketAddress*>(obj);
  plugin::ScriptableHandle* connected_socket = socket_addr->Connect(NULL);
  if (NULL == connected_socket) {
    return false;
  }
  params->outs()[0]->tag = NACL_SRPC_ARG_TYPE_OBJECT;
  params->outs()[0]->u.oval = connected_socket;
  return true;
}

bool RpcToString(void* obj, plugin::SrpcParams *params) {
  plugin::SocketAddress* socket_addr =
      reinterpret_cast<plugin::SocketAddress*>(obj);
  const char* str = socket_addr->wrapper()->conn_cap_path();
  if (NULL == str) {
    return false;
  }
  size_t len = strnlen(str, NACL_PATH_MAX);
  // strnlen ensures that len <= NACL_PATH_MAX < SIZE_T_MAX, so no overflow.
  char* ret_string = reinterpret_cast<char*>(malloc(len + 1));
  if (NULL == ret_string) {
    return false;
  }
  strncpy(ret_string, str, len + 1);
  params->outs()[0]->tag = NACL_SRPC_ARG_TYPE_STRING;
  params->outs()[0]->u.sval = ret_string;
  return true;
}

}  // namespace

namespace plugin {

SocketAddress* SocketAddress::New(Plugin* plugin, nacl::DescWrapper* wrapper) {
  PLUGIN_PRINTF(("SocketAddress::New()\n"));
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
  AddMethodCall(RpcToString, "toString", "", "s");
}

SocketAddress::SocketAddress() {
  PLUGIN_PRINTF(("SocketAddress::SocketAddress(%p)\n",
                 static_cast<void*>(this)));
}

SocketAddress::~SocketAddress() {
  PLUGIN_PRINTF(("SocketAddress::~SocketAddress(%p)\n",
                 static_cast<void*>(this)));
}

// Returns a connected socket for the address.
ScriptableHandle* SocketAddress::Connect(ServiceRuntime* sri) {
  PLUGIN_PRINTF(("SocketAddress::Connect(%p)\n", static_cast<void*>(sri)));
  nacl::DescWrapper* con_desc = wrapper()->Connect();
  if (NULL == con_desc) {
    PLUGIN_PRINTF(("SocketAddress::Connect: connect failed\n"));
    return NULL;
  } else {
    PLUGIN_PRINTF(("SocketAddress::Connect: take returned %p\n",
                   static_cast<void*>(con_desc)));
    ConnectedSocket* portable_connected_socket =
        ConnectedSocket::New(plugin(), con_desc, true, sri);
    ScriptableHandle* connected_socket =
        plugin()->browser_interface()->NewScriptableHandle(
            portable_connected_socket);
    PLUGIN_PRINTF(("SocketAddress::Connect: CS returned %p\n",
                   static_cast<void*>(connected_socket)));
    return connected_socket;
  }
}

}  // namespace plugin
