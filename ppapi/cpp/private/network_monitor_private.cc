// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/network_monitor_private.h"

#include "ppapi/c/private/ppb_network_monitor_private.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/private/network_list_private.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_NetworkMonitor_Private_0_3>() {
  return PPB_NETWORKMONITOR_PRIVATE_INTERFACE_0_3;
}

}  // namespace

NetworkMonitorPrivate::NetworkMonitorPrivate(const InstanceHandle& instance) {
  if (has_interface<PPB_NetworkMonitor_Private_0_3>()) {
    PassRefFromConstructor(get_interface<PPB_NetworkMonitor_Private_0_3>()->
        Create(instance.pp_instance()));
  }
}

int32_t NetworkMonitorPrivate::UpdateNetworkList(
    const CompletionCallbackWithOutput<NetworkListPrivate>& callback) {
  if (has_interface<PPB_NetworkMonitor_Private_0_3>()) {
    return get_interface<PPB_NetworkMonitor_Private_0_3>()->UpdateNetworkList(
        pp_resource(), callback.output(), callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

// static
bool NetworkMonitorPrivate::IsAvailable() {
  return has_interface<PPB_NetworkMonitor_Private_0_3>();
}

}  // namespace pp
