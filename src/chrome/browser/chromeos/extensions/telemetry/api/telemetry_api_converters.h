// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_API_CONVERTERS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_API_CONVERTERS_H_

#include <utility>
#include <vector>

#include "ash/webui/telemetry_extension_ui/mojom/probe_service.mojom.h"
#include "base/check.h"
#include "chrome/common/chromeos/extensions/api/telemetry.h"

namespace chromeos {

namespace converters {

// This file contains helper functions used by telemetry_api.cc to convert its
// types to/from telemetry service types.

namespace unchecked {

// Functions in unchecked namespace do not verify whether input pointer is
// nullptr, they should be called only via ConvertPtr wrapper that checks
// whether input pointer is nullptr.

chromeos::api::os_telemetry::CpuCStateInfo UncheckedConvertPtr(
    ash::health::mojom::CpuCStateInfoPtr input);

chromeos::api::os_telemetry::LogicalCpuInfo UncheckedConvertPtr(
    ash::health::mojom::LogicalCpuInfoPtr input);

chromeos::api::os_telemetry::PhysicalCpuInfo UncheckedConvertPtr(
    ash::health::mojom::PhysicalCpuInfoPtr input);

}  // namespace unchecked

chromeos::api::os_telemetry::CpuArchitectureEnum Convert(
    ash::health::mojom::CpuArchitectureEnum input);

template <class OutputT, class InputT>
std::vector<OutputT> ConvertPtrVector(std::vector<InputT> input) {
  std::vector<OutputT> output;
  for (auto&& element : input) {
    DCHECK(!element.is_null());
    output.push_back(unchecked::UncheckedConvertPtr(std::move(element)));
  }
  return output;
}

template <class OutputT, class InputT>
OutputT ConvertPtr(InputT input) {
  return (!input.is_null()) ? unchecked::UncheckedConvertPtr(std::move(input))
                            : OutputT();
}

}  // namespace converters
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_API_CONVERTERS_H_
