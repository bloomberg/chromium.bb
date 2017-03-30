// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformanceNavigationTiming.h"

#include "bindings/core/v8/V8ObjectBuilder.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentTiming.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "core/timing/PerformanceBase.h"
#include "platform/loader/fetch/ResourceTimingInfo.h"

namespace blink {

PerformanceNavigationTiming::PerformanceNavigationTiming(
    LocalFrame* frame,
    ResourceTimingInfo* info,
    double timeOrigin)
    : PerformanceResourceTiming(info ? info->initialURL().getString() : "",
                                "navigation",
                                0.0,
                                0.0),
      ContextClient(frame),
      m_timeOrigin(timeOrigin),
      m_resourceTimingInfo(info) {
  DCHECK(frame);
  DCHECK(info);
}

PerformanceNavigationTiming::~PerformanceNavigationTiming() {}

DEFINE_TRACE(PerformanceNavigationTiming) {
  ContextClient::trace(visitor);
  PerformanceEntry::trace(visitor);
}

DocumentLoadTiming* PerformanceNavigationTiming::documentLoadTiming() const {
  DocumentLoader* loader = documentLoader();
  if (!loader)
    return nullptr;

  return &loader->timing();
}

DocumentLoader* PerformanceNavigationTiming::documentLoader() const {
  if (!frame())
    return nullptr;
  return frame()->loader().documentLoader();
}

const DocumentTiming* PerformanceNavigationTiming::documentTiming() const {
  if (!frame())
    return nullptr;
  Document* document = frame()->document();
  if (!document)
    return nullptr;

  return &document->timing();
}

ResourceLoadTiming* PerformanceNavigationTiming::resourceLoadTiming() const {
  return m_resourceTimingInfo->finalResponse().resourceLoadTiming();
}

bool PerformanceNavigationTiming::allowTimingDetails() const {
  return true;
}

bool PerformanceNavigationTiming::didReuseConnection() const {
  return m_resourceTimingInfo->finalResponse().connectionReused();
}

unsigned long long PerformanceNavigationTiming::getTransferSize() const {
  return m_resourceTimingInfo->transferSize();
}

unsigned long long PerformanceNavigationTiming::getEncodedBodySize() const {
  return m_resourceTimingInfo->finalResponse().encodedBodyLength();
}

unsigned long long PerformanceNavigationTiming::getDecodedBodySize() const {
  return m_resourceTimingInfo->finalResponse().decodedBodyLength();
}

AtomicString PerformanceNavigationTiming::getNavigationType(
    NavigationType type,
    const Document* document) {
  if (document &&
      document->pageVisibilityState() == PageVisibilityStatePrerender) {
    return "prerender";
  }
  switch (type) {
    case NavigationTypeReload:
      return "reload";
    case NavigationTypeBackForward:
      return "back_forward";
    case NavigationTypeLinkClicked:
    case NavigationTypeFormSubmitted:
    case NavigationTypeFormResubmitted:
    case NavigationTypeOther:
      return "navigate";
  }
  NOTREACHED();
  return "navigate";
}

AtomicString PerformanceNavigationTiming::initiatorType() const {
  return "navigation";
}

bool PerformanceNavigationTiming::getAllowRedirectDetails() const {
  ExecutionContext* context = frame() ? frame()->document() : nullptr;
  SecurityOrigin* securityOrigin = nullptr;
  if (context)
    securityOrigin = context->getSecurityOrigin();
  if (!securityOrigin)
    return false;
  // TODO(sunjian): Think about how to make this flag deterministic.
  // crbug/693183.
  return PerformanceBase::allowsTimingRedirect(
      m_resourceTimingInfo->redirectChain(),
      m_resourceTimingInfo->finalResponse(), *securityOrigin, context);
}

DOMHighResTimeStamp PerformanceNavigationTiming::unloadEventStart() const {
  bool allowRedirectDetails = getAllowRedirectDetails();
  DocumentLoadTiming* timing = documentLoadTiming();

  if (!allowRedirectDetails || !timing ||
      !timing->hasSameOriginAsPreviousDocument())
    return 0;
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, timing->unloadEventStart());
}

DOMHighResTimeStamp PerformanceNavigationTiming::unloadEventEnd() const {
  bool allowRedirectDetails = getAllowRedirectDetails();
  DocumentLoadTiming* timing = documentLoadTiming();

  if (!allowRedirectDetails || !timing ||
      !timing->hasSameOriginAsPreviousDocument())
    return 0;
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, timing->unloadEventEnd());
}

DOMHighResTimeStamp PerformanceNavigationTiming::domInteractive() const {
  const DocumentTiming* timing = documentTiming();
  if (!timing)
    return 0.0;
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, timing->domInteractive());
}

DOMHighResTimeStamp PerformanceNavigationTiming::domContentLoadedEventStart()
    const {
  const DocumentTiming* timing = documentTiming();
  if (!timing)
    return 0.0;
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, timing->domContentLoadedEventStart());
}

DOMHighResTimeStamp PerformanceNavigationTiming::domContentLoadedEventEnd()
    const {
  const DocumentTiming* timing = documentTiming();
  if (!timing)
    return 0.0;
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, timing->domContentLoadedEventEnd());
}

DOMHighResTimeStamp PerformanceNavigationTiming::domComplete() const {
  const DocumentTiming* timing = documentTiming();
  if (!timing)
    return 0.0;
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, timing->domComplete());
}

DOMHighResTimeStamp PerformanceNavigationTiming::loadEventStart() const {
  DocumentLoadTiming* timing = documentLoadTiming();
  if (!timing)
    return 0.0;
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, timing->loadEventStart());
}

DOMHighResTimeStamp PerformanceNavigationTiming::loadEventEnd() const {
  DocumentLoadTiming* timing = documentLoadTiming();
  if (!timing)
    return 0.0;
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, timing->loadEventEnd());
}

AtomicString PerformanceNavigationTiming::type() const {
  DocumentLoader* loader = documentLoader();
  if (frame() && loader)
    return getNavigationType(loader->getNavigationType(), frame()->document());
  return "navigate";
}

unsigned short PerformanceNavigationTiming::redirectCount() const {
  bool allowRedirectDetails = getAllowRedirectDetails();
  DocumentLoadTiming* timing = documentLoadTiming();
  if (!allowRedirectDetails || !timing)
    return 0;
  return timing->redirectCount();
}

DOMHighResTimeStamp PerformanceNavigationTiming::redirectStart() const {
  bool allowRedirectDetails = getAllowRedirectDetails();
  DocumentLoadTiming* timing = documentLoadTiming();
  if (!allowRedirectDetails || !timing)
    return 0;
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, timing->redirectStart());
}

DOMHighResTimeStamp PerformanceNavigationTiming::redirectEnd() const {
  bool allowRedirectDetails = getAllowRedirectDetails();
  DocumentLoadTiming* timing = documentLoadTiming();
  if (!allowRedirectDetails || !timing)
    return 0;
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, timing->redirectEnd());
}

DOMHighResTimeStamp PerformanceNavigationTiming::fetchStart() const {
  DocumentLoadTiming* timing = documentLoadTiming();
  if (!timing)
    return 0.0;
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, timing->fetchStart());
}

DOMHighResTimeStamp PerformanceNavigationTiming::responseEnd() const {
  DocumentLoadTiming* timing = documentLoadTiming();
  if (!timing)
    return 0.0;
  return PerformanceBase::monotonicTimeToDOMHighResTimeStamp(
      m_timeOrigin, timing->responseEnd());
}

// Overriding PerformanceEntry's attributes.
DOMHighResTimeStamp PerformanceNavigationTiming::duration() const {
  return loadEventEnd();
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
