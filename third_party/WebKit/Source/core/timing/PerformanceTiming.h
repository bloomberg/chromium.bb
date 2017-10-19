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

#ifndef PerformanceTiming_h
#define PerformanceTiming_h

#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class CSSTiming;
class DocumentLoadTiming;
class DocumentLoader;
class DocumentParserTiming;
class DocumentTiming;
class LocalFrame;
class PaintTiming;
class ResourceLoadTiming;
class ScriptState;
class ScriptValue;

// Legacy support for NT1(https://www.w3.org/TR/navigation-timing/).
class CORE_EXPORT PerformanceTiming final
    : public GarbageCollected<PerformanceTiming>,
      public ScriptWrappable,
      public DOMWindowClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PerformanceTiming);

 public:
  static PerformanceTiming* Create(LocalFrame* frame) {
    return new PerformanceTiming(frame);
  }

  unsigned long long navigationStart() const;
  unsigned long long unloadEventStart() const;
  unsigned long long unloadEventEnd() const;
  unsigned long long redirectStart() const;
  unsigned long long redirectEnd() const;
  unsigned long long fetchStart() const;
  unsigned long long domainLookupStart() const;
  unsigned long long domainLookupEnd() const;
  unsigned long long connectStart() const;
  unsigned long long connectEnd() const;
  unsigned long long secureConnectionStart() const;
  unsigned long long requestStart() const;
  unsigned long long responseStart() const;
  unsigned long long responseEnd() const;
  unsigned long long domLoading() const;
  unsigned long long domInteractive() const;
  unsigned long long domContentLoadedEventStart() const;
  unsigned long long domContentLoadedEventEnd() const;
  unsigned long long domComplete() const;
  unsigned long long loadEventStart() const;
  unsigned long long loadEventEnd() const;

  // The below are non-spec timings, for Page Load UMA metrics.

  // The time the first document layout is performed.
  unsigned long long FirstLayout() const;
  // The time the first paint operation was performed.
  unsigned long long FirstPaint() const;
  // The time the first paint operation for visible text was performed.
  unsigned long long FirstTextPaint() const;
  // The time the first paint operation for image was performed.
  unsigned long long FirstImagePaint() const;
  // The time of the first 'contentful' paint. A contentful paint is a paint
  // that includes content of some kind (for example, text or image content).
  unsigned long long FirstContentfulPaint() const;
  // The time of the first 'meaningful' paint, A meaningful paint is a paint
  // where the page's primary content is visible.
  unsigned long long FirstMeaningfulPaint() const;

  unsigned long long ParseStart() const;
  unsigned long long ParseStop() const;
  unsigned long long ParseBlockedOnScriptLoadDuration() const;
  unsigned long long ParseBlockedOnScriptLoadFromDocumentWriteDuration() const;
  unsigned long long ParseBlockedOnScriptExecutionDuration() const;
  unsigned long long ParseBlockedOnScriptExecutionFromDocumentWriteDuration()
      const;
  unsigned long long AuthorStyleSheetParseDurationBeforeFCP() const;
  unsigned long long UpdateStyleDurationBeforeFCP() const;

  ScriptValue toJSONForBinding(ScriptState*) const;

  virtual void Trace(blink::Visitor*);

  unsigned long long MonotonicTimeToIntegerMilliseconds(double) const;
  double IntegerMillisecondsToMonotonicTime(unsigned long long) const;

 private:
  explicit PerformanceTiming(LocalFrame*);

  const DocumentTiming* GetDocumentTiming() const;
  const CSSTiming* CssTiming() const;
  const DocumentParserTiming* GetDocumentParserTiming() const;
  const PaintTiming* GetPaintTiming() const;
  DocumentLoader* GetDocumentLoader() const;
  DocumentLoadTiming* GetDocumentLoadTiming() const;
  ResourceLoadTiming* GetResourceLoadTiming() const;
};

}  // namespace blink

#endif  // PerformanceTiming_h
