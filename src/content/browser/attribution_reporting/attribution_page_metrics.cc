// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_page_metrics.h"

#include <utility>

#include "base/metrics/histogram_functions.h"

namespace content {

namespace {
constexpr size_t kMaxStoredOrigins = 100;
}  // namespace

AttributionPageMetrics::AttributionPageMetrics() = default;

AttributionPageMetrics::~AttributionPageMetrics() {
  // TODO(https://crbug.com/1044099): Consider limiting registrations per page
  // based on this metric.
  base::UmaHistogramExactLinear("Conversions.RegisteredConversionsPerPage",
                                num_conversions_on_current_page_, 100);

  base::UmaHistogramExactLinear("Conversions.RegisteredImpressionsPerPage",
                                num_impressions_on_current_page_, 100);

  if (!conversion_reporting_origins_on_current_page_.empty()) {
    base::UmaHistogramExactLinear(
        "Conversions.UniqueReportingOriginsPerPage.Conversions",
        conversion_reporting_origins_on_current_page_.size(), 100);
  }

  if (!impression_reporting_origins_on_current_page_.empty()) {
    base::UmaHistogramExactLinear(
        "Conversions.UniqueReportingOriginsPerPage.Impressions",
        impression_reporting_origins_on_current_page_.size(), 100);
  }
}

void AttributionPageMetrics::OnConversion(url::Origin reporting_origin) {
  num_conversions_on_current_page_++;
  if (conversion_reporting_origins_on_current_page_.size() <
      kMaxStoredOrigins) {
    conversion_reporting_origins_on_current_page_.insert(
        std::move(reporting_origin));
  }
}

void AttributionPageMetrics::OnImpression(url::Origin reporting_origin) {
  num_impressions_on_current_page_++;
  if (impression_reporting_origins_on_current_page_.size() <
      kMaxStoredOrigins) {
    impression_reporting_origins_on_current_page_.insert(
        std::move(reporting_origin));
  }
}

}  // namespace content
