// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_use_measurement/chrome_data_use_measurement.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/browser_process.h"
#include "components/data_use_measurement/core/data_use_ascriber.h"
#include "components/data_use_measurement/core/url_request_classifier.h"
#include "components/metrics/metrics_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace data_use_measurement {

namespace {
void UpdateMetricsUsagePrefs(int64_t total_bytes,
                             bool is_cellular,
                             bool is_metrics_service_usage) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Some unit tests use IOThread but do not initialize MetricsService. In that
  // case it's fine to skip the update.
  auto* metrics_service = g_browser_process->metrics_service();
  if (metrics_service) {
    metrics_service->UpdateMetricsUsagePrefs(total_bytes, is_cellular,
                                             is_metrics_service_usage);
  }
}

// This function is for forwarding metrics usage pref changes to the metrics
// service on the appropriate thread.
// TODO(gayane): Reduce the frequency of posting tasks from IO to UI thread.
void UpdateMetricsUsagePrefsOnUIThread(int64_t total_bytes,
                                       bool is_cellular,
                                       bool is_metrics_service_usage) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  base::PostTaskWithTraits(
      FROM_HERE, content::BrowserThread::UI,
      base::BindOnce(UpdateMetricsUsagePrefs, total_bytes, is_cellular,
                     is_metrics_service_usage));
}
}  // namespace

ChromeDataUseMeasurement::ChromeDataUseMeasurement(
    std::unique_ptr<URLRequestClassifier> url_request_classifier,
    DataUseAscriber* ascriber)
    : DataUseMeasurement(std::move(url_request_classifier), ascriber) {}

void ChromeDataUseMeasurement::UpdateDataUseToMetricsService(
    int64_t total_bytes,
    bool is_cellular,
    bool is_metrics_service_usage) {
  // Update data use of user traffic and services distinguishing cellular and
  // metrics services data use.
  UpdateMetricsUsagePrefsOnUIThread(total_bytes, is_cellular,
                                    is_metrics_service_usage);
}

}  // namespace data_use_measurement
