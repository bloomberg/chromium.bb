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
    double time_origin,
    PerformanceServerTimingVector& serverTiming)
    : PerformanceResourceTiming(info ? info->InitialURL().GetString() : "",
                                "navigation",
                                time_origin,
                                0.0,
                                0.0,
                                serverTiming),
      ContextClient(frame),
      resource_timing_info_(info) {
  DCHECK(frame);
  DCHECK(info);
}

PerformanceNavigationTiming::~PerformanceNavigationTiming() {}

void PerformanceNavigationTiming::Trace(blink::Visitor* visitor) {
  ContextClient::Trace(visitor);
  PerformanceResourceTiming::Trace(visitor);
}

DocumentLoadTiming* PerformanceNavigationTiming::GetDocumentLoadTiming() const {
  DocumentLoader* loader = GetDocumentLoader();
  if (!loader)
    return nullptr;

  return &loader->GetTiming();
}

DocumentLoader* PerformanceNavigationTiming::GetDocumentLoader() const {
  if (!GetFrame())
    return nullptr;
  return GetFrame()->Loader().GetDocumentLoader();
}

const DocumentTiming* PerformanceNavigationTiming::GetDocumentTiming() const {
  if (!GetFrame())
    return nullptr;
  Document* document = GetFrame()->GetDocument();
  if (!document)
    return nullptr;

  return &document->GetTiming();
}

ResourceLoadTiming* PerformanceNavigationTiming::GetResourceLoadTiming() const {
  return resource_timing_info_->FinalResponse().GetResourceLoadTiming();
}

bool PerformanceNavigationTiming::AllowTimingDetails() const {
  return true;
}

bool PerformanceNavigationTiming::DidReuseConnection() const {
  return resource_timing_info_->FinalResponse().ConnectionReused();
}

unsigned long long PerformanceNavigationTiming::GetTransferSize() const {
  return resource_timing_info_->TransferSize();
}

unsigned long long PerformanceNavigationTiming::GetEncodedBodySize() const {
  return resource_timing_info_->FinalResponse().EncodedBodyLength();
}

unsigned long long PerformanceNavigationTiming::GetDecodedBodySize() const {
  return resource_timing_info_->FinalResponse().DecodedBodyLength();
}

AtomicString PerformanceNavigationTiming::GetNavigationType(
    NavigationType type,
    const Document* document) {
  if (document &&
      document->GetPageVisibilityState() == kPageVisibilityStatePrerender) {
    return "prerender";
  }
  switch (type) {
    case kNavigationTypeReload:
      return "reload";
    case kNavigationTypeBackForward:
      return "back_forward";
    case kNavigationTypeLinkClicked:
    case kNavigationTypeFormSubmitted:
    case kNavigationTypeFormResubmitted:
    case kNavigationTypeOther:
      return "navigate";
  }
  NOTREACHED();
  return "navigate";
}

AtomicString PerformanceNavigationTiming::initiatorType() const {
  return "navigation";
}

bool PerformanceNavigationTiming::GetAllowRedirectDetails() const {
  ExecutionContext* context = GetFrame() ? GetFrame()->GetDocument() : nullptr;
  SecurityOrigin* security_origin = nullptr;
  if (context)
    security_origin = context->GetSecurityOrigin();
  if (!security_origin)
    return false;
  // TODO(sunjian): Think about how to make this flag deterministic.
  // crbug/693183.
  return PerformanceBase::AllowsTimingRedirect(
      resource_timing_info_->RedirectChain(),
      resource_timing_info_->FinalResponse(), *security_origin, context);
}

AtomicString PerformanceNavigationTiming::AlpnNegotiatedProtocol() const {
  return resource_timing_info_->FinalResponse().AlpnNegotiatedProtocol();
}

AtomicString PerformanceNavigationTiming::ConnectionInfo() const {
  return resource_timing_info_->FinalResponse().ConnectionInfoString();
}

DOMHighResTimeStamp PerformanceNavigationTiming::unloadEventStart() const {
  bool allow_redirect_details = GetAllowRedirectDetails();
  DocumentLoadTiming* timing = GetDocumentLoadTiming();

  if (!allow_redirect_details || !timing ||
      !timing->HasSameOriginAsPreviousDocument())
    return 0;
  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->UnloadEventStart(),
      false /* allow_negative_value */);
}

DOMHighResTimeStamp PerformanceNavigationTiming::unloadEventEnd() const {
  bool allow_redirect_details = GetAllowRedirectDetails();
  DocumentLoadTiming* timing = GetDocumentLoadTiming();

  if (!allow_redirect_details || !timing ||
      !timing->HasSameOriginAsPreviousDocument())
    return 0;
  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->UnloadEventEnd(), false /* allow_negative_value */);
}

DOMHighResTimeStamp PerformanceNavigationTiming::domInteractive() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0.0;
  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomInteractive(), false /* allow_negative_value */);
}

DOMHighResTimeStamp PerformanceNavigationTiming::domContentLoadedEventStart()
    const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0.0;
  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomContentLoadedEventStart(),
      false /* allow_negative_value */);
}

DOMHighResTimeStamp PerformanceNavigationTiming::domContentLoadedEventEnd()
    const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0.0;
  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomContentLoadedEventEnd(),
      false /* allow_negative_value */);
}

DOMHighResTimeStamp PerformanceNavigationTiming::domComplete() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0.0;
  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->DomComplete(), false /* allow_negative_value */);
}

DOMHighResTimeStamp PerformanceNavigationTiming::loadEventStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0.0;
  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->LoadEventStart(), false /* allow_negative_value */);
}

DOMHighResTimeStamp PerformanceNavigationTiming::loadEventEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0.0;
  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->LoadEventEnd(), false /* allow_negative_value */);
}

AtomicString PerformanceNavigationTiming::type() const {
  DocumentLoader* loader = GetDocumentLoader();
  if (GetFrame() && loader)
    return GetNavigationType(loader->GetNavigationType(),
                             GetFrame()->GetDocument());
  return "navigate";
}

unsigned short PerformanceNavigationTiming::redirectCount() const {
  bool allow_redirect_details = GetAllowRedirectDetails();
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!allow_redirect_details || !timing)
    return 0;
  return timing->RedirectCount();
}

DOMHighResTimeStamp PerformanceNavigationTiming::redirectStart() const {
  bool allow_redirect_details = GetAllowRedirectDetails();
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!allow_redirect_details || !timing)
    return 0;
  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->RedirectStart(), false /* allow_negative_value */);
}

DOMHighResTimeStamp PerformanceNavigationTiming::redirectEnd() const {
  bool allow_redirect_details = GetAllowRedirectDetails();
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!allow_redirect_details || !timing)
    return 0;
  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->RedirectEnd(), false /* allow_negative_value */);
}

DOMHighResTimeStamp PerformanceNavigationTiming::fetchStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0.0;
  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->FetchStart(), false /* allow_negative_value */);
}

DOMHighResTimeStamp PerformanceNavigationTiming::responseEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0.0;
  return PerformanceBase::MonotonicTimeToDOMHighResTimeStamp(
      TimeOrigin(), timing->ResponseEnd(), false /* allow_negative_value */);
}

// Overriding PerformanceEntry's attributes.
DOMHighResTimeStamp PerformanceNavigationTiming::duration() const {
  return loadEventEnd();
}

void PerformanceNavigationTiming::BuildJSONValue(
    ScriptState* script_state,
    V8ObjectBuilder& builder) const {
  PerformanceResourceTiming::BuildJSONValue(script_state, builder);
  builder.AddNumber("unloadEventStart", unloadEventStart());
  builder.AddNumber("unloadEventEnd", unloadEventEnd());
  builder.AddNumber("domInteractive", domInteractive());
  builder.AddNumber("domContentLoadedEventStart", domContentLoadedEventStart());
  builder.AddNumber("domContentLoadedEventEnd", domContentLoadedEventEnd());
  builder.AddNumber("domComplete", domComplete());
  builder.AddNumber("loadEventStart", loadEventStart());
  builder.AddNumber("loadEventEnd", loadEventEnd());
  builder.AddString("type", type());
  builder.AddNumber("redirectCount", redirectCount());
}
}
