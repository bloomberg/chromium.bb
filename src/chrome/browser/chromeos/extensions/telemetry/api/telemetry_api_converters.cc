// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/telemetry_api_converters.h"

#include <inttypes.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/webui/telemetry_extension_ui/mojom/probe_service.mojom.h"
#include "base/notreached.h"
#include "chrome/common/chromeos/extensions/api/telemetry.h"

namespace chromeos {
namespace converters {

namespace {

namespace telemetry_api = ::chromeos::api::os_telemetry;
namespace telemetry_service = ::ash::health::mojom;

}  // namespace

namespace unchecked {

telemetry_api::CpuCStateInfo UncheckedConvertPtr(
    telemetry_service::CpuCStateInfoPtr input) {
  telemetry_api::CpuCStateInfo result;
  if (input->name.has_value()) {
    result.name = std::make_unique<std::string>(input->name.value());
  }
  if (input->time_in_state_since_last_boot_us) {
    result.time_in_state_since_last_boot_us = std::make_unique<double_t>(
        input->time_in_state_since_last_boot_us->value);
  }
  return result;
}

telemetry_api::LogicalCpuInfo UncheckedConvertPtr(
    telemetry_service::LogicalCpuInfoPtr input) {
  telemetry_api::LogicalCpuInfo result;
  if (input->max_clock_speed_khz) {
    result.max_clock_speed_khz =
        std::make_unique<int32_t>(input->max_clock_speed_khz->value);
  }
  if (input->scaling_max_frequency_khz) {
    result.scaling_max_frequency_khz =
        std::make_unique<int32_t>(input->scaling_max_frequency_khz->value);
  }
  if (input->scaling_current_frequency_khz) {
    result.scaling_current_frequency_khz =
        std::make_unique<int32_t>(input->scaling_current_frequency_khz->value);
  }
  if (input->idle_time_ms) {
    result.idle_time_ms =
        std::make_unique<double_t>(input->idle_time_ms->value);
  }
  result.c_states = ConvertPtrVector<telemetry_api::CpuCStateInfo>(
      std::move(input->c_states));
  return result;
}

telemetry_api::PhysicalCpuInfo UncheckedConvertPtr(
    telemetry_service::PhysicalCpuInfoPtr input) {
  telemetry_api::PhysicalCpuInfo result;
  if (input->model_name.has_value()) {
    result.model_name =
        std::make_unique<std::string>(input->model_name.value());
  }
  result.logical_cpus = ConvertPtrVector<telemetry_api::LogicalCpuInfo>(
      std::move(input->logical_cpus));
  return result;
}

}  // namespace unchecked

telemetry_api::CpuArchitectureEnum Convert(
    telemetry_service::CpuArchitectureEnum input) {
  switch (input) {
    case telemetry_service::CpuArchitectureEnum::kUnknown:
      return telemetry_api::CpuArchitectureEnum::CPU_ARCHITECTURE_ENUM_UNKNOWN;
    case telemetry_service::CpuArchitectureEnum::kX86_64:
      return telemetry_api::CpuArchitectureEnum::CPU_ARCHITECTURE_ENUM_X86_64;
    case telemetry_service::CpuArchitectureEnum::kAArch64:
      return telemetry_api::CpuArchitectureEnum::CPU_ARCHITECTURE_ENUM_AARCH64;
    case telemetry_service::CpuArchitectureEnum::kArmv7l:
      return telemetry_api::CpuArchitectureEnum::CPU_ARCHITECTURE_ENUM_ARMV7L;
  }
  NOTREACHED();
}

}  // namespace converters
}  // namespace chromeos
