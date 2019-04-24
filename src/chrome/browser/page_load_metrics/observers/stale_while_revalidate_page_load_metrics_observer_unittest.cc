// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/stale_while_revalidate_page_load_metrics_observer.h"

#include <memory>

#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/common/page_load_metrics/test/page_load_metrics_test_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/platform/web_loading_behavior_flag.h"

class StaleWhileRevalidatePageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<StaleWhileRevalidatePageLoadMetricsObserver>());
  }
};

TEST_F(StaleWhileRevalidatePageLoadMetricsObserverTest, TestHistograms) {
  struct TestCase {
    blink::WebLoadingBehaviorFlag flag;
    const char* fcp_histogram;
    const char* parse_to_fcp_histogram;
  };

  TestCase test_cases[] = {
      {blink::WebLoadingBehaviorFlag::
           kStaleWhileRevalidateResourceCandidateNetworkLoad,
       internal::kHistogramSWRFirstContentfulPaintNetwork,
       internal::kHistogramSWRParseToFirstContentfulPaintNetwork},
      {blink::WebLoadingBehaviorFlag::
           kStaleWhileRevalidateResourceCandidateStaleCacheLoad,
       internal::kHistogramSWRFirstContentfulPaintStaleCache,
       internal::kHistogramSWRParseToFirstContentfulPaintStaleCache},
      {blink::WebLoadingBehaviorFlag::
           kStaleWhileRevalidateResourceCandidateCacheLoad,
       internal::kHistogramSWRFirstContentfulPaintCache,
       internal::kHistogramSWRParseToFirstContentfulPaintCache}};
  for (const auto test_case : test_cases) {
    base::TimeDelta parse_start = base::TimeDelta::FromMilliseconds(1);
    base::TimeDelta contentful_paint = base::TimeDelta::FromMilliseconds(3);
    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.navigation_start = base::Time::FromDoubleT(1);
    timing.parse_timing->parse_start = parse_start;
    timing.paint_timing->first_contentful_paint = contentful_paint;
    PopulateRequiredTimingFields(&timing);

    page_load_metrics::mojom::PageLoadMetadata metadata;
    metadata.behavior_flags |= test_case.flag;
    NavigateAndCommit(GURL("https://www.google.com/"));
    SimulateTimingAndMetadataUpdate(timing, metadata);

    histogram_tester().ExpectTotalCount(test_case.fcp_histogram, 1);
    histogram_tester().ExpectBucketCount(test_case.fcp_histogram,
                                         contentful_paint.InMilliseconds(), 1);
    histogram_tester().ExpectTotalCount(test_case.parse_to_fcp_histogram, 1);
    histogram_tester().ExpectBucketCount(
        test_case.parse_to_fcp_histogram,
        contentful_paint.InMilliseconds() - parse_start.InMilliseconds(), 1);
  }
}
