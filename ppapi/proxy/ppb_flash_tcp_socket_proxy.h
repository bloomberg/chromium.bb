// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_FLASH_TCP_SOCKET_PROXY_H_
#define PPAPI_PROXY_PPB_FLASH_TCP_SOCKET_PROXY_H_

#include <string>

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/private/ppb_flash_tcp_socket.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/ppapi_proxy_export.h"

namespace ppapi {
namespace proxy {

// The maximum number of bytes that each PpapiHostMsg_PPBFlashTCPSocket_Read
// message is allowed to request.
PPAPI_PROXY_EXPORT extern const int32_t kFlashTCPSocketMaxReadSize;
// The maximum number of bytes that each PpapiHostMsg_PPBFlashTCPSocket_Write
// message is allowed to carry.
PPAPI_PROXY_EXPORT extern const int32_t kFlashTCPSocketMaxWriteSize;

class PPB_Flash_TCPSocket_Proxy : public InterfaceProxy {
 public:
  PPB_Flash_TCPSocket_Proxy(Dispatcher* dispatcher,
                            const void* target_interface);
  virtual ~PPB_Flash_TCPSocket_Proxy();

  static const Info* GetInfo();

  static PP_Resource CreateProxyResource(PP_Instance instance);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Browser->plugin message handlers.
  void OnMsgConnectACK(uint32 plugin_dispatcher_id,
                       uint32 socket_id,
                       bool succeeded,
                       const PP_Flash_NetAddress& local_addr,
                       const PP_Flash_NetAddress& remote_addr);
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

  DISALLOW_COPY_AND_ASSIGN(PPB_Flash_TCPSocket_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_FLASH_TCP_SOCKET_PROXY_H_
