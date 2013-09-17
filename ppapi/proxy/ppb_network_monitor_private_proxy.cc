// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_network_monitor_private_proxy.h"

#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/thunk/ppb_network_monitor_api.h"

namespace ppapi {
namespace proxy {

class PPB_NetworkMonitor_Private_Proxy::NetworkMonitor
    : public Resource,
      public thunk::PPB_NetworkMonitor_API,
      public base::SupportsWeakPtr<
          PPB_NetworkMonitor_Private_Proxy::NetworkMonitor> {
 public:
  NetworkMonitor(PP_Instance instance,
                 PPB_NetworkMonitor_Private_Proxy* proxy)
      : Resource(OBJECT_IS_PROXY, instance),
        proxy_(proxy),
        initial_list_sent_(false),
        network_list_(NULL) {
  }

  virtual ~NetworkMonitor() {
    if (TrackedCallback::IsPending(update_callback_))
      update_callback_->PostAbort();
    proxy_->OnNetworkMonitorDeleted(this, pp_instance());
  }

  // thunk::PPB_NetworkMonitor_API interface.
  virtual int32_t UpdateNetworkList(
        PP_Resource* network_list,
        scoped_refptr<TrackedCallback> callback) OVERRIDE {
    if (!network_list)
      return PP_ERROR_BADARGUMENT;
    if (TrackedCallback::IsPending(update_callback_))
      return PP_ERROR_INPROGRESS;

    if (current_list_ && !initial_list_sent_) {
      initial_list_sent_ = true;
      thunk::EnterResourceCreationNoLock enter(pp_instance());
      *network_list = PPB_NetworkList_Private_Shared::Create(
          OBJECT_IS_PROXY, pp_instance(), current_list_);
      return PP_OK;
    }

    network_list_ = network_list;
    update_callback_ = callback;
    return PP_OK_COMPLETIONPENDING;
  }

  // Resource overrides.
  virtual ppapi::thunk::PPB_NetworkMonitor_API*
  AsPPB_NetworkMonitor_API() OVERRIDE {
    return this;
  }

  // This is invoked when a network list is received for this monitor (either
  // initially or on a change).
  void OnNetworkListReceived(const scoped_refptr<NetworkListStorage>& list) {
    current_list_ = list;

    if (TrackedCallback::IsPending(update_callback_)) {
      initial_list_sent_ = true;
      {
        thunk::EnterResourceCreationNoLock enter(pp_instance());
        *network_list_ = PPB_NetworkList_Private_Shared::Create(
            OBJECT_IS_PROXY, pp_instance(), list);
        network_list_ = NULL;
      }
      update_callback_->Run(PP_OK);
    }
  }

 private:
  PPB_NetworkMonitor_Private_Proxy* proxy_;
  scoped_refptr<NetworkListStorage> current_list_;
  bool initial_list_sent_;

  // Parameters passed to UpdateNetworkList();
  PP_Resource* network_list_;
  scoped_refptr<TrackedCallback> update_callback_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMonitor);
};

PPB_NetworkMonitor_Private_Proxy::PPB_NetworkMonitor_Private_Proxy(
    Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      monitors_(ObserverList<NetworkMonitor>::NOTIFY_EXISTING_ONLY) {
}

PPB_NetworkMonitor_Private_Proxy::~PPB_NetworkMonitor_Private_Proxy() {
  DCHECK(!monitors_.might_have_observers());
}

// static
PP_Resource PPB_NetworkMonitor_Private_Proxy::CreateProxyResource(
    PP_Instance instance) {
  // TODO(dmichael): Check that this thread has a valid message loop associated
  //                 with it.
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;
  PPB_NetworkMonitor_Private_Proxy* proxy =
      static_cast<PPB_NetworkMonitor_Private_Proxy*>(
          dispatcher->GetInterfaceProxy(kApiID));
  if (!proxy)
    return 0;

  scoped_refptr<NetworkMonitor> result(new NetworkMonitor(instance, proxy));

  bool first_network_monitor = !proxy->monitors_.might_have_observers();
  proxy->monitors_.AddObserver(result.get());
  if (first_network_monitor) {
    // If that is the first network monitor then send Start message.
    PluginGlobals::Get()->GetBrowserSender()->Send(
        new PpapiHostMsg_PPBNetworkMonitor_Start(
            dispatcher->plugin_dispatcher_id()));

    // We could have received network list message after sending the
    // previous Stop message. This list is stale now, so reset it
    // here.
    proxy->current_list_ = NULL;
  } else if (proxy->current_list_.get()) {
    result->OnNetworkListReceived(proxy->current_list_);
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
  FOR_EACH_OBSERVER(NetworkMonitor, monitors_,
                    OnNetworkListReceived(list_storage));
}

void PPB_NetworkMonitor_Private_Proxy::OnNetworkMonitorDeleted(
    NetworkMonitor* monitor,
    PP_Instance instance) {
  monitors_.RemoveObserver(monitor);
  if (!monitors_.might_have_observers()) {
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
