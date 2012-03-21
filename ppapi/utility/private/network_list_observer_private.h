// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_UTILITY_PRIVATE_NETWORK_LIST_OBSERVER_H_
#define PPAPI_UTILITY_PRIVATE_NETWORK_LIST_OBSERVER_H_

#include "ppapi/cpp/private/network_monitor_private.h"

namespace pp {

class NetworkListPrivate;

/// <code>NetworkListObserver</code> is a wrapper for
/// <code>pp::NetworkMonitorPrivate</code> that makes it easier to
/// handle network list update notifications. A child class must
/// implement the <code>OnNetworkListChanged()</code> method. That
/// method will be called once after the object is created and then
/// every time network configuration changes.
class NetworkListObserverPrivate {
 public:
  explicit NetworkListObserverPrivate(const InstanceHandle& instance);
  virtual ~NetworkListObserverPrivate();

 protected:
  /// Called once after this object is created and later every time
  /// network configuration changes. Child classes must implement this
  /// method.
  ///
  /// @param[in] list The current list of network interfaces.
  virtual void OnNetworkListChanged(const NetworkListPrivate& list) = 0;

 private:
  static void NetworkListCallbackHandler(void* user_data,
                                         PP_Resource list_resource);

  NetworkMonitorPrivate monitor_;
};

}  // namespace pp

#endif  // PPAPI_UTILITY_PRIVATE_NETWORK_LIST_OBSERVER_H_
