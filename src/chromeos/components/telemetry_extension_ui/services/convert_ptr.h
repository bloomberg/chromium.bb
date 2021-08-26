// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_SERVICES_CONVERT_PTR_H_
#define CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_SERVICES_CONVERT_PTR_H_

#include <utility>

#include "chromeos/components/telemetry_extension_ui/services/diagnostics_service_converters.h"
#include "chromeos/components/telemetry_extension_ui/services/probe_service_converters.h"

// To use ConvertPtr with other functions, headers to function definitions
// must be included in this file.

namespace chromeos {
namespace converters {

template <class InputT>
auto ConvertPtr(InputT input) {
  return (!input.is_null()) ? unchecked::UncheckedConvertPtr(std::move(input))
                            : nullptr;
}

}  // namespace converters
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_SERVICES_CONVERT_PTR_H_
