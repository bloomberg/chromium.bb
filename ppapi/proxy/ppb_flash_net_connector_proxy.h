// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_FLASH_NET_CONNECTOR_PROXY_H_
#define PPAPI_PROXY_PPB_FLASH_NET_CONNECTOR_PROXY_H_

#include "base/platform_file.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_non_thread_safe_ref_count.h"

struct PPB_Flash_NetConnector;

namespace ppapi {

class HostResource;

namespace proxy {

class PPB_Flash_NetConnector_Proxy : public InterfaceProxy {
 public:
  PPB_Flash_NetConnector_Proxy(Dispatcher* dispatcher,
                               const void* target_interface);
  virtual ~PPB_Flash_NetConnector_Proxy();

  static const Info* GetInfo();

  static PP_Resource CreateProxyResource(PP_Instance instance);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  struct ConnectCallbackInfo;

  // Plugin->host message handlers.
  void OnMsgCreate(PP_Instance instance,
                   ppapi::HostResource* result);
  void OnMsgConnectTcp(const ppapi::HostResource& resource,
                       const std::string& host,
                       uint16_t port);
  void OnMsgConnectTcpAddress(const ppapi::HostResource& resource_id,
                              const std::string& net_address_as_string);

  // Host->plugin message handler.
  void OnMsgConnectACK(const ppapi::HostResource& host_resource,
                       int32_t result,
                       IPC::PlatformFileForTransit handle,
                       const std::string& load_addr_as_string,
                       const std::string& remote_addr_as_string);

  void OnCompleteCallbackInHost(int32_t result, ConnectCallbackInfo* info);

  pp::CompletionCallbackFactory<PPB_Flash_NetConnector_Proxy,
                                ProxyNonThreadSafeRefCount> callback_factory_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_FLASH_NET_CONNECTOR_PROXY_H_
