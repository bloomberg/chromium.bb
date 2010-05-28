/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <signal.h>
#include <string.h>
#include "native_client/src/trusted/plugin/srpc/connected_socket.h"
#include "native_client/src/trusted/plugin/srpc/plugin.h"
#include "native_client/src/trusted/plugin/srpc/socket_address.h"

#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"

#include "native_client/src/trusted/plugin/srpc/utility.h"

namespace nacl_srpc {

int SocketAddress::number_alive_counter = 0;

bool SocketAddress::Init(struct PortableHandleInitializer* init_info) {
  if (!DescBasedHandle::Init(init_info)) {
    return false;
  }
  LoadMethods();
  return true;
}

// SocketAddress implements only the connect method.
void SocketAddress::LoadMethods() {
  AddMethodToMap(RpcConnect, "connect", METHOD_CALL, "", "h");
  AddMethodToMap(RpcToString, "toString", METHOD_CALL, "", "s");
}

bool SocketAddress::RpcConnect(void* obj, SrpcParams *params) {
  SocketAddress* socket_addr = reinterpret_cast<SocketAddress*>(obj);
  ScriptableHandle<ConnectedSocket>* connected_socket;
  connected_socket = socket_addr->Connect(NULL);
  if (NULL == connected_socket) {
    return false;
  }
  params->outs[0]->tag = NACL_SRPC_ARG_TYPE_OBJECT;
  params->outs[0]->u.oval =
      static_cast<BrowserScriptableObject*>(connected_socket);
  return true;
}

bool SocketAddress::RpcToString(void* obj, SrpcParams *params) {
  SocketAddress* socket_addr = reinterpret_cast<SocketAddress*>(obj);
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
  params->outs[0]->tag = NACL_SRPC_ARG_TYPE_STRING;
  params->outs[0]->u.sval = ret_string;
  return true;
}

SocketAddress::SocketAddress() {
  dprintf(("SocketAddress::SocketAddress(%p, %d)\n",
           static_cast<void *>(this),
           ++number_alive_counter));
}

SocketAddress::~SocketAddress() {
  dprintf(("SocketAddress::~SocketAddress(%p, %d)\n",
           static_cast<void *>(this),
           --number_alive_counter));
}

// Returns a connected socket for the address.
ScriptableHandle<ConnectedSocket>*
    SocketAddress::Connect(ServiceRuntimeInterface* sri) {
  dprintf(("SocketAddress::Connect(%p)\n", static_cast<void *>(sri)));
  nacl::DescWrapper* con_desc = wrapper()->Connect();
  if (NULL == con_desc) {
    dprintf(("SocketAddress::Connect: connect failed\n"));
    return NULL;
  } else {
    struct ConnectedSocketInitializer init_info(GetBrowserInterface(),
                                                con_desc,
                                                plugin_,
                                                true, sri);
    dprintf(("SocketAddress::Connect: take returned %p\n",
             static_cast<void *>(con_desc)));
    ScriptableHandle<ConnectedSocket>* connected_socket =
        ScriptableHandle<ConnectedSocket>::New(
            static_cast<PortableHandleInitializer*>(&init_info));
    dprintf(("SocketAddress::Connect: CS returned %p\n",
             static_cast<void *>(connected_socket)));
    return connected_socket;
  }
}

}  // namespace nacl_srpc
