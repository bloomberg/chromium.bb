// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRACING_BACKGROUND_TRACING_METRICS_PROVIDER_H_
#define CHROME_BROWSER_TRACING_BACKGROUND_TRACING_METRICS_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/metrics/metrics_provider.h"

namespace tracing {

// Provides trace log metrics collected using BackgroundTracingManager to UMA
// proto. Background tracing uploads metrics of larger size compared to UMA
// histograms and it is better to upload them as independent metrics rather
// than part of UMA histograms log. Uploading as independent logs is useful to
// track upload sizes, and also to make sure the UMA metrics are not discarded
// from saving to disk due to large size of the logs. The background tracing
// manager will make sure traces are only uploaded on WiFi, or the traces are
// small when uploading over data, to make sure weekly upload quota for UMA
// metrics is not affected on Android.
class BackgroundTracingMetricsProvider : public metrics::MetricsProvider {
 public:
  BackgroundTracingMetricsProvider();

  BackgroundTracingMetricsProvider(const BackgroundTracingMetricsProvider&) =
      delete;
  BackgroundTracingMetricsProvider& operator=(
      const BackgroundTracingMetricsProvider&) = delete;

  ~BackgroundTracingMetricsProvider() override;

  // metrics::MetricsProvider:
  void Init() override;
#if defined(OS_WIN)
  void AsyncInit(base::OnceClosure done_callback) override;
#endif  // defined(OS_WIN)
  bool HasIndependentMetrics() override;
  void ProvideIndependentMetrics(
      base::OnceCallback<void(bool)> done_callback,
      metrics::ChromeUserMetricsExtension* uma_proto,
      base::HistogramSnapshotManager* snapshot_manager) override;

 private:
  std::vector<std::unique_ptr<metrics::MetricsProvider>>
      system_profile_providers_;
#if defined(OS_WIN)
  // owned by |system_profile_providers_|.
  raw_ptr<MetricsProvider> av_metrics_provider_ = nullptr;
#endif
};

}  // namespace tracing

#endif  // CHROME_BROWSER_TRACING_BACKGROUND_TRACING_METRICS_PROVIDER_H_
