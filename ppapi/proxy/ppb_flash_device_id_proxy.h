// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_FLASH_DEVICE_ID_PROXY_H_
#define PPAPI_PROXY_PPB_FLASH_DEVICE_ID_PROXY_H_

#include <string>

#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"

namespace ppapi {
namespace proxy {

class PPB_Flash_DeviceID_Proxy : public InterfaceProxy {
 public:
  PPB_Flash_DeviceID_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Flash_DeviceID_Proxy();

  static PP_Resource CreateProxyResource(PP_Instance instance);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  static const ApiID kApiID = API_ID_PPB_FLASH_DEVICE_ID;

 private:
  void OnPluginMsgGetReply(int32 routing_id,
                           PP_Resource resource,
                           int32 result,
                           const std::string& id);

  DISALLOW_COPY_AND_ASSIGN(PPB_Flash_DeviceID_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_FLASH_DEVICE_ID_PROXY_H_

