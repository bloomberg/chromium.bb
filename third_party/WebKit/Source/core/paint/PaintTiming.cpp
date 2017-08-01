// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintTiming.h"

#include <memory>
#include <utility>

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/ProgressTracker.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "platform/Histogram.h"
#include "platform/WebFrameScheduler.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "public/platform/WebLayerTreeView.h"

namespace blink {

namespace {

Performance* GetPerformanceInstance(LocalFrame* frame) {
  Performance* performance = nullptr;
  if (frame && frame->DomWindow()) {
    performance = DOMWindowPerformance::performance(*frame->DomWindow());
  }
  return performance;
}

}  // namespace

static const char kSupplementName[] = "PaintTiming";

PaintTiming& PaintTiming::From(Document& document) {
  PaintTiming* timing = static_cast<PaintTiming*>(
      Supplement<Document>::From(document, kSupplementName));
  if (!timing) {
    timing = new PaintTiming(document);
    Supplement<Document>::ProvideTo(document, kSupplementName, timing);
  }
  return *timing;
}

void PaintTiming::MarkFirstPaint() {
  // Test that m_firstPaint is non-zero here, as well as in setFirstPaint, so
  // we avoid invoking monotonicallyIncreasingTime() on every call to
  // markFirstPaint().
  if (first_paint_ != 0.0)
    return;
  SetFirstPaint(MonotonicallyIncreasingTime());
  NotifyPaintTimingChanged();
}

void PaintTiming::MarkFirstContentfulPaint() {
  // Test that m_firstContentfulPaint is non-zero here, as well as in
  // setFirstContentfulPaint, so we avoid invoking
  // monotonicallyIncreasingTime() on every call to
  // markFirstContentfulPaint().
  if (first_contentful_paint_ != 0.0)
    return;
  SetFirstContentfulPaint(MonotonicallyIncreasingTime());
  NotifyPaintTimingChanged();
}

void PaintTiming::MarkFirstTextPaint() {
  if (first_text_paint_ != 0.0)
    return;
  first_text_paint_ = MonotonicallyIncreasingTime();
  SetFirstContentfulPaint(first_text_paint_);
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "loading,rail,devtools.timeline", "firstTextPaint",
      TraceEvent::ToTraceTimestamp(first_text_paint_), "frame", GetFrame());
  NotifyPaintTimingChanged();
}

void PaintTiming::MarkFirstImagePaint() {
  if (first_image_paint_ != 0.0)
    return;
  first_image_paint_ = MonotonicallyIncreasingTime();
  SetFirstContentfulPaint(first_image_paint_);
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "loading,rail,devtools.timeline", "firstImagePaint",
      TraceEvent::ToTraceTimestamp(first_image_paint_), "frame", GetFrame());
  NotifyPaintTimingChanged();
}

void PaintTiming::SetFirstMeaningfulPaintCandidate(double timestamp) {
  if (first_meaningful_paint_candidate_)
    return;
  first_meaningful_paint_candidate_ = timestamp;
  if (GetFrame() && GetFrame()->View() && !GetFrame()->View()->IsAttached()) {
    GetFrame()->FrameScheduler()->OnFirstMeaningfulPaint();
  }
}

void PaintTiming::SetFirstMeaningfulPaint(
    double stamp,
    double swap_stamp,
    FirstMeaningfulPaintDetector::HadUserInput had_input) {
  DCHECK_EQ(first_meaningful_paint_, 0.0);
  TRACE_EVENT_MARK_WITH_TIMESTAMP2("loading,rail,devtools.timeline",
                                   "firstMeaningfulPaint",
                                   TraceEvent::ToTraceTimestamp(stamp), "frame",
                                   GetFrame(), "afterUserInput", had_input);

  // Notify FMP for UMA only if there's no user input before FMP, so that layout
  // changes caused by user interactions wouldn't be considered as FMP.
  if (had_input == FirstMeaningfulPaintDetector::kNoUserInput) {
    first_meaningful_paint_ = stamp;
    first_meaningful_paint_swap_ = swap_stamp;
    NotifyPaintTimingChanged();
  }

  ReportUserInputHistogram(had_input);
}

void PaintTiming::ReportUserInputHistogram(
    FirstMeaningfulPaintDetector::HadUserInput had_input) {
  DEFINE_STATIC_LOCAL(EnumerationHistogram, had_user_input_histogram,
                      ("PageLoad.Internal.PaintTiming."
                       "HadUserInputBeforeFirstMeaningfulPaint",
                       FirstMeaningfulPaintDetector::kHadUserInputEnumMax));

  if (GetFrame() && GetFrame()->IsMainFrame())
    had_user_input_histogram.Count(had_input);
}

void PaintTiming::NotifyPaint(bool is_first_paint,
                              bool text_painted,
                              bool image_painted) {
  if (is_first_paint)
    MarkFirstPaint();
  if (text_painted)
    MarkFirstTextPaint();
  if (image_painted)
    MarkFirstImagePaint();
  fmp_detector_->NotifyPaint();
}

DEFINE_TRACE(PaintTiming) {
  visitor->Trace(fmp_detector_);
  Supplement<Document>::Trace(visitor);
}

PaintTiming::PaintTiming(Document& document)
    : Supplement<Document>(document),
      fmp_detector_(new FirstMeaningfulPaintDetector(this, document)) {}

LocalFrame* PaintTiming::GetFrame() const {
  return GetSupplementable()->GetFrame();
}

void PaintTiming::NotifyPaintTimingChanged() {
  if (GetSupplementable()->Loader())
    GetSupplementable()->Loader()->DidChangePerformanceTiming();
}

void PaintTiming::SetFirstPaint(double stamp) {
  if (first_paint_ != 0.0)
    return;
  first_paint_ = stamp;
  TRACE_EVENT_INSTANT1("loading,rail,devtools.timeline", "firstPaint",
                       TRACE_EVENT_SCOPE_PROCESS, "frame", GetFrame());
  RegisterNotifySwapTime(PaintEvent::kFirstPaint);
}

void PaintTiming::SetFirstContentfulPaint(double stamp) {
  if (first_contentful_paint_ != 0.0)
    return;
  SetFirstPaint(stamp);
  first_contentful_paint_ = stamp;
  TRACE_EVENT_INSTANT1("loading,rail,devtools.timeline", "firstContentfulPaint",
                       TRACE_EVENT_SCOPE_PROCESS, "frame", GetFrame());
  RegisterNotifySwapTime(PaintEvent::kFirstContentfulPaint);
  GetFrame()->Loader().Progress().DidFirstContentfulPaint();
}

void PaintTiming::RegisterNotifySwapTime(PaintEvent event) {
  RegisterNotifySwapTime(event,
                         WTF::Bind(&PaintTiming::ReportSwapTime,
                                   WrapCrossThreadWeakPersistent(this), event));
}

void PaintTiming::RegisterNotifySwapTime(
    PaintEvent event,
    WTF::Function<void(bool, double)> callback) {
  // ReportSwapTime on layerTreeView will queue a swap-promise, the callback is
  // called when the swap for current render frame completes or fails to happen.
  if (!GetFrame() || !GetFrame()->GetPage())
    return;
  if (WebLayerTreeView* layerTreeView =
          GetFrame()->GetPage()->GetChromeClient().GetWebLayerTreeView(
              GetFrame())) {
    layerTreeView->NotifySwapTime(ConvertToBaseCallback(std::move(callback)));
  }
}

void PaintTiming::ReportSwapTime(PaintEvent event,
                                 bool did_swap,
                                 double timestamp) {
  if (!did_swap)
    return;
  Performance* performance = GetPerformanceInstance(GetFrame());
  switch (event) {
    case PaintEvent::kFirstPaint:
      first_paint_swap_ = timestamp;
      if (performance)
        performance->AddFirstPaintTiming(first_paint_);
      return;
    case PaintEvent::kFirstContentfulPaint:
      first_contentful_paint_swap_ = timestamp;
      if (performance)
        performance->AddFirstContentfulPaintTiming(first_contentful_paint_);
      return;
    default:
      NOTREACHED();
  }
}

}  // namespace blink
