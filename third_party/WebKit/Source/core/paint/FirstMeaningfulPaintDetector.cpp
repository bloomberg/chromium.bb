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

FirstMeaningfulPaintDetector& FirstMeaningfulPaintDetector::from(
    Document& document) {
  return PaintTiming::from(document).firstMeaningfulPaintDetector();
}

FirstMeaningfulPaintDetector::FirstMeaningfulPaintDetector(
    PaintTiming* paintTiming,
    Document& document)
    : m_paintTiming(paintTiming),
      m_network0QuietTimer(
          TaskRunnerHelper::get(TaskType::UnspecedTimer, &document),
          this,
          &FirstMeaningfulPaintDetector::network0QuietTimerFired),
      m_network2QuietTimer(
          TaskRunnerHelper::get(TaskType::UnspecedTimer, &document),
          this,
          &FirstMeaningfulPaintDetector::network2QuietTimerFired) {}

Document* FirstMeaningfulPaintDetector::document() {
  return m_paintTiming->supplementable();
}

// Computes "layout significance" (http://goo.gl/rytlPL) of a layout operation.
// Significance of a layout is the number of layout objects newly added to the
// layout tree, weighted by page height (before and after the layout).
// A paint after the most significance layout during page load is reported as
// First Meaningful Paint.
void FirstMeaningfulPaintDetector::markNextPaintAsMeaningfulIfNeeded(
    const LayoutObjectCounter& counter,
    int contentsHeightBeforeLayout,
    int contentsHeightAfterLayout,
    int visibleHeight) {
  if (m_network0QuietReached && m_network2QuietReached)
    return;

  unsigned delta = counter.count() - m_prevLayoutObjectCount;
  m_prevLayoutObjectCount = counter.count();

  if (visibleHeight == 0)
    return;

  double ratioBefore = std::max(
      1.0, static_cast<double>(contentsHeightBeforeLayout) / visibleHeight);
  double ratioAfter = std::max(
      1.0, static_cast<double>(contentsHeightAfterLayout) / visibleHeight);
  double significance = delta / ((ratioBefore + ratioAfter) / 2);

  // If the page has many blank characters, the significance value is
  // accumulated until the text become visible.
  int approximateBlankCharacterCount =
      FontFaceSet::approximateBlankCharacterCount(*document());
  if (approximateBlankCharacterCount > kBlankCharactersThreshold) {
    m_accumulatedSignificanceWhileHavingBlankText += significance;
  } else {
    significance += m_accumulatedSignificanceWhileHavingBlankText;
    m_accumulatedSignificanceWhileHavingBlankText = 0;
    if (significance > m_maxSignificanceSoFar) {
      m_nextPaintIsMeaningful = true;
      m_maxSignificanceSoFar = significance;
    }
  }
}

void FirstMeaningfulPaintDetector::notifyPaint() {
  if (!m_nextPaintIsMeaningful)
    return;

  // Skip document background-only paints.
  if (m_paintTiming->firstPaint() == 0.0)
    return;

  m_provisionalFirstMeaningfulPaint = monotonicallyIncreasingTime();
  m_nextPaintIsMeaningful = false;

  if (m_network2QuietReached)
    return;

  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "loading", "firstMeaningfulPaintCandidate",
      TraceEvent::toTraceTimestamp(m_provisionalFirstMeaningfulPaint), "frame",
      document()->frame());
  // Ignore the first meaningful paint candidate as this generally is the first
  // contentful paint itself.
  if (!m_seenFirstMeaningfulPaintCandidate) {
    m_seenFirstMeaningfulPaintCandidate = true;
    return;
  }
  m_paintTiming->setFirstMeaningfulPaintCandidate(
      m_provisionalFirstMeaningfulPaint);
}

int FirstMeaningfulPaintDetector::activeConnections() {
  DCHECK(document());
  ResourceFetcher* fetcher = document()->fetcher();
  return fetcher->blockingRequestCount() + fetcher->nonblockingRequestCount();
}

// This function is called when the number of active connections is decreased.
void FirstMeaningfulPaintDetector::checkNetworkStable() {
  DCHECK(document());
  if (!document()->hasFinishedParsing())
    return;

  setNetworkQuietTimers(activeConnections());
}

void FirstMeaningfulPaintDetector::setNetworkQuietTimers(
    int activeConnections) {
  if (!m_network0QuietReached && activeConnections == 0) {
    // This restarts 0-quiet timer if it's already running.
    m_network0QuietTimer.startOneShot(kNetwork0QuietWindowSeconds,
                                      BLINK_FROM_HERE);
  }
  if (!m_network2QuietReached && activeConnections <= 2) {
    // If activeConnections < 2 and the timer is already running, current
    // 2-quiet window continues; the timer shouldn't be restarted.
    if (activeConnections == 2 || !m_network2QuietTimer.isActive()) {
      m_network2QuietTimer.startOneShot(kNetwork2QuietWindowSeconds,
                                        BLINK_FROM_HERE);
    }
  }
}

void FirstMeaningfulPaintDetector::network0QuietTimerFired(TimerBase*) {
  if (!document() || m_network0QuietReached || activeConnections() > 0 ||
      !m_paintTiming->firstContentfulPaint())
    return;
  m_network0QuietReached = true;

  if (m_provisionalFirstMeaningfulPaint) {
    // Enforce FirstContentfulPaint <= FirstMeaningfulPaint.
    m_firstMeaningfulPaint0Quiet =
        std::max(m_provisionalFirstMeaningfulPaint,
                 m_paintTiming->firstContentfulPaint());
  }
  reportHistograms();
}

void FirstMeaningfulPaintDetector::network2QuietTimerFired(TimerBase*) {
  if (!document() || m_network2QuietReached || activeConnections() > 2 ||
      !m_paintTiming->firstContentfulPaint())
    return;
  m_network2QuietReached = true;

  if (m_provisionalFirstMeaningfulPaint) {
    // If there's only been one contentful paint, then there won't have been
    // a meaningful paint signalled to the Scheduler, so mark one now.
    // This is a no-op if a FMPC has already been marked.
    m_paintTiming->setFirstMeaningfulPaintCandidate(
        m_provisionalFirstMeaningfulPaint);
    // Enforce FirstContentfulPaint <= FirstMeaningfulPaint.
    m_firstMeaningfulPaint2Quiet =
        std::max(m_provisionalFirstMeaningfulPaint,
                 m_paintTiming->firstContentfulPaint());
    // Report FirstMeaningfulPaint when the page reached network 2-quiet.
    m_paintTiming->setFirstMeaningfulPaint(m_firstMeaningfulPaint2Quiet);
  }
  reportHistograms();
}

void FirstMeaningfulPaintDetector::reportHistograms() {
  // This enum backs an UMA histogram, and should be treated as append-only.
  enum HadNetworkQuiet {
    HadNetwork0Quiet,
    HadNetwork2Quiet,
    HadNetworkQuietEnumMax
  };
  DEFINE_STATIC_LOCAL(EnumerationHistogram, hadNetworkQuietHistogram,
                      ("PageLoad.Experimental.Renderer."
                       "FirstMeaningfulPaintDetector.HadNetworkQuiet",
                       HadNetworkQuietEnumMax));

  // This enum backs an UMA histogram, and should be treated as append-only.
  enum FMPOrderingEnum {
    FMP0QuietFirst,
    FMP2QuietFirst,
    FMP0QuietEqualFMP2Quiet,
    FMPOrderingEnumMax
  };
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, firstMeaningfulPaintOrderingHistogram,
      ("PageLoad.Experimental.Renderer.FirstMeaningfulPaintDetector."
       "FirstMeaningfulPaintOrdering",
       FMPOrderingEnumMax));

  if (m_firstMeaningfulPaint0Quiet && m_firstMeaningfulPaint2Quiet) {
    int sample;
    if (m_firstMeaningfulPaint2Quiet < m_firstMeaningfulPaint0Quiet) {
      sample = FMP0QuietFirst;
    } else if (m_firstMeaningfulPaint2Quiet > m_firstMeaningfulPaint0Quiet) {
      sample = FMP2QuietFirst;
    } else {
      sample = FMP0QuietEqualFMP2Quiet;
    }
    firstMeaningfulPaintOrderingHistogram.count(sample);
  } else if (m_firstMeaningfulPaint0Quiet) {
    hadNetworkQuietHistogram.count(HadNetwork0Quiet);
  } else if (m_firstMeaningfulPaint2Quiet) {
    hadNetworkQuietHistogram.count(HadNetwork2Quiet);
  }
}

DEFINE_TRACE(FirstMeaningfulPaintDetector) {
  visitor->trace(m_paintTiming);
}

}  // namespace blink
