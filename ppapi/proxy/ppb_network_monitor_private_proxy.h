// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_NETWORK_MONITOR_PRIVATE_PROXY_H_
#define PPAPI_PROXY_PPB_NETWORK_MONITOR_PRIVATE_PROXY_H_

#include <list>

#include "base/observer_list_threadsafe.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/ppb_network_list_private_shared.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/thunk/ppb_network_monitor_private_api.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace ppapi {
namespace proxy {

class PPB_NetworkMonitor_Private_Proxy : public InterfaceProxy {
 public:
  explicit PPB_NetworkMonitor_Private_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_NetworkMonitor_Private_Proxy();

  // Creates n NetworkManager object in the plugin process.
  static PP_Resource CreateProxyResource(PP_Instance instance,
                                         PPB_NetworkMonitor_Callback callback,
                                         void* user_data);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  static const ApiID kApiID = API_ID_PPB_NETWORKMANAGER_PRIVATE;

 private:
  class NetworkMonitor;
  friend class NetworkMonitor;

  // IPC message handler for the messages received from the browser.
  void OnPluginMsgNetworkList(uint32 plugin_dispatcher_id,
                              const ppapi::NetworkList& list);

  // Called by NetworkMonitor destructor.
  void OnNetworkMonitorDeleted(NetworkMonitor* monitor,
                               PP_Instance instance);

  // We use ObserverListThreadSafe because we want to send notifications to the
  // same thread that created the NetworkMonitor.
  scoped_refptr<ObserverListThreadSafe<NetworkMonitor> > monitors_;

  int monitors_count_;
  scoped_refptr<NetworkListStorage> current_list_;

  DISALLOW_COPY_AND_ASSIGN(PPB_NetworkMonitor_Private_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_NETWORK_MONITOR_PRIVATE_PROXY_H_
