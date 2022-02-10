// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/content_to_visible_time_reporter.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom.h"
#include "ui/gfx/presentation_feedback.h"

namespace blink {

namespace {

//  Used to generate unique "TabSwitching::Latency" event ids. Note: The address
//  of ContentToVisibleTimeReporter can't be used as an id because a single
//  ContentToVisibleTimeReporter can generate multiple overlapping events.
int g_num_trace_events_in_process = 0;

const char* GetHistogramSuffix(
    bool has_saved_frames,
    const mojom::RecordContentToVisibleTimeRequest& start_state) {
  if (has_saved_frames)
    return "WithSavedFrames";

  if (start_state.destination_is_loaded) {
    return "NoSavedFrames_Loaded";
  } else {
    return "NoSavedFrames_NotLoaded";
  }
}

void ReportUnOccludedMetric(const base::TimeTicks requested_time,
                            const gfx::PresentationFeedback& feedback) {
  const base::TimeDelta delta = feedback.timestamp - requested_time;
  UMA_HISTOGRAM_TIMES("Aura.WebContentsWindowUnOccludedTime", delta);
}

void RecordBackForwardCacheRestoreMetric(
    const base::TimeTicks requested_time,
    const gfx::PresentationFeedback& feedback) {
  const base::TimeDelta delta = feedback.timestamp - requested_time;
  // Histogram to record the content to visible duration after restoring a page
  // from back-forward cache. Here min, max bucket size are same as the
  // "PageLoad.PaintTiming.NavigationToFirstContentfulPaint" metric.
  base::UmaHistogramCustomTimes(
      "BackForwardCache.Restore.NavigationToFirstPaint", delta,
      base::Milliseconds(10), base::Minutes(10), 100);
}

}  // namespace

ContentToVisibleTimeReporter::ContentToVisibleTimeReporter() = default;

ContentToVisibleTimeReporter::~ContentToVisibleTimeReporter() = default;

base::OnceCallback<void(const gfx::PresentationFeedback&)>
ContentToVisibleTimeReporter::TabWasShown(
    bool has_saved_frames,
    mojom::RecordContentToVisibleTimeRequestPtr start_state) {
  DCHECK(!start_state->event_start_time.is_null());
  if (IsTabSwitchMetric2FeatureEnabled() && tab_switch_start_state_ &&
      tab_switch_start_state_->show_reason_tab_switching &&
      start_state->show_reason_tab_switching) {
    // Missed a tab hide, so record an incomplete tab switch. As a side effect
    // this will reset the state.
    //
    // This can happen when the tab is backgrounded, but still visible in a
    // visible capturer or VR, so the widget is never notified to hide.
    // TabWasHidden is only called correctly for *hidden* capturers (such as
    // picture-in-picture). See WebContentsImpl::CalculatePageVisibilityState
    // for more details.
    //
    // TODO(crbug.com/1289266): Refactor visibility states to call TabWasHidden
    // every time a tab is backgrounded, even if the content is still visible.
    RecordHistogramsAndTraceEvents(TabSwitchResult::kMissedTabHide,
                                   true /* show_reason_tab_switching */,
                                   false /* show_reason_unoccluded */,
                                   false /* show_reason_bfcache_restore */,
                                   gfx::PresentationFeedback::Failure());
  }
  DCHECK(!tab_switch_start_state_);

  // Invalidate previously issued callbacks, to avoid accessing a null
  // |tab_switch_start_state_|.
  //
  // TODO(crbug.com/1289266): Make sure that TabWasShown() is never called twice
  // without a call to TabWasHidden() in-between, and remove this mitigation.
  weak_ptr_factory_.InvalidateWeakPtrs();

  has_saved_frames_ = has_saved_frames;
  tab_switch_start_state_ = std::move(start_state);

  // |tab_switch_start_state_| is only reset by RecordHistogramsAndTraceEvents
  // once the metrics have been emitted.
  return base::BindOnce(
      &ContentToVisibleTimeReporter::RecordHistogramsAndTraceEvents,
      weak_ptr_factory_.GetWeakPtr(), TabSwitchResult::kSuccess,
      tab_switch_start_state_->show_reason_tab_switching,
      tab_switch_start_state_->show_reason_unoccluded,
      tab_switch_start_state_->show_reason_bfcache_restore);
}

base::OnceCallback<void(const gfx::PresentationFeedback&)>
ContentToVisibleTimeReporter::TabWasShown(bool has_saved_frames,
                                          base::TimeTicks event_start_time,
                                          bool destination_is_loaded,
                                          bool show_reason_tab_switching,
                                          bool show_reason_unoccluded,
                                          bool show_reason_bfcache_restore) {
  return TabWasShown(
      has_saved_frames,
      mojom::RecordContentToVisibleTimeRequest::New(
          event_start_time, destination_is_loaded, show_reason_tab_switching,
          show_reason_unoccluded, show_reason_bfcache_restore));
}

void ContentToVisibleTimeReporter::TabWasHidden() {
  if (tab_switch_start_state_) {
    RecordHistogramsAndTraceEvents(TabSwitchResult::kIncomplete,
                                   true /* show_reason_tab_switching */,
                                   false /* show_reason_unoccluded */,
                                   false /* show_reason_bfcache_restore */,
                                   gfx::PresentationFeedback::Failure());
    weak_ptr_factory_.InvalidateWeakPtrs();
  }
}

bool ContentToVisibleTimeReporter::IsTabSwitchMetric2FeatureEnabled() {
  if (!is_tab_switch_metric2_feature_enabled_) {
    is_tab_switch_metric2_feature_enabled_ =
        base::FeatureList::IsEnabled(blink::features::kTabSwitchMetrics2);
  }
  return *is_tab_switch_metric2_feature_enabled_;
}

void ContentToVisibleTimeReporter::RecordHistogramsAndTraceEvents(
    TabSwitchResult tab_switch_result,
    bool show_reason_tab_switching,
    bool show_reason_unoccluded,
    bool show_reason_bfcache_restore,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(tab_switch_start_state_);
  // If the DCHECK fail, make sure RenderWidgetHostImpl::WasShown was triggered
  // for recording the event.
  DCHECK(show_reason_bfcache_restore || show_reason_unoccluded ||
         show_reason_tab_switching);
  // The kPresentationFailure result should only be used if `feedback` has a
  // failure.
  DCHECK_NE(tab_switch_result, TabSwitchResult::kPresentationFailure);

  if (show_reason_bfcache_restore) {
    RecordBackForwardCacheRestoreMetric(
        tab_switch_start_state_->event_start_time, feedback);
  }

  if (show_reason_unoccluded) {
    ReportUnOccludedMetric(tab_switch_start_state_->event_start_time, feedback);
  }

  if (!show_reason_tab_switching)
    return;

  // Tab switching has occurred.
  if (tab_switch_result == TabSwitchResult::kSuccess &&
      feedback.flags & gfx::PresentationFeedback::kFailure) {
    tab_switch_result = TabSwitchResult::kPresentationFailure;
  }

  const auto tab_switch_duration =
      feedback.timestamp - tab_switch_start_state_->event_start_time;

  // Record trace events.
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "latency", "TabSwitching::Latency",
      TRACE_ID_LOCAL(g_num_trace_events_in_process),
      tab_switch_start_state_->event_start_time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP2(
      "latency", "TabSwitching::Latency",
      TRACE_ID_LOCAL(g_num_trace_events_in_process), feedback.timestamp,
      "result", tab_switch_result, "latency",
      tab_switch_duration.InMillisecondsF());
  ++g_num_trace_events_in_process;

  const char* suffix =
      GetHistogramSuffix(has_saved_frames_, *tab_switch_start_state_);

  if (IsTabSwitchMetric2FeatureEnabled()) {
    // Record result histogram.
    base::UmaHistogramEnumeration(
        base::StrCat({"Browser.Tabs.TabSwitchResult2.", suffix}),
        tab_switch_result);

    // Record latency histogram.
    switch (tab_switch_result) {
      case TabSwitchResult::kSuccess:
        base::UmaHistogramMediumTimes(
            base::StrCat({"Browser.Tabs.TotalSwitchDuration2.", suffix}),
            tab_switch_duration);
        break;
      case TabSwitchResult::kMissedTabHide:
      case TabSwitchResult::kIncomplete:
        base::UmaHistogramMediumTimes(
            base::StrCat(
                {"Browser.Tabs.TotalIncompleteSwitchDuration2.", suffix}),
            tab_switch_duration);
        break;
      case TabSwitchResult::kPresentationFailure:
        break;
    }
  }

  // TODO(crbug.com/1164477): Remove these once the TabSwitchMetric2 feature
  // is enabled by default. During the validation experiment, the experiment
  // group will log TabSwitchResult2 and TabSwitchResult with the same values,
  // so that it's easy to compare the same TabSwitchResult metrics for the
  // control group and experiment group. If TabSwitchResult in the experiment
  // group is ok, so is TabSwitchResult2, and we can then deprecate
  // TabSwitchResult since the historical data outside the experiment group is
  // unreliable.

  // Record result histogram.
  base::UmaHistogramEnumeration(
      base::StrCat({"Browser.Tabs.TabSwitchResult.", suffix}),
      tab_switch_result);

  // Record latency histogram.
  switch (tab_switch_result) {
    case TabSwitchResult::kSuccess:
      base::UmaHistogramTimes(
          base::StrCat({"Browser.Tabs.TotalSwitchDuration.", suffix}),
          tab_switch_duration);
      break;
    case TabSwitchResult::kMissedTabHide:
      // This was not included in the v1 histograms.
      DCHECK(IsTabSwitchMetric2FeatureEnabled());
      [[fallthrough]];
    case TabSwitchResult::kIncomplete:
      base::UmaHistogramTimes(
          base::StrCat({"Browser.Tabs.TotalIncompleteSwitchDuration.", suffix}),
          tab_switch_duration);
      break;
    case TabSwitchResult::kPresentationFailure:
      break;
  }

  // Reset tab switch information.
  has_saved_frames_ = false;
  tab_switch_start_state_.reset();
}

}  // namespace blink
