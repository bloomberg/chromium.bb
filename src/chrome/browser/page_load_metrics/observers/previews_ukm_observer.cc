// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/previews_ukm_observer.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/browser/previews/previews_content_util.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/previews_state.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#endif

namespace previews {

namespace {

const char kOfflinePreviewsMimeType[] = "multipart/related";

}  // namespace

PreviewsUKMObserver::PreviewsUKMObserver()
    : committed_preview_(PreviewsType::NONE) {}

PreviewsUKMObserver::~PreviewsUKMObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewsUKMObserver::OnCommit(content::NavigationHandle* navigation_handle,
                              ukm::SourceId source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  save_data_enabled_ = IsDataSaverEnabled(navigation_handle);

  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(navigation_handle->GetWebContents());
  if (!ui_tab_helper)
    return STOP_OBSERVING;

  previews::PreviewsUserData* previews_user_data =
      ui_tab_helper->GetPreviewsUserData(navigation_handle);
  if (!previews_user_data)
    return STOP_OBSERVING;

  committed_preview_ = previews_user_data->committed_previews_type();
  content::PreviewsState previews_state =
      previews_user_data->committed_previews_state();

  if (navigation_handle->GetWebContents()->GetContentsMimeType() ==
      kOfflinePreviewsMimeType) {
    if (!IsOfflinePreview(navigation_handle->GetWebContents()))
      return STOP_OBSERVING;
    offline_preview_seen_ = true;
    DCHECK_EQ(previews::GetMainFramePreviewsType(previews_state),
              previews::PreviewsType::OFFLINE);
  }

  if (previews_state && previews::GetMainFramePreviewsType(previews_state) ==
                            previews::PreviewsType::LITE_PAGE) {
    lite_page_seen_ = true;
  }
  if (previews_state && previews::GetMainFramePreviewsType(previews_state) ==
                            previews::PreviewsType::LITE_PAGE_REDIRECT) {
    lite_page_redirect_seen_ = true;
  }
  if (previews_state && previews::GetMainFramePreviewsType(previews_state) ==
                            previews::PreviewsType::NOSCRIPT) {
    noscript_seen_ = true;
  }
  if (previews_state && previews::GetMainFramePreviewsType(previews_state) ==
                            previews::PreviewsType::RESOURCE_LOADING_HINTS) {
    resource_loading_hints_seen_ = true;
  }
  if (previews_user_data &&
      previews_user_data->cache_control_no_transform_directive()) {
    origin_opt_out_occurred_ = true;
  }
  if (previews_user_data && previews_user_data->server_lite_page_info()) {
    navigation_restart_penalty_ =
        navigation_handle->NavigationStart() -
        previews_user_data->server_lite_page_info()->original_navigation_start;
  }

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewsUKMObserver::ShouldObserveMimeType(const std::string& mime_type) const {
  if (PageLoadMetricsObserver::ShouldObserveMimeType(mime_type) ==
          CONTINUE_OBSERVING ||
      mime_type == kOfflinePreviewsMimeType) {
    return CONTINUE_OBSERVING;
  }
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewsUKMObserver::OnStart(content::NavigationHandle* navigation_handle,
                             const GURL& currently_committed_url,
                             bool started_in_foreground) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!started_in_foreground)
    return STOP_OBSERVING;
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewsUKMObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordPreviewsTypes(info);
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewsUKMObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordPreviewsTypes(info);
  return STOP_OBSERVING;
}

void PreviewsUKMObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordPreviewsTypes(info);
}

void PreviewsUKMObserver::RecordPreviewsTypes(
    const page_load_metrics::PageLoadExtraInfo& info) {
  // Record the page end reason in UMA.
  if (committed_preview_ != PreviewsType::NONE) {
    UMA_HISTOGRAM_ENUMERATION(
        "Previews.PageEndReason", info.page_end_reason,
        page_load_metrics::PageEndReason::PAGE_END_REASON_COUNT);
  }
  base::UmaHistogramExactLinear(
      base::StringPrintf(
          "Previews.PageEndReason.%s",
          previews::GetStringNameForType(committed_preview_).c_str()),
      info.page_end_reason,
      page_load_metrics::PageEndReason::PAGE_END_REASON_COUNT);

  // Only record previews types when they are active.
  // |navigation_restart_penalty_| is included here because a Lite Page Redirect
  // preview can be attempted and not commit. This incurs the penalty but may
  // also cause no preview to be committed.
  if (!server_lofi_seen_ && !client_lofi_seen_ && !lite_page_seen_ &&
      !noscript_seen_ && !resource_loading_hints_seen_ &&
      !offline_preview_seen_ && !origin_opt_out_occurred_ &&
      !save_data_enabled_ && !lite_page_redirect_seen_ &&
      !navigation_restart_penalty_.has_value()) {
    return;
  }

  ukm::builders::Previews builder(info.source_id);
  if (server_lofi_seen_)
    builder.Setserver_lofi(1);
  if (client_lofi_seen_)
    builder.Setclient_lofi(1);
  if (lite_page_seen_)
    builder.Setlite_page(1);
  if (lite_page_redirect_seen_)
    builder.Setlite_page_redirect(1);
  if (noscript_seen_)
    builder.Setnoscript(1);
  if (resource_loading_hints_seen_)
    builder.Setresource_loading_hints(1);
  if (offline_preview_seen_)
    builder.Setoffline_preview(1);
  if (opt_out_occurred_)
    builder.Setopt_out(previews::params::IsPreviewsOmniboxUiEnabled() ? 2 : 1);
  if (origin_opt_out_occurred_)
    builder.Setorigin_opt_out(1);
  if (save_data_enabled_)
    builder.Setsave_data_enabled(1);
  if (navigation_restart_penalty_.has_value()) {
    builder.Setnavigation_restart_penalty(
        navigation_restart_penalty_->InMilliseconds());
  }
  builder.Record(ukm::UkmRecorder::Get());
}

void PreviewsUKMObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (extra_request_complete_info.data_reduction_proxy_data) {
    if (extra_request_complete_info.data_reduction_proxy_data
            ->lofi_received()) {
      server_lofi_seen_ = true;
    }
    if (extra_request_complete_info.data_reduction_proxy_data
            ->client_lofi_requested()) {
      client_lofi_seen_ = true;
    }
  }
}

void PreviewsUKMObserver::OnEventOccurred(const void* const event_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (event_key == PreviewsUITabHelper::OptOutEventKey())
    opt_out_occurred_ = true;
}

bool PreviewsUKMObserver::IsDataSaverEnabled(
    content::NavigationHandle* navigation_handle) const {
  Profile* profile = Profile::FromBrowserContext(
      navigation_handle->GetWebContents()->GetBrowserContext());

  data_reduction_proxy::DataReductionProxySettings*
      data_reduction_proxy_settings =
          DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
              profile);
  if (!data_reduction_proxy_settings) {
    DCHECK(profile->IsOffTheRecord());
    return false;
  }

  return data_reduction_proxy_settings->IsDataReductionProxyEnabled();
}

bool PreviewsUKMObserver::IsOfflinePreview(
    content::WebContents* web_contents) const {
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_pages::OfflinePageTabHelper* tab_helper =
      offline_pages::OfflinePageTabHelper::FromWebContents(web_contents);
  return tab_helper && tab_helper->GetOfflinePreviewItem();
#else
  return false;
#endif
}

}  // namespace previews
