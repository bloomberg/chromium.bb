// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CPU_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_CPU_METRICS_PROVIDER_H_

#include "base/macros.h"
#include "components/metrics/metrics_provider.h"

namespace metrics {

// CPUMetricsProvider adds CPU Info in the system profile. These include
// CPU vendor information, cpu cores, etc. This doesn't provide CPU usage
// information.
class CPUMetricsProvider : public MetricsProvider {
 public:
  CPUMetricsProvider();
  ~CPUMetricsProvider() override;

  void ProvideSystemProfileMetrics(
      SystemProfileProto* system_profile_proto) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CPUMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CPU_METRICS_PROVIDER_H_
