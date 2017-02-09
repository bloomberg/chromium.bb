// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_POWER_MONITOR_POWER_MONITOR_MESSAGE_BROADCASTER_H_
#define SERVICES_DEVICE_POWER_MONITOR_POWER_MONITOR_MESSAGE_BROADCASTER_H_

#include "base/macros.h"
#include "base/power_monitor/power_observer.h"
#include "services/device/public/interfaces/power_monitor.mojom.h"

namespace device {

// A class used to monitor the power state change and communicate it to child
// processes via IPC.
class PowerMonitorMessageBroadcaster : public base::PowerObserver,
                                       public device::mojom::PowerMonitor {
 public:
  explicit PowerMonitorMessageBroadcaster();
  ~PowerMonitorMessageBroadcaster() override;

  static void Create(device::mojom::PowerMonitorRequest request);

  // device::mojom::PowerMonitor:
  void SetClient(
      device::mojom::PowerMonitorClientPtr power_monitor_client) override;

  // base::PowerObserver:
  void OnPowerStateChange(bool on_battery_power) override;
  void OnSuspend() override;
  void OnResume() override;

 private:
  device::mojom::PowerMonitorClientPtr power_monitor_client_;

  DISALLOW_COPY_AND_ASSIGN(PowerMonitorMessageBroadcaster);
};

}  // namespace device

#endif  // SERVICES_DEVICE_POWER_MONITOR_POWER_MONITOR_MESSAGE_BROADCASTER_H_
