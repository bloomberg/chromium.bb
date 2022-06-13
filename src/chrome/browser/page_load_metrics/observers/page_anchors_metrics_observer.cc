// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/page_anchors_metrics_observer.h"

#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_builders.h"

PageAnchorsMetricsObserver::AnchorsData::AnchorsData(
    content::WebContents* contents)
    : content::WebContentsUserData<AnchorsData>(*contents) {}

PageAnchorsMetricsObserver::AnchorsData::~AnchorsData() = default;

int PageAnchorsMetricsObserver::AnchorsData::MedianLinkLocation() {
  DCHECK(!link_locations_.empty());
  sort(link_locations_.begin(), link_locations_.end());
  size_t idx = link_locations_.size() / 2;
  if (link_locations_.size() % 2 == 0) {
    return (link_locations_[idx - 1] + link_locations_[idx]) * 50;
  }
  return link_locations_[link_locations_.size() / 2] * 100;
}

void PageAnchorsMetricsObserver::AnchorsData::Clear() {
  number_of_anchors_same_host_ = 0;
  number_of_anchors_contains_image_ = 0;
  number_of_anchors_in_iframe_ = 0;
  number_of_anchors_url_incremented_ = 0;
  number_of_anchors_ = 0;
  total_clickable_space_ = 0;
  viewport_height_ = 0;
  viewport_width_ = 0;
  link_locations_.clear();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PageAnchorsMetricsObserver::AnchorsData);

void PageAnchorsMetricsObserver::RecordUkm() {
  PageAnchorsMetricsObserver::AnchorsData* data =
      PageAnchorsMetricsObserver::AnchorsData::FromWebContents(web_contents_);
  if (!data || data->number_of_anchors_ == 0) {
    // NavigationPredictor did not record any anchor data, don't log anything.
    return;
  }
  ukm::builders::NavigationPredictorPageLinkMetrics builder(
      GetDelegate().GetPageUkmSourceId());
  builder.SetMedianLinkLocation(data->MedianLinkLocation());
  builder.SetNumberOfAnchors_ContainsImage(
      data->number_of_anchors_contains_image_);
  builder.SetNumberOfAnchors_InIframe(data->number_of_anchors_in_iframe_);
  builder.SetNumberOfAnchors_SameHost(data->number_of_anchors_same_host_);
  builder.SetNumberOfAnchors_Total(data->number_of_anchors_);
  builder.SetNumberOfAnchors_URLIncremented(
      data->number_of_anchors_url_incremented_);
  builder.SetTotalClickableSpace(data->total_clickable_space_);
  builder.SetViewport_Height(data->viewport_height_);
  builder.SetViewport_Width(data->viewport_width_);
  builder.Record(ukm::UkmRecorder::Get());
  // Clear the AnchorsData for the next page load.
  data->Clear();
}

void PageAnchorsMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming&) {
  RecordUkm();
}
page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PageAnchorsMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming&) {
  RecordUkm();
  return STOP_OBSERVING;
}
