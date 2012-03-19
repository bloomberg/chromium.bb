// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_TALK_PRIVATE_PROXY_H_
#define PPAPI_PROXY_PPB_TALK_PRIVATE_PROXY_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/api_id.h"

namespace ppapi {
namespace proxy {

class PPB_Talk_Private_Proxy : public InterfaceProxy {
 public:
  PPB_Talk_Private_Proxy(Dispatcher* dispatcher);

  static PP_Resource CreateProxyResource(PP_Instance instance);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  static const ApiID kApiID = API_ID_PPB_TALK;

 private:
  // Message handlers.
  void OnMsgGetPermissionACK(uint32 dispatcher_id,
                             PP_Resource resource,
                             int32_t result);

  DISALLOW_COPY_AND_ASSIGN(PPB_Talk_Private_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_TALK_PRIVATE_PROXY_H_
