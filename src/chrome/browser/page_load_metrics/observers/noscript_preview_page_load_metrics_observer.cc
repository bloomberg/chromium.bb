// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/noscript_preview_page_load_metrics_observer.h"

#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/loader/chrome_navigation_data.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/previews/content/previews_content_util.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_user_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace previews {

namespace noscript_preview_names {

const char kNavigationToLoadEvent[] =
    "PageLoad.Clients.NoScriptPreview.DocumentTiming."
    "NavigationToLoadEventFired";
const char kNavigationToFirstContentfulPaint[] =
    "PageLoad.Clients.NoScriptPreview.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kNavigationToFirstMeaningfulPaint[] =
    "PageLoad.Clients.NoScriptPreview.Experimental.PaintTiming."
    "NavigationToFirstMeaningfulPaint";
const char kParseBlockedOnScriptLoad[] =
    "PageLoad.Clients.NoScriptPreview.ParseTiming.ParseBlockedOnScriptLoad";
const char kParseDuration[] =
    "PageLoad.Clients.NoScriptPreview.ParseTiming.ParseDuration";

const char kNumNetworkResources[] =
    "PageLoad.Clients.NoScriptPreview.Experimental.CompletedResources.Network";
const char kNetworkBytes[] =
    "PageLoad.Clients.NoScriptPreview.Experimental.Bytes.Network";

}  // namespace noscript_preview_names

NoScriptPreviewPageLoadMetricsObserver::
    NoScriptPreviewPageLoadMetricsObserver() {}

NoScriptPreviewPageLoadMetricsObserver::
    ~NoScriptPreviewPageLoadMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NoScriptPreviewPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  if (!started_in_foreground)
    return STOP_OBSERVING;

  if (previews::params::IsNoScriptPreviewsEnabled())
    return CONTINUE_OBSERVING;

  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NoScriptPreviewPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  ChromeNavigationData* nav_data = static_cast<ChromeNavigationData*>(
      navigation_handle->GetNavigationData());
  if (!nav_data)
    return STOP_OBSERVING;

  previews::PreviewsType preview_type =
      previews::GetMainFramePreviewsType(nav_data->previews_state());
  if (preview_type != previews::PreviewsType::NOSCRIPT)
    return STOP_OBSERVING;

  data_savings_inflation_percent_ =
      nav_data->previews_user_data()->data_savings_inflation_percent();

  browser_context_ = navigation_handle->GetWebContents()->GetBrowserContext();

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NoScriptPreviewPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  // FlushMetricsOnAppEnterBackground is invoked on Android in cases where the
  // app is about to be backgrounded, as part of the Activity.onPause()
  // flow. After this method is invoked, Chrome may be killed without further
  // notification.
  if (info.did_commit) {
    RecordPageSizeUMA();
    RecordTimingMetrics(timing, info);
  }
  return STOP_OBSERVING;
}

void NoScriptPreviewPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  // TODO(dougarnett): Determine if a different event makes more sense.
  // https://crbug.com/864720
  int64_t inflation_bytes = 0;
  if (data_savings_inflation_percent_ == 0) {
    data_savings_inflation_percent_ =
        previews::params::NoScriptPreviewsInflationPercent();
    inflation_bytes = previews::params::NoScriptPreviewsInflationBytes();
  }

  int64_t total_saved_bytes =
      (total_network_bytes_ * data_savings_inflation_percent_) / 100 +
      inflation_bytes;

  DCHECK(info.url.SchemeIs(url::kHttpsScheme));

  WriteToSavings(info.url, total_saved_bytes);
}

void NoScriptPreviewPageLoadMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  if (extra_request_complete_info.was_cached)
    return;

  num_network_resources_++;
  network_bytes_ += extra_request_complete_info.raw_body_bytes;
}

void NoScriptPreviewPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordPageSizeUMA();
  RecordTimingMetrics(timing, info);
}

void NoScriptPreviewPageLoadMetricsObserver::RecordPageSizeUMA() const {
  PAGE_RESOURCE_COUNT_HISTOGRAM(noscript_preview_names::kNumNetworkResources,
                                num_network_resources_);
  PAGE_BYTES_HISTOGRAM(noscript_preview_names::kNetworkBytes, network_bytes_);
}

void NoScriptPreviewPageLoadMetricsObserver::RecordTimingMetrics(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->load_event_start, info)) {
    PAGE_LOAD_HISTOGRAM(noscript_preview_names::kNavigationToLoadEvent,
                        timing.document_timing->load_event_start.value());
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(
        noscript_preview_names::kNavigationToFirstContentfulPaint,
        timing.paint_timing->first_contentful_paint.value());
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(
        noscript_preview_names::kNavigationToFirstMeaningfulPaint,
        timing.paint_timing->first_meaningful_paint.value());
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_stop, info)) {
    PAGE_LOAD_HISTOGRAM(
        noscript_preview_names::kParseBlockedOnScriptLoad,
        timing.parse_timing->parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(noscript_preview_names::kParseDuration,
                        timing.parse_timing->parse_stop.value() -
                            timing.parse_timing->parse_start.value());
  }
}

void NoScriptPreviewPageLoadMetricsObserver::OnResourceDataUseObserved(
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  for (auto const& resource : resources) {
    total_network_bytes_ += resource->delta_bytes;
  }
}

void NoScriptPreviewPageLoadMetricsObserver::WriteToSavings(
    const GURL& url,
    int64_t bytes_saved) {
  DCHECK(url.SchemeIs(url::kHttpsScheme));

  data_reduction_proxy::DataReductionProxySettings*
      data_reduction_proxy_settings =
          DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
              browser_context_);

  bool data_saver_enabled =
      data_reduction_proxy_settings->IsDataReductionProxyEnabled();

  data_reduction_proxy_settings->data_reduction_proxy_service()
      ->UpdateContentLengths(0, bytes_saved, data_saver_enabled,
                             data_reduction_proxy::HTTPS, "text/html", true,
                             data_use_measurement::DataUseUserData::OTHER, 0);

  data_reduction_proxy_settings->data_reduction_proxy_service()
      ->UpdateDataUseForHost(0, bytes_saved, url.HostNoBrackets());
}

}  // namespace previews
