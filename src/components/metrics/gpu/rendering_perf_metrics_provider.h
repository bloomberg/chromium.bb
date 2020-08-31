// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_GPU_RENDERING_PERF_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_GPU_RENDERING_PERF_METRICS_PROVIDER_H_

#include "base/macros.h"
#include "components/metrics/metrics_provider.h"

namespace metrics {

// RenderingPerfMetricsProvider provides metrics related to rendering
// performance.
class RenderingPerfMetricsProvider : public MetricsProvider {
 public:
  RenderingPerfMetricsProvider();
  ~RenderingPerfMetricsProvider() override;

  // MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderingPerfMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_GPU_RENDERING_PERF_METRICS_PROVIDER_H_
