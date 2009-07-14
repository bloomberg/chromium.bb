/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

SocketAddress::SocketAddress(){
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
  int rv = (desc()->vtbl->ConnectAddr)(desc(), plugin_->effp_);
  dprintf(("SocketAddress::Connect: returned %d\n", rv));
  if (0 == rv) {
    struct NaClDesc* con_desc = NaClNrdXferEffectorTakeDesc(&plugin_->eff_);
    struct ConnectedSocketInitializer init_info(GetPortablePluginInterface(),
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
  } else {
    dprintf(("SocketAddress::Connect: connect failed\n"));
    return NULL;
  }
}

}  // namespace nacl_srpc
