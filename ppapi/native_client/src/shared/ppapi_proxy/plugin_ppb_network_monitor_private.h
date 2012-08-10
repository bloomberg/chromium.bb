// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_NETWORK_MONITOR_PRIVATE_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_NETWORK_MONITOR_PRIVATE_H_

#include <pthread.h>
#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/ref_counted.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/private/ppb_network_monitor_private.h"

namespace ppapi_proxy {

class PluginNetworkMonitorPrivate : public PluginResource {
 public:
  PluginNetworkMonitorPrivate();

  void set_callback(PPB_NetworkMonitor_Callback network_list_callback,
                    void* user_data) {
    network_list_callback_ = network_list_callback;
    user_data_ = user_data;
  }

  void OnNetworkListChanged(PP_Resource list_resource);

  static const PPB_NetworkMonitor_Private* GetInterface();
  virtual bool InitFromBrowserResource(PP_Resource resource);

 protected:
  virtual ~PluginNetworkMonitorPrivate();

 private:
  PP_Resource resource_;
  PPB_NetworkMonitor_Callback network_list_callback_;
  void* user_data_;

  IMPLEMENT_RESOURCE(PluginNetworkMonitorPrivate);
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginNetworkMonitorPrivate);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_PPB_NETWORK_MONITOR_PRIVATE_H_AUDIO_H_
