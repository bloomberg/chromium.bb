// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_NETWORK_MONITOR_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_NETWORK_MONITOR_PRIVATE_H_

#include "ppapi/c/private/ppb_network_monitor_private.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/instance_handle.h"

namespace pp {

class Instance;

class NetworkMonitorPrivate : public Resource {
 public:
  NetworkMonitorPrivate(const InstanceHandle& instance,
                        PPB_NetworkMonitor_Callback callback,
                        void* user_data);

  // Returns true if the required interface is available.
  static bool IsAvailable();
};

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_NETWORK_MONITOR_PRIVATE_H_
