// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_BROKER_PROXY_H_
#define PPAPI_PPB_BROKER_PROXY_H_

#include "base/sync_socket.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_non_thread_safe_ref_count.h"

struct PPB_BrokerTrusted;

namespace ppapi {

class HostResource;

namespace proxy {

class PPB_Broker_Proxy : public InterfaceProxy {
 public:
  PPB_Broker_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Broker_Proxy();

  static PP_Resource CreateProxyResource(PP_Instance instance);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  static const InterfaceID kInterfaceID = INTERFACE_ID_PPB_BROKER;

 private:
  // Message handlers.
  void OnMsgCreate(PP_Instance instance, ppapi::HostResource* result_resource);
  void OnMsgConnect(const ppapi::HostResource& broker);
  void OnMsgConnectComplete(const ppapi::HostResource& broker,
                            IPC::PlatformFileForTransit foreign_socket_handle,
                            int32_t result);

  void ConnectCompleteInHost(int32_t result,
                             const ppapi::HostResource& host_resource);

  pp::CompletionCallbackFactory<PPB_Broker_Proxy,
                                ProxyNonThreadSafeRefCount> callback_factory_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PPB_BROKER_PROXY_H_
