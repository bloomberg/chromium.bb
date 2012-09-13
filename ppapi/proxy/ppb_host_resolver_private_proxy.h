// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_HOST_RESOLVER_PRIVATE_PROXY_H_
#define PPAPI_PROXY_PPB_HOST_RESOLVER_PRIVATE_PROXY_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/private/ppb_host_resolver_shared.h"

namespace ppapi {
namespace proxy {

class PPB_HostResolver_Private_Proxy : public InterfaceProxy {
 public:
  PPB_HostResolver_Private_Proxy(Dispatcher* dispatcher);
  virtual~PPB_HostResolver_Private_Proxy();

  static PP_Resource CreateProxyResource(PP_Instance instance);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  static const ApiID kApiID = API_ID_PPB_HOSTRESOLVER_PRIVATE;

 private:
  // Browser->plugin message handlers.
  void OnMsgResolveACK(
      uint32 plugin_dispatcher_id,
      uint32 host_resolver_id,
      bool succeeded,
      const std::string& canonical_name,
      const std::vector<PP_NetAddress_Private>& net_address_list);

  DISALLOW_COPY_AND_ASSIGN(PPB_HostResolver_Private_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_HOST_RESOLVER_PRIVATE_PROXY_H_
