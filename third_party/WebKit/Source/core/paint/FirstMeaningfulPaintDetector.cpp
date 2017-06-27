// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/FirstMeaningfulPaintDetector.h"

#include "core/css/FontFaceSet.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/paint/PaintTiming.h"
#include "platform/Histogram.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/loader/fetch/ResourceFetcher.h"

namespace blink {

namespace {

// Web fonts that laid out more than this number of characters block First
// Meaningful Paint.
const int kBlankCharactersThreshold = 200;

}  // namespace

FirstMeaningfulPaintDetector& FirstMeaningfulPaintDetector::From(
    Document& document) {
  return PaintTiming::From(document).GetFirstMeaningfulPaintDetector();
}

FirstMeaningfulPaintDetector::FirstMeaningfulPaintDetector(
    PaintTiming* paint_timing,
    Document& document)
    : paint_timing_(paint_timing),
      network0_quiet_timer_(
          TaskRunnerHelper::Get(TaskType::kUnspecedTimer, &document),
          this,
          &FirstMeaningfulPaintDetector::Network0QuietTimerFired),
      network2_quiet_timer_(
          TaskRunnerHelper::Get(TaskType::kUnspecedTimer, &document),
          this,
          &FirstMeaningfulPaintDetector::Network2QuietTimerFired) {}

Document* FirstMeaningfulPaintDetector::GetDocument() {
  return paint_timing_->GetSupplementable();
}

// Computes "layout significance" (http://goo.gl/rytlPL) of a layout operation.
// Significance of a layout is the number of layout objects newly added to the
// layout tree, weighted by page height (before and after the layout).
// A paint after the most significance layout during page load is reported as
// First Meaningful Paint.
void FirstMeaningfulPaintDetector::MarkNextPaintAsMeaningfulIfNeeded(
    const LayoutObjectCounter& counter,
    int contents_height_before_layout,
    int contents_height_after_layout,
    int visible_height) {
  if (network0_quiet_reached_ && network2_quiet_reached_)
    return;

  unsigned delta = counter.Count() - prev_layout_object_count_;
  prev_layout_object_count_ = counter.Count();

  if (visible_height == 0)
    return;

  double ratio_before = std::max(
      1.0, static_cast<double>(contents_height_before_layout) / visible_height);
  double ratio_after = std::max(
      1.0, static_cast<double>(contents_height_after_layout) / visible_height);
  double significance = delta / ((ratio_before + ratio_after) / 2);

  // If the page has many blank characters, the significance value is
  // accumulated until the text become visible.
  int approximate_blank_character_count =
      FontFaceSet::ApproximateBlankCharacterCount(*GetDocument());
  if (approximate_blank_character_count > kBlankCharactersThreshold) {
    accumulated_significance_while_having_blank_text_ += significance;
  } else {
    significance += accumulated_significance_while_having_blank_text_;
    accumulated_significance_while_having_blank_text_ = 0;
    if (significance > max_significance_so_far_) {
      next_paint_is_meaningful_ = true;
      max_significance_so_far_ = significance;
    }
  }
}

void FirstMeaningfulPaintDetector::NotifyPaint() {
  if (!next_paint_is_meaningful_)
    return;

  // Skip document background-only paints.
  if (paint_timing_->FirstPaint() == 0.0)
    return;

  provisional_first_meaningful_paint_ = MonotonicallyIncreasingTime();
  had_user_input_before_provisional_first_meaningful_paint_ = had_user_input_;
  next_paint_is_meaningful_ = false;

  if (network2_quiet_reached_)
    return;

  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "loading,devtools.timeline", "firstMeaningfulPaintCandidate",
      TraceEvent::ToTraceTimestamp(provisional_first_meaningful_paint_),
      "frame", GetDocument()->GetFrame());
  // Ignore the first meaningful paint candidate as this generally is the first
  // contentful paint itself.
  if (!seen_first_meaningful_paint_candidate_) {
    seen_first_meaningful_paint_candidate_ = true;
    return;
  }
  paint_timing_->SetFirstMeaningfulPaintCandidate(
      provisional_first_meaningful_paint_);
}

// This is called only on FirstMeaningfulPaintDetector for main frame.
void FirstMeaningfulPaintDetector::NotifyInputEvent() {
  // Ignore user inputs before first paint.
  if (paint_timing_->FirstPaint() == 0.0)
    return;
  had_user_input_ = kHadUserInput;
}

int FirstMeaningfulPaintDetector::ActiveConnections() {
  DCHECK(GetDocument());
  ResourceFetcher* fetcher = GetDocument()->Fetcher();
  return fetcher->BlockingRequestCount() + fetcher->NonblockingRequestCount();
}

// This function is called when the number of active connections is decreased.
void FirstMeaningfulPaintDetector::CheckNetworkStable() {
  DCHECK(GetDocument());
  if (!GetDocument()->HasFinishedParsing())
    return;

  SetNetworkQuietTimers(ActiveConnections());
}

void FirstMeaningfulPaintDetector::SetNetworkQuietTimers(
    int active_connections) {
  if (!network0_quiet_reached_ && active_connections == 0) {
    // This restarts 0-quiet timer if it's already running.
    network0_quiet_timer_.StartOneShot(kNetwork0QuietWindowSeconds,
                                       BLINK_FROM_HERE);
  }
  if (!network2_quiet_reached_ && active_connections <= 2) {
    // If activeConnections < 2 and the timer is already running, current
    // 2-quiet window continues; the timer shouldn't be restarted.
    if (active_connections == 2 || !network2_quiet_timer_.IsActive()) {
      network2_quiet_timer_.StartOneShot(kNetwork2QuietWindowSeconds,
                                         BLINK_FROM_HERE);
    }
  }
}

void FirstMeaningfulPaintDetector::Network0QuietTimerFired(TimerBase*) {
  if (!GetDocument() || network0_quiet_reached_ || ActiveConnections() > 0 ||
      !paint_timing_->FirstContentfulPaint())
    return;
  network0_quiet_reached_ = true;

  if (provisional_first_meaningful_paint_) {
    // Enforce FirstContentfulPaint <= FirstMeaningfulPaint.
    first_meaningful_paint0_quiet_ =
        std::max(provisional_first_meaningful_paint_,
                 paint_timing_->FirstContentfulPaint());
  }
  ReportHistograms();
}

void FirstMeaningfulPaintDetector::Network2QuietTimerFired(TimerBase*) {
  if (!GetDocument() || network2_quiet_reached_ || ActiveConnections() > 2 ||
      !paint_timing_->FirstContentfulPaint())
    return;
  network2_quiet_reached_ = true;

  if (provisional_first_meaningful_paint_) {
    // If there's only been one contentful paint, then there won't have been
    // a meaningful paint signalled to the Scheduler, so mark one now.
    // This is a no-op if a FMPC has already been marked.
    paint_timing_->SetFirstMeaningfulPaintCandidate(
        provisional_first_meaningful_paint_);
    // Enforce FirstContentfulPaint <= FirstMeaningfulPaint.
    first_meaningful_paint2_quiet_ =
        std::max(provisional_first_meaningful_paint_,
                 paint_timing_->FirstContentfulPaint());
    // Report FirstMeaningfulPaint when the page reached network 2-quiet.
    paint_timing_->SetFirstMeaningfulPaint(
        first_meaningful_paint2_quiet_,
        had_user_input_before_provisional_first_meaningful_paint_);
  }
  ReportHistograms();
}

void FirstMeaningfulPaintDetector::ReportHistograms() {
  // This enum backs an UMA histogram, and should be treated as append-only.
  enum HadNetworkQuiet {
    kHadNetwork0Quiet,
    kHadNetwork2Quiet,
    kHadNetworkQuietEnumMax
  };
  DEFINE_STATIC_LOCAL(EnumerationHistogram, had_network_quiet_histogram,
                      ("PageLoad.Experimental.Renderer."
                       "FirstMeaningfulPaintDetector.HadNetworkQuiet",
                       kHadNetworkQuietEnumMax));

  // This enum backs an UMA histogram, and should be treated as append-only.
  enum FMPOrderingEnum {
    kFMP0QuietFirst,
    kFMP2QuietFirst,
    kFMP0QuietEqualFMP2Quiet,
    kFMPOrderingEnumMax
  };
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, first_meaningful_paint_ordering_histogram,
      ("PageLoad.Experimental.Renderer.FirstMeaningfulPaintDetector."
       "FirstMeaningfulPaintOrdering",
       kFMPOrderingEnumMax));

  if (first_meaningful_paint0_quiet_ && first_meaningful_paint2_quiet_) {
    int sample;
    if (first_meaningful_paint2_quiet_ < first_meaningful_paint0_quiet_) {
      sample = kFMP0QuietFirst;
    } else if (first_meaningful_paint2_quiet_ >
               first_meaningful_paint0_quiet_) {
      sample = kFMP2QuietFirst;
    } else {
      sample = kFMP0QuietEqualFMP2Quiet;
    }
    first_meaningful_paint_ordering_histogram.Count(sample);
  } else if (first_meaningful_paint0_quiet_) {
    had_network_quiet_histogram.Count(kHadNetwork0Quiet);
  } else if (first_meaningful_paint2_quiet_) {
    had_network_quiet_histogram.Count(kHadNetwork2Quiet);
  }
}

DEFINE_TRACE(FirstMeaningfulPaintDetector) {
  visitor->Trace(paint_timing_);
}

}  // namespace blink
