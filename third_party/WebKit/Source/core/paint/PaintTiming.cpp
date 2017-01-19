// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintTiming.h"

#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/DocumentLoader.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "platform/WebFrameScheduler.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

namespace {

Performance* getPerformanceInstance(LocalFrame* frame) {
  Performance* performance = nullptr;
  if (frame && frame->domWindow()) {
    performance = DOMWindowPerformance::performance(*frame->domWindow());
  }
  return performance;
}

}  // namespace

static const char kSupplementName[] = "PaintTiming";

PaintTiming& PaintTiming::from(Document& document) {
  PaintTiming* timing = static_cast<PaintTiming*>(
      Supplement<Document>::from(document, kSupplementName));
  if (!timing) {
    timing = new PaintTiming(document);
    Supplement<Document>::provideTo(document, kSupplementName, timing);
  }
  return *timing;
}

void PaintTiming::markFirstPaint() {
  // Test that m_firstPaint is non-zero here, as well as in setFirstPaint, so
  // we avoid invoking monotonicallyIncreasingTime() on every call to
  // markFirstPaint().
  if (m_firstPaint != 0.0)
    return;
  setFirstPaint(monotonicallyIncreasingTime());
  notifyPaintTimingChanged();
}

void PaintTiming::markFirstContentfulPaint() {
  // Test that m_firstContentfulPaint is non-zero here, as well as in
  // setFirstContentfulPaint, so we avoid invoking
  // monotonicallyIncreasingTime() on every call to
  // markFirstContentfulPaint().
  if (m_firstContentfulPaint != 0.0)
    return;
  setFirstContentfulPaint(monotonicallyIncreasingTime());
  notifyPaintTimingChanged();
}

void PaintTiming::markFirstTextPaint() {
  if (m_firstTextPaint != 0.0)
    return;
  m_firstTextPaint = monotonicallyIncreasingTime();
  setFirstContentfulPaint(m_firstTextPaint);
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "blink.user_timing,rail", "firstTextPaint",
      TraceEvent::toTraceTimestamp(m_firstTextPaint), "frame", frame());
  notifyPaintTimingChanged();
}

void PaintTiming::markFirstImagePaint() {
  if (m_firstImagePaint != 0.0)
    return;
  m_firstImagePaint = monotonicallyIncreasingTime();
  setFirstContentfulPaint(m_firstImagePaint);
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "blink.user_timing,rail", "firstImagePaint",
      TraceEvent::toTraceTimestamp(m_firstImagePaint), "frame", frame());
  notifyPaintTimingChanged();
}

void PaintTiming::markFirstMeaningfulPaintCandidate() {
  if (m_firstMeaningfulPaintCandidate)
    return;
  m_firstMeaningfulPaintCandidate = monotonicallyIncreasingTime();
  if (frame() && frame()->view() && !frame()->view()->parent()) {
    frame()->frameScheduler()->onFirstMeaningfulPaint();
  }
}

void PaintTiming::setFirstMeaningfulPaint(double stamp) {
  DCHECK_EQ(m_firstMeaningfulPaint, 0.0);
  m_firstMeaningfulPaint = stamp;
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "blink.user_timing", "firstMeaningfulPaint",
      TraceEvent::toTraceTimestamp(m_firstMeaningfulPaint), "frame", frame());
  notifyPaintTimingChanged();
}

void PaintTiming::notifyPaint(bool isFirstPaint,
                              bool textPainted,
                              bool imagePainted) {
  if (isFirstPaint)
    markFirstPaint();
  if (textPainted)
    markFirstTextPaint();
  if (imagePainted)
    markFirstImagePaint();
  m_fmpDetector->notifyPaint();
}

DEFINE_TRACE(PaintTiming) {
  visitor->trace(m_fmpDetector);
  Supplement<Document>::trace(visitor);
}

PaintTiming::PaintTiming(Document& document)
    : Supplement<Document>(document),
      m_fmpDetector(new FirstMeaningfulPaintDetector(this, document)) {}

LocalFrame* PaintTiming::frame() const {
  return supplementable()->frame();
}

void PaintTiming::notifyPaintTimingChanged() {
  if (supplementable()->loader())
    supplementable()->loader()->didChangePerformanceTiming();
}

void PaintTiming::setFirstPaint(double stamp) {
  if (m_firstPaint != 0.0)
    return;
  m_firstPaint = stamp;
  Performance* performance = getPerformanceInstance(frame());
  if (performance)
    performance->addFirstPaintTiming(m_firstPaint);

  TRACE_EVENT_INSTANT1("blink.user_timing,rail", "firstPaint",
                       TRACE_EVENT_SCOPE_PROCESS, "frame", frame());
}

void PaintTiming::setFirstContentfulPaint(double stamp) {
  if (m_firstContentfulPaint != 0.0)
    return;
  setFirstPaint(stamp);
  m_firstContentfulPaint = stamp;
  Performance* performance = getPerformanceInstance(frame());
  if (performance)
    performance->addFirstContentfulPaintTiming(m_firstContentfulPaint);
  TRACE_EVENT_INSTANT1("blink.user_timing,rail", "firstContentfulPaint",
                       TRACE_EVENT_SCOPE_PROCESS, "frame", frame());
}

}  // namespace blink
