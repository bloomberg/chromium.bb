// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_TELEMETRY_LOG_H_
#define CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_TELEMETRY_LOG_H_

#include <string>

#include "chromeos/components/diagnostics_ui/mojom/system_data_provider.mojom.h"

namespace chromeos {
namespace diagnostics {

class TelemetryLog {
 public:
  TelemetryLog();
  ~TelemetryLog();

  TelemetryLog(const TelemetryLog&) = delete;
  TelemetryLog& operator=(const TelemetryLog&) = delete;

  void UpdateSystemInfo(mojom::SystemInfoPtr latest_system_info);
  void UpdateBatteryChargeStatus(
      mojom::BatteryChargeStatusPtr latest_battery_charge_status);
  void UpdateBatteryHealth(mojom::BatteryHealthPtr latest_battery_health);
  void UpdateMemoryUsage(mojom::MemoryUsagePtr latest_memory_usage);
  void UpdateCpuUsage(mojom::CpuUsagePtr latest_cpu_usage);

  // Returns the telemetry log as a string.
  std::string GetContents() const;

 private:
  mojom::SystemInfoPtr latest_system_info_;
  mojom::BatteryChargeStatusPtr latest_battery_charge_status_;
  mojom::BatteryHealthPtr latest_battery_health_;
  mojom::MemoryUsagePtr latest_memory_usage_;
  mojom::CpuUsagePtr latest_cpu_usage_;
};

}  // namespace diagnostics
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_TELEMETRY_LOG_H_
