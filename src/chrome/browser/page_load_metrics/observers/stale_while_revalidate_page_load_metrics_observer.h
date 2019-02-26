// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_STALE_WHILE_REVALIDATE_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_STALE_WHILE_REVALIDATE_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"

namespace internal {

// Expose metrics for tests.
extern const char kHistogramSWRFirstContentfulPaintNetwork[];
extern const char kHistogramSWRParseToFirstContentfulPaintNetwork[];
extern const char kHistogramSWRFirstContentfulPaintStaleCache[];
extern const char kHistogramSWRParseToFirstContentfulPaintStaleCache[];
extern const char kHistogramSWRFirstContentfulPaintCache[];
extern const char kHistogramSWRParseToFirstContentfulPaintCache[];

}  // namespace internal

class StaleWhileRevalidatePageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  StaleWhileRevalidatePageLoadMetricsObserver() = default;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(StaleWhileRevalidatePageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_STALE_WHILE_REVALIDATE_PAGE_LOAD_METRICS_OBSERVER_H_
