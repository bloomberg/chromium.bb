// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/tracing_features.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "components/tracing/common/tracing_switches.h"

namespace features {

// Enables the perfetto tracing backend. For startup tracing, pass the
// --enable-perfetto flag instead.
const base::Feature kTracingPerfettoBackend{"TracingPerfettoBackend",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

namespace tracing {

bool TracingUsesPerfettoBackend() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnablePerfetto) ||
         base::FeatureList::IsEnabled(features::kTracingPerfettoBackend);
}

}  // namespace tracing
