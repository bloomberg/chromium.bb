// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/network_monitor_private.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_NetworkMonitor_Private>() {
  return PPB_NETWORKMONITOR_PRIVATE_INTERFACE;
}

}  // namespace

NetworkMonitorPrivate::NetworkMonitorPrivate(
    const InstanceHandle& instance,
    PPB_NetworkMonitor_Callback callback,
    void* user_data) {
  if (has_interface<PPB_NetworkMonitor_Private>()) {
    PassRefFromConstructor(get_interface<PPB_NetworkMonitor_Private>()->Create(
        instance.pp_instance(), callback, user_data));
  }
}

// static
bool NetworkMonitorPrivate::IsAvailable() {
  return has_interface<PPB_NetworkMonitor_Private>();
}

}  // namespace pp
