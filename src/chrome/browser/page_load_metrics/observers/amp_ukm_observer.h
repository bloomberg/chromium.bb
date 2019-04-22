// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AMP_UKM_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AMP_UKM_OBSERVER_H_

#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "services/metrics/public/cpp/ukm_source.h"

// If URL-Keyed-Metrics (UKM) is enabled in the system, this is used to
// populate it with AMP-related page-load metrics.
class AmpUkmObserver : public page_load_metrics::PageLoadMetricsObserver {
 public:
  AmpUkmObserver();
  ~AmpUkmObserver() override;

  void OnLoadingBehaviorObserved(
      content::RenderFrameHost* rfh,
      int behavior_flags,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;

 private:
  // Track what we've already logged.
  bool logged_main_frame_ = false;
  bool logged_subframe_ = false;

  DISALLOW_COPY_AND_ASSIGN(AmpUkmObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AMP_UKM_OBSERVER_H_
