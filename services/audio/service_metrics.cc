// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/service_metrics.h"

#include "base/debug/stack_trace.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/clock.h"

namespace audio {

ServiceMetrics::ServiceMetrics(base::Clock* clock)
    : clock_(clock), service_start_(clock_->Now()) {}

ServiceMetrics::~ServiceMetrics() {
  LogHasNoConnectionsDuration();
  UMA_HISTOGRAM_CUSTOM_TIMES("Media.AudioService.Uptime",
                             clock_->Now() - service_start_, base::TimeDelta(),
                             base::TimeDelta::FromDays(7), 50);
}

void ServiceMetrics::HasConnections() {
  has_connections_start_ = clock_->Now();
  LogHasNoConnectionsDuration();
}

void ServiceMetrics::HasNoConnections() {
  has_no_connections_start_ = clock_->Now();
  DCHECK_NE(base::Time(), has_connections_start_);
  UMA_HISTOGRAM_CUSTOM_TIMES("Media.AudioService.HasConnectionsDuration",
                             clock_->Now() - has_connections_start_,
                             base::TimeDelta(), base::TimeDelta::FromDays(7),
                             50);
  has_connections_start_ = base::Time();
}

void ServiceMetrics::LogHasNoConnectionsDuration() {
  // Service shuts down without having accepted any connections in its lifetime
  // or with active connections, meaning there is no "no connection" interval in
  // progress.
  if (has_no_connections_start_ == base::Time())
    return;

  UMA_HISTOGRAM_CUSTOM_TIMES("Media.AudioService.HasNoConnectionsDuration",
                             clock_->Now() - has_no_connections_start_,
                             base::TimeDelta(),
                             base::TimeDelta::FromMinutes(10), 50);
  has_no_connections_start_ = base::Time();
}

}  // namespace audio
