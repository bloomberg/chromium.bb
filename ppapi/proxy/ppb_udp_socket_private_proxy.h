// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_UDP_SOCKET_PRIVATE_PROXY_H_
#define PPAPI_PROXY_PPB_UDP_SOCKET_PRIVATE_PROXY_H_

#include <string>

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "ppapi/proxy/interface_proxy.h"

namespace ppapi {
namespace proxy {

class PPB_UDPSocket_Private_Proxy : public InterfaceProxy {
 public:
  PPB_UDPSocket_Private_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_UDPSocket_Private_Proxy();

  static PP_Resource CreateProxyResource(PP_Instance instance);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  static const ApiID kApiID = API_ID_PPB_UDPSOCKET_PRIVATE;

 private:
  // Browser->plugin message handlers.
  void OnMsgBindACK(uint32 plugin_dispatcher_id,
                    uint32 socket_id,
                    bool succeeded);
  void OnMsgRecvFromACK(uint32 plugin_dispatcher_id,
                        uint32 socket_id,
                        bool succeeded,
                        const std::string& data,
                        const PP_NetAddress_Private& addr);
  void OnMsgSendToACK(uint32 plugin_dispatcher_id,
                      uint32 socket_id,
                      bool succeeded,
                      int32_t bytes_written);

  DISALLOW_COPY_AND_ASSIGN(PPB_UDPSocket_Private_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_UDP_SOCKET_PRIVATE_PROXY_H_
