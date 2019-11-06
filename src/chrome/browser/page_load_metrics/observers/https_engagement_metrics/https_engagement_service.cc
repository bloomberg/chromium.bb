// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_service.h"

#include "base/metrics/histogram_macros.h"

namespace internal {
const char kHttpsEngagementSessionPercentage[] =
    "Navigation.EngagementTime.Ratio";
}

HttpsEngagementService::HttpsEngagementService() {}

HttpsEngagementService::~HttpsEngagementService() {}

void HttpsEngagementService::RecordTimeOnPage(base::TimeDelta foreground_time,
                                              PageScheme scheme) {
  switch (scheme) {
    case HTTPS:
      time_on_https_ += foreground_time;
      break;
    case HTTP:
      time_on_http_ += foreground_time;
      break;
    case OTHER:
      return;
  }
}

void HttpsEngagementService::StoreMetricsAndClear() {
  double total_time =
      time_on_https_.InMillisecondsF() + time_on_http_.InMillisecondsF();
  if (total_time == 0)
    return;

  double https_ratio = 100.0 * (time_on_https_.InMillisecondsF() / total_time);
  UMA_HISTOGRAM_PERCENTAGE(internal::kHttpsEngagementSessionPercentage,
                           https_ratio);

  time_on_https_ = base::TimeDelta();
  time_on_http_ = base::TimeDelta();
}
