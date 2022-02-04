// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/histogram_util.h"

#include "base/metrics/histogram_macros.h"

namespace metrics {
namespace structured {

void LogInternalError(StructuredMetricsError error) {
  UMA_HISTOGRAM_ENUMERATION("UMA.StructuredMetrics.InternalError2", error);
}

void LogEventRecordingState(EventRecordingState state) {
  UMA_HISTOGRAM_ENUMERATION("UMA.StructuredMetrics.EventRecordingState", state);
}

void LogNumEventsInUpload(const int num_events) {
  UMA_HISTOGRAM_COUNTS_1000("UMA.StructuredMetrics.NumEventsInUpload",
                            num_events);
}

void LogKeyValidation(KeyValidationState state) {
  UMA_HISTOGRAM_ENUMERATION("UMA.StructuredMetrics.KeyValidationState", state);
}

void LogIsEventRecordedUsingMojo(bool used_mojo_api) {
  UMA_HISTOGRAM_BOOLEAN("UMA.StructuredMetrics.EventsRecordedUsingMojo",
                        used_mojo_api);
}

}  // namespace structured
}  // namespace metrics
