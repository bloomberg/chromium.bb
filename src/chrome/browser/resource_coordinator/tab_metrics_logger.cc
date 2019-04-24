// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_metrics_logger.h"

#include <algorithm>
#include <string>

#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_ranker/mru_features.h"
#include "chrome/browser/resource_coordinator/tab_ranker/tab_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_importance_signals.h"
#include "net/base/mime_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/platform/web_sudden_termination_disabler_type.h"
#include "url/gurl.h"

using metrics::TabMetricsEvent;

namespace {

// Populates navigation-related metrics.
void PopulatePageTransitionFeatures(tab_ranker::TabFeatures* tab,
                                    ui::PageTransition page_transition) {
  // We only report the following core types.
  // Note: Redirects unrelated to clicking a link still get the "link" type.
  if (ui::PageTransitionCoreTypeIs(page_transition, ui::PAGE_TRANSITION_LINK) ||
      ui::PageTransitionCoreTypeIs(page_transition,
                                   ui::PAGE_TRANSITION_AUTO_BOOKMARK) ||
      ui::PageTransitionCoreTypeIs(page_transition,
                                   ui::PAGE_TRANSITION_FORM_SUBMIT) ||
      ui::PageTransitionCoreTypeIs(page_transition,
                                   ui::PAGE_TRANSITION_RELOAD)) {
    tab->page_transition_core_type =
        ui::PageTransitionStripQualifier(page_transition);
  }

  tab->page_transition_from_address_bar =
      (page_transition & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) != 0;
  tab->page_transition_is_redirect =
      ui::PageTransitionIsRedirect(page_transition);
}

}  // namespace

TabMetricsLogger::TabMetricsLogger() = default;
TabMetricsLogger::~TabMetricsLogger() = default;

// static
TabMetricsEvent::ContentType TabMetricsLogger::GetContentTypeFromMimeType(
    const std::string& mime_type) {
  // Test for special cases before testing wildcard types.
  if (mime_type.empty())
    return TabMetricsEvent::CONTENT_TYPE_UNKNOWN;
  if (net::MatchesMimeType("text/html", mime_type))
    return TabMetricsEvent::CONTENT_TYPE_TEXT_HTML;
  if (net::MatchesMimeType("application/*", mime_type))
    return TabMetricsEvent::CONTENT_TYPE_APPLICATION;
  if (net::MatchesMimeType("audio/*", mime_type))
    return TabMetricsEvent::CONTENT_TYPE_AUDIO;
  if (net::MatchesMimeType("image/*", mime_type))
    return TabMetricsEvent::CONTENT_TYPE_IMAGE;
  if (net::MatchesMimeType("text/*", mime_type))
    return TabMetricsEvent::CONTENT_TYPE_TEXT;
  if (net::MatchesMimeType("video/*", mime_type))
    return TabMetricsEvent::CONTENT_TYPE_VIDEO;
  return TabMetricsEvent::CONTENT_TYPE_OTHER;
}

// static
int TabMetricsLogger::GetSiteEngagementScore(
    content::WebContents* web_contents) {
  if (!SiteEngagementService::IsEnabled())
    return -1;

  SiteEngagementService* service = SiteEngagementService::Get(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  DCHECK(service);

  // Scores range from 0 to 100. Round down to a multiple of 10 to conform to
  // privacy guidelines.
  double raw_score = service->GetScore(web_contents->GetVisibleURL());
  int rounded_score = static_cast<int>(raw_score / 10) * 10;
  DCHECK_LE(0, rounded_score);
  DCHECK_GE(100, rounded_score);
  return rounded_score;
}

// static
tab_ranker::TabFeatures TabMetricsLogger::GetTabFeatures(
    const Browser* browser,
    const TabMetrics& tab_metrics,
    base::TimeDelta inactive_duration) {
  DCHECK(browser);
  const TabStripModel* tab_strip_model = browser->tab_strip_model();
  content::WebContents* web_contents = tab_metrics.web_contents;

  tab_ranker::TabFeatures tab;

  tab.has_before_unload_handler =
      web_contents->GetMainFrame()->GetSuddenTerminationDisablerState(
          blink::kBeforeUnloadHandler);
  tab.has_form_entry =
      web_contents->GetPageImportanceSignals().had_form_interaction;
  tab.host = web_contents->GetLastCommittedURL().host();

  int index = tab_strip_model->GetIndexOfWebContents(web_contents);
  DCHECK_NE(index, TabStripModel::kNoTab);
  tab.is_pinned = tab_strip_model->IsTabPinned(index);

  tab.key_event_count = tab_metrics.page_metrics.key_event_count;
  tab.mouse_event_count = tab_metrics.page_metrics.mouse_event_count;
  tab.navigation_entry_count = web_contents->GetController().GetEntryCount();
  tab.num_reactivations = tab_metrics.page_metrics.num_reactivations;

  PopulatePageTransitionFeatures(&tab, tab_metrics.page_transition);

  if (SiteEngagementService::IsEnabled())
    tab.site_engagement_score = GetSiteEngagementScore(web_contents);

  tab.time_from_backgrounded = inactive_duration.InMilliseconds();
  tab.touch_event_count = tab_metrics.page_metrics.touch_event_count;

  // This checks if the tab was audible within the past two seconds, same as the
  // audio indicator in the tab strip.
  auto* audible_helper = RecentlyAudibleHelper::FromWebContents(web_contents);
  tab.was_recently_audible = audible_helper->WasRecentlyAudible();
  return tab;
}

void TabMetricsLogger::LogTabMetrics(
    ukm::SourceId ukm_source_id,
    const tab_ranker::TabFeatures& tab_features,
    content::WebContents* web_contents,
    int64_t label_id) {
  if (!ukm_source_id)
    return;

  if (web_contents) {
    // UKM recording is disabled in OTR.
    if (web_contents->GetBrowserContext()->IsOffTheRecord())
      return;

    // Verify that the browser is not closing.
    const Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
    if (base::ContainsKey(
            BrowserList::GetInstance()->currently_closing_browsers(),
            browser)) {
      return;
    }

    const TabStripModel* tab_strip_model = browser->tab_strip_model();
    if (tab_strip_model->closing_all())
      return;
  }

  ukm::builders::TabManager_TabMetrics entry(ukm_source_id);
  PopulateTabFeaturesToUkmEntry(tab_features, &entry);
  entry.SetLabelId(label_id);
  entry.SetQueryId(query_id_);
  entry.Record(ukm::UkmRecorder::Get());
}

void TabMetricsLogger::LogForegroundedOrClosedMetrics(
    ukm::SourceId ukm_source_id,
    const ForegroundedOrClosedMetrics& metrics) {
  if (!ukm_source_id)
    return;

  ukm::builders::TabManager_Background_ForegroundedOrClosed(ukm_source_id)
      .SetLabelId(metrics.label_id)
      .SetIsForegrounded(metrics.is_foregrounded)
      .SetMRUIndex(metrics.mru_index)
      .SetTimeFromBackgrounded(metrics.time_from_backgrounded)
      .SetTotalTabCount(metrics.total_tab_count)
      .SetIsDiscarded(metrics.is_discarded)
      .Record(ukm::UkmRecorder::Get());
}

void TabMetricsLogger::LogTabLifetime(ukm::SourceId ukm_source_id,
                                      base::TimeDelta time_since_navigation) {
  if (!ukm_source_id)
    return;
  ukm::builders::TabManager_TabLifetime(ukm_source_id)
      .SetTimeSinceNavigation(time_since_navigation.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}
