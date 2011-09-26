// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_FLASH_UDP_SOCKET_PROXY_H_
#define PPAPI_PROXY_PPB_FLASH_UDP_SOCKET_PROXY_H_

#include <string>

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/private/ppb_flash_udp_socket.h"
#include "ppapi/proxy/interface_proxy.h"

namespace ppapi {
namespace proxy {

// The maximum number of bytes that each PpapiHostMsg_PPBFlashUDPSocket_RecvFrom
// message is allowed to request.
extern const int32_t kFlashUDPSocketMaxReadSize;
// The maximum number of bytes that each PpapiHostMsg_PPBFlashUDPSocket_SendTo
// message is allowed to carry.
extern const int32_t kFlashUDPSocketMaxWriteSize;

class PPB_Flash_UDPSocket_Proxy : public InterfaceProxy {
 public:
  PPB_Flash_UDPSocket_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Flash_UDPSocket_Proxy();

  static const Info* GetInfo();

  static PP_Resource CreateProxyResource(PP_Instance instance);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Browser->plugin message handlers.
  void OnMsgBindACK(uint32 plugin_dispatcher_id,
                    uint32 socket_id,
                    bool succeeded);
  void OnMsgRecvFromACK(uint32 plugin_dispatcher_id,
                        uint32 socket_id,
                        bool succeeded,
                        const std::string& data,
                        const PP_Flash_NetAddress& addr);
  void OnMsgSendToACK(uint32 plugin_dispatcher_id,
                      uint32 socket_id,
                      bool succeeded,
                      int32_t bytes_written);

  DISALLOW_COPY_AND_ASSIGN(PPB_Flash_UDPSocket_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_FLASH_UDP_SOCKET_PROXY_H_

