// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/power_monitor/power_monitor_broadcast_source.h"

#include "base/location.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/device/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

PowerMonitorBroadcastSource::PowerMonitorBroadcastSource(
    service_manager::Connector* connector)
    : last_reported_battery_power_state_(false), binding_(this) {
  if (connector) {
    device::mojom::PowerMonitorPtr power_monitor;
    connector->BindInterface(device::mojom::kServiceName,
                             mojo::MakeRequest(&power_monitor));
    device::mojom::PowerMonitorClientPtr client;
    binding_.Bind(mojo::MakeRequest(&client));
    power_monitor->AddClient(std::move(client));
  }
}

PowerMonitorBroadcastSource::~PowerMonitorBroadcastSource() {}

bool PowerMonitorBroadcastSource::IsOnBatteryPowerImpl() {
  return last_reported_battery_power_state_;
}

void PowerMonitorBroadcastSource::PowerStateChange(bool on_battery_power) {
  last_reported_battery_power_state_ = on_battery_power;
  ProcessPowerEvent(PowerMonitorSource::POWER_STATE_EVENT);
}

void PowerMonitorBroadcastSource::Suspend() {
  ProcessPowerEvent(PowerMonitorSource::SUSPEND_EVENT);
}

void PowerMonitorBroadcastSource::Resume() {
  ProcessPowerEvent(PowerMonitorSource::RESUME_EVENT);
}

}  // namespace device
