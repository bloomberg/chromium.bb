// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/battery/battery_dispatcher_proxy.h"

#include "platform/battery/battery_status.h"
#include "platform/threading/BindForMojo.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"

namespace blink {

BatteryDispatcherProxy::BatteryDispatcherProxy(Listener* listener)
    : listener_(listener) {
  ASSERT(listener_);
}

BatteryDispatcherProxy::~BatteryDispatcherProxy() {}

void BatteryDispatcherProxy::StartListening() {
  ASSERT(!monitor_.is_bound());
  Platform::current()->connectToRemoteService(mojo::GetProxy(&monitor_));
  // monitor_ can be null during testing.
  if (monitor_)
    QueryNextStatus();
}

void BatteryDispatcherProxy::StopListening() {
  // monitor_ can be null during testing.
  if (monitor_)
    monitor_.reset();
}

void BatteryDispatcherProxy::QueryNextStatus() {
  monitor_->QueryNextStatus(
      sameThreadBindForMojo(&BatteryDispatcherProxy::OnDidChange, this));
}

void BatteryDispatcherProxy::OnDidChange(
    device::BatteryStatusPtr battery_status) {
  // monitor_ can be null during testing.
  if (monitor_)
    QueryNextStatus();

  DCHECK(battery_status);

  BatteryStatus status(battery_status->charging,
                       battery_status->charging_time,
                       battery_status->discharging_time,
                       battery_status->level);
  listener_->OnUpdateBatteryStatus(status);
}

} // namespace blink
