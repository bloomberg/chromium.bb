// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/layout_page_load_metrics_observer.h"

#include "base/metrics/histogram_functions.h"

namespace page_load_metrics {

namespace {

void Record(const PageRenderData& data) {
  if (data.all_layout_block_count > 0) {
    base::UmaHistogramPercentage(
        "Blink.Layout.NGRatio.Blocks",
        data.ng_layout_block_count * 100 / data.all_layout_block_count);
  }
  if (data.all_layout_call_count) {
    base::UmaHistogramPercentage(
        "Blink.Layout.NGRatio.Calls",
        data.ng_layout_call_count * 100 / data.all_layout_call_count);
  }
}

}  // namespace

LayoutPageLoadMetricsObserver::LayoutPageLoadMetricsObserver() = default;

LayoutPageLoadMetricsObserver::~LayoutPageLoadMetricsObserver() = default;

void LayoutPageLoadMetricsObserver::OnComplete(const mojom::PageLoadTiming&) {
  Record(GetDelegate().GetPageRenderData());
}

PageLoadMetricsObserver::ObservePolicy
LayoutPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const mojom::PageLoadTiming&) {
  Record(GetDelegate().GetPageRenderData());
  // Record() should be called once per page.
  return STOP_OBSERVING;
}

}  // namespace page_load_metrics
