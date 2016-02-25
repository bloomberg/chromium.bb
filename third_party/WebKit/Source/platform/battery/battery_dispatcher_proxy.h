// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLINK_PLATFORM_BATTERY_BATTERY_DISPATCHER_PROXY_H_
#define BLINK_PLATFORM_BATTERY_BATTERY_DISPATCHER_PROXY_H_

#include "device/battery/battery_monitor.mojom.h"
#include "platform/PlatformExport.h"
#include "wtf/Noncopyable.h"

namespace blink {

class BatteryStatus;

// This class connects a BatteryDispatcherProxy::Listener to the underlying Mojo
// service.  Note that currently the access to the Mojo service is limited in
// platform/.  In future, we'll let classes in core/ and modules/ directly
// communicate with Mojo, and then, there will be no need to use this proxy
// class.
//
// TODO(yukishiino): Remove this class once Mojo supports WTF-types.
class PLATFORM_EXPORT BatteryDispatcherProxy {
  WTF_MAKE_NONCOPYABLE(BatteryDispatcherProxy);
 public:
  class PLATFORM_EXPORT Listener {
   public:
    virtual ~Listener() = default;

    // This method is called when a new battery status is available.
    virtual void OnUpdateBatteryStatus(const BatteryStatus&) = 0;
  };

  explicit BatteryDispatcherProxy(Listener*);
  ~BatteryDispatcherProxy();

  void StartListening();
  void StopListening();

 private:
  void QueryNextStatus();
  void OnDidChange(device::BatteryStatusPtr);

  device::BatteryMonitorPtr monitor_;
  Listener* listener_;

  friend class BatteryDispatcherProxyTest;
};

}  // namespace blink

#endif  // BLINK_PLATFORM_BATTERY_BATTERY_DISPATCHER_PROXY_H_
