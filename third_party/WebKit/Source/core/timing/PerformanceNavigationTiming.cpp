// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformanceNavigationTiming.h"

#include "bindings/core/v8/V8ObjectBuilder.h"
#include "core/timing/PerformanceBase.h"

namespace blink {

PerformanceNavigationTiming::PerformanceNavigationTiming(
    double timeOrigin,
    const String& requestedUrl,
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
    bool allowRedirectDetails,
    bool hasSameOriginAsPreviousDocument,
    ResourceLoadTiming* timing,
    double lastRedirectEndTime,
    double finishTime,
    unsigned long long transferSize,
    unsigned long long encodedBodyLength,
    unsigned long long decodedBodyLength,
    bool didReuseConnection)
    : PerformanceResourceTiming("navigation",
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
                                allowRedirectDetails,
                                requestedUrl,
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
      m_allowRedirectDetails(allowRedirectDetails),
      m_hasSameOriginAsPreviousDocument(hasSameOriginAsPreviousDocument) {}

PerformanceNavigationTiming::~PerformanceNavigationTiming() {}

DOMHighResTimeStamp PerformanceNavigationTiming::unloadEventStart() const {
  if (!m_allowRedirectDetails || !m_hasSameOriginAsPreviousDocument)
    return 0;
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, m_unloadEventStart);
}

DOMHighResTimeStamp PerformanceNavigationTiming::unloadEventEnd() const {
  if (!m_allowRedirectDetails || !m_hasSameOriginAsPreviousDocument)
    return 0;

  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(m_timeOrigin,
                                                             m_unloadEventEnd);
}

DOMHighResTimeStamp PerformanceNavigationTiming::domInteractive() const {
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(m_timeOrigin,
                                                             m_domInteractive);
}

DOMHighResTimeStamp PerformanceNavigationTiming::domContentLoadedEventStart()
    const {
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, m_domContentLoadedEventStart);
}

DOMHighResTimeStamp PerformanceNavigationTiming::domContentLoadedEventEnd()
    const {
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, m_domContentLoadedEventEnd);
}

DOMHighResTimeStamp PerformanceNavigationTiming::domComplete() const {
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(m_timeOrigin,
                                                             m_domComplete);
}

DOMHighResTimeStamp PerformanceNavigationTiming::loadEventStart() const {
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(m_timeOrigin,
                                                             m_loadEventStart);
}

DOMHighResTimeStamp PerformanceNavigationTiming::loadEventEnd() const {
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(m_timeOrigin,
                                                             m_loadEventEnd);
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
  if (!m_allowRedirectDetails)
    return 0;
  return m_redirectCount;
}

DOMHighResTimeStamp PerformanceNavigationTiming::fetchStart() const {
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(m_timeOrigin,
                                                             m_fetchStart);
}

DOMHighResTimeStamp PerformanceNavigationTiming::redirectStart() const {
  if (!m_allowRedirectDetails)
    return 0;
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(m_timeOrigin,
                                                             m_redirectStart);
}

DOMHighResTimeStamp PerformanceNavigationTiming::redirectEnd() const {
  if (!m_allowRedirectDetails)
    return 0;
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(m_timeOrigin,
                                                             m_redirectEnd);
}

DOMHighResTimeStamp PerformanceNavigationTiming::responseEnd() const {
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(m_timeOrigin,
                                                             m_responseEnd);
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
