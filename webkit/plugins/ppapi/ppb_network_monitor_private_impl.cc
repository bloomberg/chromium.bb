// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_network_monitor_private_impl.h"

#include "base/bind.h"
#include "ppapi/shared_impl/ppb_network_list_private_shared.h"
#include "net/base/net_util.h"
#include "webkit/plugins/ppapi/resource_helper.h"

namespace webkit {
namespace ppapi {

PPB_NetworkMonitor_Private_Impl::PPB_NetworkMonitor_Private_Impl(
    PP_Instance instance,
    PPB_NetworkMonitor_Callback callback,
    void* user_data)
    : Resource(::ppapi::OBJECT_IS_IMPL, instance),
      callback_(callback),
      user_data_(user_data),
      started_(false) {
}

PPB_NetworkMonitor_Private_Impl::~PPB_NetworkMonitor_Private_Impl() {
  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (plugin_delegate && started_) {
    plugin_delegate->RemoveNetworkListObserver(this);
  }
}

// static
PP_Resource PPB_NetworkMonitor_Private_Impl::Create(
    PP_Instance instance,
    PPB_NetworkMonitor_Callback callback,
    void* user_data) {
  scoped_refptr<PPB_NetworkMonitor_Private_Impl> result(
      new PPB_NetworkMonitor_Private_Impl(instance, callback, user_data));
  if (!result->Start())
    return 0;
  return result->GetReference();
}

::ppapi::thunk::PPB_NetworkMonitor_Private_API*
PPB_NetworkMonitor_Private_Impl::AsPPB_NetworkMonitor_Private_API() {
  return this;
}

bool PPB_NetworkMonitor_Private_Impl::Start() {
  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return false;
  started_ = plugin_delegate->AddNetworkListObserver(this);
  return started_;
}

void PPB_NetworkMonitor_Private_Impl::OnNetworkListChanged(
    const net::NetworkInterfaceList& list) {
  scoped_ptr< ::ppapi::PPB_NetworkList_Private_Shared::NetworkList> list_copy(
      new ::ppapi::PPB_NetworkList_Private_Shared::NetworkList(list.size()));
  for (size_t i = 0; i < list.size(); ++i) {
    ::ppapi::PPB_NetworkList_Private_Shared::NetworkInfo& network =
        list_copy->at(i);
    network.name = list[i].name;

    network.addresses.resize(1);
    CHECK_LE(list[i].address.size(), sizeof(network.addresses[0].data));
    network.addresses[0].size = list[i].address.size();
    memcpy(network.addresses[0].data, &(list[i].address.front()),
           list[i].address.size());

    // TODO(sergeyu): Currently net::NetworkInterfaceList provides
    // only name and one IP address. Add all other fields and copy
    // them here.
    network.type = PP_NETWORKLIST_UNKNOWN;
    network.state = PP_NETWORKLIST_UP;
    network.display_name = list[i].name;
    network.mtu = 0;
  }
  PP_Resource list_resource =
      ::ppapi::PPB_NetworkList_Private_Shared::Create(
          ::ppapi::OBJECT_IS_IMPL, pp_instance(), list_copy.Pass());
  callback_(user_data_, list_resource);
}

}  // namespace ppapi
}  // namespace webkit
