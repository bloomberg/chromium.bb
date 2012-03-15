// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_TCP_SERVER_SOCKET_PRIVATE_PROXY_H_
#define PPAPI_PROXY_PPB_TCP_SERVER_SOCKET_PRIVATE_PROXY_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/proxy/interface_proxy.h"

namespace ppapi {

class PPB_TCPServerSocket_Shared;

namespace proxy {

class PPB_TCPServerSocket_Private_Proxy : public InterfaceProxy {
 public:
  explicit PPB_TCPServerSocket_Private_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_TCPServerSocket_Private_Proxy();

  static PP_Resource CreateProxyResource(PP_Instance instance);

  void ObjectDestroyed(uint32 socket_id);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  static const ApiID kApiID = API_ID_PPB_TCPSERVERSOCKET_PRIVATE;

 private:
  typedef std::map<uint32, PPB_TCPServerSocket_Shared*>
    IDToServerSocketMap;

  void OnMsgListenACK(uint32 plugin_dispatcher_id,
                      PP_Resource socket_resource,
                      uint32 socket_id,
                      int32_t status);
  void OnMsgAcceptACK(uint32 plugin_dispatcher_id,
                      uint32 server_socket_id,
                      uint32 accepted_socket_id,
                      const PP_NetAddress_Private& local_addr,
                      const PP_NetAddress_Private& remote_addr);

  IDToServerSocketMap id_to_server_socket_;

  DISALLOW_COPY_AND_ASSIGN(PPB_TCPServerSocket_Private_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_TCP_SERVER_SOCKET_PRIVATE_PROXY_H_
