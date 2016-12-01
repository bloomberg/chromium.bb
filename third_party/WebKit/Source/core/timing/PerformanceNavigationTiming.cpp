// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformanceNavigationTiming.h"

#include "bindings/core/v8/V8ObjectBuilder.h"
#include "core/timing/PerformanceBase.h"

namespace blink {

// TODO(sunjian): Move this logic into PerformanceBase
static double monotonicTimeToDOMHighResTimeStamp(double timeOrigin,
                                                 double seconds) {
  DCHECK(seconds >= 0.0);
  if (!seconds || !timeOrigin)
    return 0.0;
  if (seconds < timeOrigin)
    return 0.0;
  return PerformanceBase::clampTimeResolution(seconds - timeOrigin) * 1000.0;
}

PerformanceNavigationTiming::PerformanceNavigationTiming(
    double timeOrigin,
    double unloadEventStart,
    double unloadEventEnd,
    double loadEventStart,
    double loadEventEnd,
    unsigned short redirectCount,
    double domInteractive,
    double domContentLoadedEventStart,
    double domContentLoadedEventEnd,
    double domComplete,
    NavigationType type,
    double redirectStart,
    double redirectEnd,
    double fetchStart,
    double responseEnd,
    bool hasCrossOriginRedirect,
    bool hasSameOriginAsPreviousDocument,
    ResourceLoadTiming* timing,
    double lastRedirectEndTime,
    double finishTime,
    unsigned long long transferSize,
    unsigned long long encodedBodyLength,
    unsigned long long decodedBodyLength,
    bool didReuseConnection)
    : PerformanceResourceTiming("",
                                timeOrigin,
                                timing,
                                lastRedirectEndTime,
                                finishTime,
                                transferSize,
                                encodedBodyLength,
                                decodedBodyLength,
                                didReuseConnection,
                                true /*allowTimingDetails*/,  // TODO(sunjian):
                                                              // Create an enum
                                                              // for this.
                                !hasCrossOriginRedirect,
                                "document",
                                "navigation",
                                timeOrigin),
      m_timeOrigin(timeOrigin),
      m_unloadEventStart(unloadEventStart),
      m_unloadEventEnd(unloadEventEnd),
      m_loadEventStart(loadEventStart),
      m_loadEventEnd(loadEventEnd),
      m_redirectCount(redirectCount),
      m_domInteractive(domInteractive),
      m_domContentLoadedEventStart(domContentLoadedEventStart),
      m_domContentLoadedEventEnd(domContentLoadedEventEnd),
      m_domComplete(domComplete),
      m_type(type),
      m_redirectStart(redirectStart),
      m_redirectEnd(redirectEnd),
      m_fetchStart(fetchStart),
      m_responseEnd(responseEnd),
      m_hasCrossOriginRedirect(hasCrossOriginRedirect),
      m_hasSameOriginAsPreviousDocument(hasSameOriginAsPreviousDocument) {}

PerformanceNavigationTiming::~PerformanceNavigationTiming() {}

double PerformanceNavigationTiming::unloadEventStart() const {
  if (m_hasCrossOriginRedirect || !m_hasSameOriginAsPreviousDocument)
    return 0;
  return monotonicTimeToDOMHighResTimeStamp(m_timeOrigin, m_unloadEventStart);
}

double PerformanceNavigationTiming::unloadEventEnd() const {
  if (m_hasCrossOriginRedirect || !m_hasSameOriginAsPreviousDocument)
    return 0;

  return monotonicTimeToDOMHighResTimeStamp(m_timeOrigin, m_unloadEventEnd);
}

double PerformanceNavigationTiming::domInteractive() const {
  return monotonicTimeToDOMHighResTimeStamp(m_timeOrigin, m_domInteractive);
}

double PerformanceNavigationTiming::domContentLoadedEventStart() const {
  return monotonicTimeToDOMHighResTimeStamp(m_timeOrigin,
                                            m_domContentLoadedEventStart);
}

double PerformanceNavigationTiming::domContentLoadedEventEnd() const {
  return monotonicTimeToDOMHighResTimeStamp(m_timeOrigin,
                                            m_domContentLoadedEventEnd);
}

double PerformanceNavigationTiming::domComplete() const {
  return monotonicTimeToDOMHighResTimeStamp(m_timeOrigin, m_domComplete);
}

double PerformanceNavigationTiming::loadEventStart() const {
  return monotonicTimeToDOMHighResTimeStamp(m_timeOrigin, m_loadEventStart);
}

double PerformanceNavigationTiming::loadEventEnd() const {
  return monotonicTimeToDOMHighResTimeStamp(m_timeOrigin, m_loadEventEnd);
}

AtomicString PerformanceNavigationTiming::type() const {
  switch (m_type) {
    case NavigationType::Reload:
      return "reload";
    case NavigationType::BackForward:
      return "back_forward";
    case NavigationType::Prerender:
      return "prerender";
    case NavigationType::Navigate:
      return "navigate";
  }
  NOTREACHED();
  return "navigate";
}

unsigned short PerformanceNavigationTiming::redirectCount() const {
  // TODO(sunjian): Also check response headers to allow opt-in crbugs/665160
  if (m_hasCrossOriginRedirect)
    return 0;
  return m_redirectCount;
}

double PerformanceNavigationTiming::fetchStart() const {
  return monotonicTimeToDOMHighResTimeStamp(m_timeOrigin, m_fetchStart);
}

double PerformanceNavigationTiming::redirectStart() const {
  if (m_hasCrossOriginRedirect)
    return 0;
  return monotonicTimeToDOMHighResTimeStamp(m_timeOrigin, m_redirectStart);
}

double PerformanceNavigationTiming::redirectEnd() const {
  if (m_hasCrossOriginRedirect)
    return 0;
  return monotonicTimeToDOMHighResTimeStamp(m_timeOrigin, m_redirectEnd);
}

double PerformanceNavigationTiming::responseEnd() const {
  return monotonicTimeToDOMHighResTimeStamp(m_timeOrigin, m_responseEnd);
}

void PerformanceNavigationTiming::buildJSONValue(
    V8ObjectBuilder& builder) const {
  PerformanceResourceTiming::buildJSONValue(builder);
  builder.addNumber("unloadEventStart", unloadEventStart());
  builder.addNumber("unloadEventEnd", unloadEventEnd());
  builder.addNumber("domInteractive", domInteractive());
  builder.addNumber("domContentLoadedEventStart", domContentLoadedEventStart());
  builder.addNumber("domContentLoadedEventEnd", domContentLoadedEventEnd());
  builder.addNumber("domComplete", domComplete());
  builder.addNumber("loadEventStart", loadEventStart());
  builder.addNumber("loadEventEnd", loadEventEnd());
  builder.addString("type", type());
  builder.addNumber("redirectCount", redirectCount());
}
}
