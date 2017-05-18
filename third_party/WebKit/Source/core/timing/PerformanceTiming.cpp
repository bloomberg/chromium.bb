/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/timing/PerformanceTiming.h"

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8ObjectBuilder.h"
#include "core/css/CSSTiming.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentParserTiming.h"
#include "core/dom/DocumentTiming.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/paint/PaintTiming.h"
#include "core/timing/PerformanceBase.h"
#include "platform/loader/fetch/ResourceLoadTiming.h"
#include "platform/loader/fetch/ResourceResponse.h"

// Legacy support for NT1(https://www.w3.org/TR/navigation-timing/).
namespace blink {

static unsigned long long ToIntegerMilliseconds(double seconds) {
  DCHECK_GE(seconds, 0);
  double clamped_seconds = PerformanceBase::ClampTimeResolution(seconds);
  return static_cast<unsigned long long>(clamped_seconds * 1000.0);
}

static double ToDoubleSeconds(unsigned long long integer_milliseconds) {
  return integer_milliseconds / 1000.0;
}

PerformanceTiming::PerformanceTiming(LocalFrame* frame)
    : DOMWindowClient(frame) {}

unsigned long long PerformanceTiming::navigationStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->NavigationStart());
}

unsigned long long PerformanceTiming::unloadEventStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  if (timing->HasCrossOriginRedirect() ||
      !timing->HasSameOriginAsPreviousDocument())
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->UnloadEventStart());
}

unsigned long long PerformanceTiming::unloadEventEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  if (timing->HasCrossOriginRedirect() ||
      !timing->HasSameOriginAsPreviousDocument())
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->UnloadEventEnd());
}

unsigned long long PerformanceTiming::redirectStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  if (timing->HasCrossOriginRedirect())
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->RedirectStart());
}

unsigned long long PerformanceTiming::redirectEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  if (timing->HasCrossOriginRedirect())
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->RedirectEnd());
}

unsigned long long PerformanceTiming::fetchStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FetchStart());
}

unsigned long long PerformanceTiming::domainLookupStart() const {
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing)
    return fetchStart();

  // This will be zero when a DNS request is not performed.  Rather than
  // exposing a special value that indicates no DNS, we "backfill" with
  // fetchStart.
  double dns_start = timing->DnsStart();
  if (dns_start == 0.0)
    return fetchStart();

  return MonotonicTimeToIntegerMilliseconds(dns_start);
}

unsigned long long PerformanceTiming::domainLookupEnd() const {
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing)
    return domainLookupStart();

  // This will be zero when a DNS request is not performed.  Rather than
  // exposing a special value that indicates no DNS, we "backfill" with
  // domainLookupStart.
  double dns_end = timing->DnsEnd();
  if (dns_end == 0.0)
    return domainLookupStart();

  return MonotonicTimeToIntegerMilliseconds(dns_end);
}

unsigned long long PerformanceTiming::connectStart() const {
  DocumentLoader* loader = GetDocumentLoader();
  if (!loader)
    return domainLookupEnd();

  ResourceLoadTiming* timing = loader->GetResponse().GetResourceLoadTiming();
  if (!timing)
    return domainLookupEnd();

  // connectStart will be zero when a network request is not made.  Rather than
  // exposing a special value that indicates no new connection, we "backfill"
  // with domainLookupEnd.
  double connect_start = timing->ConnectStart();
  if (connect_start == 0.0 || loader->GetResponse().ConnectionReused())
    return domainLookupEnd();

  // ResourceLoadTiming's connect phase includes DNS, however Navigation
  // Timing's connect phase should not. So if there is DNS time, trim it from
  // the start.
  if (timing->DnsEnd() > 0.0 && timing->DnsEnd() > connect_start)
    connect_start = timing->DnsEnd();

  return MonotonicTimeToIntegerMilliseconds(connect_start);
}

unsigned long long PerformanceTiming::connectEnd() const {
  DocumentLoader* loader = GetDocumentLoader();
  if (!loader)
    return connectStart();

  ResourceLoadTiming* timing = loader->GetResponse().GetResourceLoadTiming();
  if (!timing)
    return connectStart();

  // connectEnd will be zero when a network request is not made.  Rather than
  // exposing a special value that indicates no new connection, we "backfill"
  // with connectStart.
  double connect_end = timing->ConnectEnd();
  if (connect_end == 0.0 || loader->GetResponse().ConnectionReused())
    return connectStart();

  return MonotonicTimeToIntegerMilliseconds(connect_end);
}

unsigned long long PerformanceTiming::secureConnectionStart() const {
  DocumentLoader* loader = GetDocumentLoader();
  if (!loader)
    return 0;

  ResourceLoadTiming* timing = loader->GetResponse().GetResourceLoadTiming();
  if (!timing)
    return 0;

  double ssl_start = timing->SslStart();
  if (ssl_start == 0.0)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(ssl_start);
}

unsigned long long PerformanceTiming::requestStart() const {
  ResourceLoadTiming* timing = GetResourceLoadTiming();

  if (!timing || timing->SendStart() == 0.0)
    return connectEnd();

  return MonotonicTimeToIntegerMilliseconds(timing->SendStart());
}

unsigned long long PerformanceTiming::responseStart() const {
  ResourceLoadTiming* timing = GetResourceLoadTiming();
  if (!timing || timing->ReceiveHeadersEnd() == 0.0)
    return requestStart();

  // FIXME: Response start needs to be the time of the first received byte.
  // However, the ResourceLoadTiming API currently only supports the time
  // the last header byte was received. For many responses with reasonable
  // sized cookies, the HTTP headers fit into a single packet so this time
  // is basically equivalent. But for some responses, particularly those with
  // headers larger than a single packet, this time will be too late.
  return MonotonicTimeToIntegerMilliseconds(timing->ReceiveHeadersEnd());
}

unsigned long long PerformanceTiming::responseEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->ResponseEnd());
}

unsigned long long PerformanceTiming::domLoading() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return fetchStart();

  return MonotonicTimeToIntegerMilliseconds(timing->DomLoading());
}

unsigned long long PerformanceTiming::domInteractive() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->DomInteractive());
}

unsigned long long PerformanceTiming::domContentLoadedEventStart() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(
      timing->DomContentLoadedEventStart());
}

unsigned long long PerformanceTiming::domContentLoadedEventEnd() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->DomContentLoadedEventEnd());
}

unsigned long long PerformanceTiming::domComplete() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->DomComplete());
}

unsigned long long PerformanceTiming::loadEventStart() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->LoadEventStart());
}

unsigned long long PerformanceTiming::loadEventEnd() const {
  DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->LoadEventEnd());
}

unsigned long long PerformanceTiming::FirstLayout() const {
  const DocumentTiming* timing = GetDocumentTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FirstLayout());
}

unsigned long long PerformanceTiming::FirstPaint() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FirstPaint());
}

unsigned long long PerformanceTiming::FirstTextPaint() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FirstTextPaint());
}

unsigned long long PerformanceTiming::FirstImagePaint() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FirstImagePaint());
}

unsigned long long PerformanceTiming::FirstContentfulPaint() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FirstContentfulPaint());
}

unsigned long long PerformanceTiming::FirstMeaningfulPaint() const {
  const PaintTiming* timing = GetPaintTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->FirstMeaningfulPaint());
}

unsigned long long PerformanceTiming::ParseStart() const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->ParserStart());
}

unsigned long long PerformanceTiming::ParseStop() const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return MonotonicTimeToIntegerMilliseconds(timing->ParserStop());
}

unsigned long long PerformanceTiming::ParseBlockedOnScriptLoadDuration() const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(timing->ParserBlockedOnScriptLoadDuration());
}

unsigned long long
PerformanceTiming::ParseBlockedOnScriptLoadFromDocumentWriteDuration() const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(
      timing->ParserBlockedOnScriptLoadFromDocumentWriteDuration());
}

unsigned long long PerformanceTiming::ParseBlockedOnScriptExecutionDuration()
    const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(
      timing->ParserBlockedOnScriptExecutionDuration());
}

unsigned long long
PerformanceTiming::ParseBlockedOnScriptExecutionFromDocumentWriteDuration()
    const {
  const DocumentParserTiming* timing = GetDocumentParserTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(
      timing->ParserBlockedOnScriptExecutionFromDocumentWriteDuration());
}

unsigned long long PerformanceTiming::AuthorStyleSheetParseDurationBeforeFCP()
    const {
  const CSSTiming* timing = CssTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(
      timing->AuthorStyleSheetParseDurationBeforeFCP());
}

unsigned long long PerformanceTiming::UpdateStyleDurationBeforeFCP() const {
  const CSSTiming* timing = CssTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(timing->UpdateDurationBeforeFCP());
}

DocumentLoader* PerformanceTiming::GetDocumentLoader() const {
  if (!GetFrame())
    return nullptr;

  return GetFrame()->Loader().GetDocumentLoader();
}

const DocumentTiming* PerformanceTiming::GetDocumentTiming() const {
  if (!GetFrame())
    return nullptr;

  Document* document = GetFrame()->GetDocument();
  if (!document)
    return nullptr;

  return &document->GetTiming();
}

const PaintTiming* PerformanceTiming::GetPaintTiming() const {
  if (!GetFrame())
    return nullptr;

  Document* document = GetFrame()->GetDocument();
  if (!document)
    return nullptr;

  return &PaintTiming::From(*document);
}

const CSSTiming* PerformanceTiming::CssTiming() const {
  if (!GetFrame())
    return nullptr;

  Document* document = GetFrame()->GetDocument();
  if (!document)
    return nullptr;

  return &CSSTiming::From(*document);
}

const DocumentParserTiming* PerformanceTiming::GetDocumentParserTiming() const {
  if (!GetFrame())
    return nullptr;

  Document* document = GetFrame()->GetDocument();
  if (!document)
    return nullptr;

  return &DocumentParserTiming::From(*document);
}

DocumentLoadTiming* PerformanceTiming::GetDocumentLoadTiming() const {
  DocumentLoader* loader = GetDocumentLoader();
  if (!loader)
    return nullptr;

  return &loader->GetTiming();
}

ResourceLoadTiming* PerformanceTiming::GetResourceLoadTiming() const {
  DocumentLoader* loader = GetDocumentLoader();
  if (!loader)
    return nullptr;

  return loader->GetResponse().GetResourceLoadTiming();
}

ScriptValue PerformanceTiming::toJSONForBinding(
    ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  result.AddNumber("navigationStart", navigationStart());
  result.AddNumber("unloadEventStart", unloadEventStart());
  result.AddNumber("unloadEventEnd", unloadEventEnd());
  result.AddNumber("redirectStart", redirectStart());
  result.AddNumber("redirectEnd", redirectEnd());
  result.AddNumber("fetchStart", fetchStart());
  result.AddNumber("domainLookupStart", domainLookupStart());
  result.AddNumber("domainLookupEnd", domainLookupEnd());
  result.AddNumber("connectStart", connectStart());
  result.AddNumber("connectEnd", connectEnd());
  result.AddNumber("secureConnectionStart", secureConnectionStart());
  result.AddNumber("requestStart", requestStart());
  result.AddNumber("responseStart", responseStart());
  result.AddNumber("responseEnd", responseEnd());
  result.AddNumber("domLoading", domLoading());
  result.AddNumber("domInteractive", domInteractive());
  result.AddNumber("domContentLoadedEventStart", domContentLoadedEventStart());
  result.AddNumber("domContentLoadedEventEnd", domContentLoadedEventEnd());
  result.AddNumber("domComplete", domComplete());
  result.AddNumber("loadEventStart", loadEventStart());
  result.AddNumber("loadEventEnd", loadEventEnd());
  return result.GetScriptValue();
}

unsigned long long PerformanceTiming::MonotonicTimeToIntegerMilliseconds(
    double monotonic_seconds) const {
  DCHECK_GE(monotonic_seconds, 0);
  const DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return ToIntegerMilliseconds(
      timing->MonotonicTimeToPseudoWallTime(monotonic_seconds));
}

double PerformanceTiming::IntegerMillisecondsToMonotonicTime(
    unsigned long long integer_milliseconds) const {
  const DocumentLoadTiming* timing = GetDocumentLoadTiming();
  if (!timing)
    return 0;

  return timing->PseudoWallTimeToMonotonicTime(
      ToDoubleSeconds(integer_milliseconds));
}

DEFINE_TRACE(PerformanceTiming) {
  DOMWindowClient::Trace(visitor);
}

}  // namespace blink
