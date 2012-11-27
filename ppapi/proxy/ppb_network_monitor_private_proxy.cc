// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_network_monitor_private_proxy.h"

#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/thunk/ppb_network_monitor_private_api.h"

namespace ppapi {
namespace proxy {

class PPB_NetworkMonitor_Private_Proxy::NetworkMonitor
    : public Resource,
      public thunk::PPB_NetworkMonitor_Private_API,
      public base::SupportsWeakPtr<
          PPB_NetworkMonitor_Private_Proxy::NetworkMonitor> {
 public:
  NetworkMonitor(PP_Instance instance,
                 PPB_NetworkMonitor_Private_Proxy* proxy,
                 PPB_NetworkMonitor_Callback callback,
                 void* user_data)
      : Resource(OBJECT_IS_PROXY, instance),
        proxy_(proxy),
        callback_(callback),
        user_data_(user_data) {
  }

  virtual ~NetworkMonitor() {
    proxy_->OnNetworkMonitorDeleted(this, pp_instance());
  }


  // Resource overrides.
  virtual ppapi::thunk::PPB_NetworkMonitor_Private_API*
  AsPPB_NetworkMonitor_Private_API() OVERRIDE {
    return this;
  }

  // This is invoked when a network list is received for this monitor (either
  // initially or on a change). It acquires the ProxyLock inside because
  // ObserverListThreadSafe does not support Bind/Closure, otherwise we would
  // wrap the call with a lock using RunWhileLocked.
  void OnNetworkListReceivedLocks(
      const scoped_refptr<NetworkListStorage>& list) {
    ProxyAutoLock lock;
    PP_Resource list_resource =
        PPB_NetworkList_Private_Shared::Create(
            OBJECT_IS_PROXY, pp_instance(), list);
    CallWhileUnlocked(callback_, user_data_, list_resource);
  }

 private:
  PPB_NetworkMonitor_Private_Proxy* proxy_;
  PPB_NetworkMonitor_Callback callback_;
  void* user_data_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMonitor);
};

PPB_NetworkMonitor_Private_Proxy::PPB_NetworkMonitor_Private_Proxy(
    Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      monitors_(new ObserverListThreadSafe<NetworkMonitor>()),
      monitors_count_(0) {
}

PPB_NetworkMonitor_Private_Proxy::~PPB_NetworkMonitor_Private_Proxy() {
  monitors_->AssertEmpty();
}

// static
PP_Resource PPB_NetworkMonitor_Private_Proxy::CreateProxyResource(
    PP_Instance instance,
    PPB_NetworkMonitor_Callback callback,
    void* user_data) {
  // TODO(dmichael): Check that this thread has a valid message loop associated
  //                 with it.
  if (!callback)
    return 0;

  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;
  PPB_NetworkMonitor_Private_Proxy* proxy =
      static_cast<PPB_NetworkMonitor_Private_Proxy*>(
          dispatcher->GetInterfaceProxy(kApiID));
  if (!proxy)
    return 0;

  scoped_refptr<NetworkMonitor> result(
      new NetworkMonitor(instance, proxy, callback, user_data));
  proxy->monitors_->AddObserver(result.get());

  proxy->monitors_count_++;
  if (proxy->monitors_count_ == 1) {
    // If that is the first network monitor then send Start message.
    PluginGlobals::Get()->GetBrowserSender()->Send(
        new PpapiHostMsg_PPBNetworkMonitor_Start(
            dispatcher->plugin_dispatcher_id()));

    // We could have received network list message after sending the
    // previous Stop message. This list is stale now, so reset it
    // here.
    proxy->current_list_ = NULL;
  } else if (proxy->current_list_.get()) {
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        &NetworkMonitor::OnNetworkListReceivedLocks,
        result->AsWeakPtr(), proxy->current_list_));
  }

  return result->GetReference();
}

bool PPB_NetworkMonitor_Private_Proxy::OnMessageReceived(
    const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_NetworkMonitor_Private_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBNetworkMonitor_NetworkList,
                        OnPluginMsgNetworkList)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_NetworkMonitor_Private_Proxy::OnPluginMsgNetworkList(
    uint32 plugin_dispatcher_id,
    const ppapi::NetworkList& list) {
  scoped_refptr<NetworkListStorage> list_storage(new NetworkListStorage(list));
  current_list_ = list_storage;
  monitors_->Notify(&NetworkMonitor::OnNetworkListReceivedLocks, list_storage);
}

void PPB_NetworkMonitor_Private_Proxy::OnNetworkMonitorDeleted(
    NetworkMonitor* monitor,
    PP_Instance instance) {
  monitors_->RemoveObserver(monitor);
  monitors_count_--;
  if (monitors_count_ == 0) {
    // Send Stop message if that was the last NetworkMonitor.
    PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
    if (dispatcher) {
      PluginGlobals::Get()->GetBrowserSender()->Send(
          new PpapiHostMsg_PPBNetworkMonitor_Stop(
              dispatcher->plugin_dispatcher_id()));
    }
    current_list_ = NULL;
  }
}

}  // namespace proxy
}  // namespace ppapi
