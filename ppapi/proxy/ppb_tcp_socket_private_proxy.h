// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_TCP_SOCKET_PRIVATE_PROXY_H_
#define PPAPI_PROXY_PPB_TCP_SOCKET_PRIVATE_PROXY_H_

#include <string>

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/private/ppb_tcp_socket_private.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/ppapi_proxy_export.h"

namespace ppapi {
namespace proxy {

class PPB_TCPSocket_Private_Proxy : public InterfaceProxy {
 public:
  PPB_TCPSocket_Private_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_TCPSocket_Private_Proxy();

  static PP_Resource CreateProxyResource(PP_Instance instance);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  static const ApiID kApiID = API_ID_PPB_TCPSOCKET_PRIVATE;

 private:
  // Browser->plugin message handlers.
  void OnMsgConnectACK(uint32 plugin_dispatcher_id,
                       uint32 socket_id,
                       bool succeeded,
                       const PP_NetAddress_Private& local_addr,
                       const PP_NetAddress_Private& remote_addr);
  void OnMsgSSLHandshakeACK(uint32 plugin_dispatcher_id,
                            uint32 socket_id,
                            bool succeeded);
  void OnMsgReadACK(uint32 plugin_dispatcher_id,
                    uint32 socket_id,
                    bool succeeded,
                    const std::string& data);
  void OnMsgWriteACK(uint32 plugin_dispatcher_id,
                     uint32 socket_id,
                     bool succeeded,
                     int32_t bytes_written);

  DISALLOW_COPY_AND_ASSIGN(PPB_TCPSocket_Private_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_TCP_SOCKET_PRIVATE_PROXY_H_
