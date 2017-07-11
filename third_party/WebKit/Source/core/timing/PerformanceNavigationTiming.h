// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PerformanceNavigationTiming_h
#define PerformanceNavigationTiming_h

#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/DOMHighResTimeStamp.h"
#include "core/loader/FrameLoaderTypes.h"
#include "core/timing/PerformanceResourceTiming.h"

namespace blink {

class Document;
class DocumentTiming;
class DocumentLoader;
class DocumentLoadTiming;
class LocalFrame;
class ExecutionContext;
class ResourceTimingInfo;
class ResourceLoadTiming;

class CORE_EXPORT PerformanceNavigationTiming final
    : public PerformanceResourceTiming,
      public ContextClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PerformanceNavigationTiming);
  friend class PerformanceNavigationTimingTest;

 public:
  PerformanceNavigationTiming(LocalFrame*,
                              ResourceTimingInfo*,
                              double time_origin,
                              PerformanceServerTimingVector&);

  // Attributes inheritted from PerformanceEntry.
  DOMHighResTimeStamp duration() const override;

  AtomicString initiatorType() const override;

  // PerformanceNavigationTiming's unique attributes.
  DOMHighResTimeStamp unloadEventStart() const;
  DOMHighResTimeStamp unloadEventEnd() const;
  DOMHighResTimeStamp domInteractive() const;
  DOMHighResTimeStamp domContentLoadedEventStart() const;
  DOMHighResTimeStamp domContentLoadedEventEnd() const;
  DOMHighResTimeStamp domComplete() const;
  DOMHighResTimeStamp loadEventStart() const;
  DOMHighResTimeStamp loadEventEnd() const;
  AtomicString type() const;
  unsigned short redirectCount() const;

  // PerformanceResourceTiming overrides:
  DOMHighResTimeStamp fetchStart() const override;
  DOMHighResTimeStamp redirectStart() const override;
  DOMHighResTimeStamp redirectEnd() const override;
  DOMHighResTimeStamp responseEnd() const override;

  DECLARE_VIRTUAL_TRACE();

 protected:
  void BuildJSONValue(ScriptState*, V8ObjectBuilder&) const override;

 private:
  ~PerformanceNavigationTiming() override;

  static AtomicString GetNavigationType(NavigationType, const Document*);

  const DocumentTiming* GetDocumentTiming() const;
  DocumentLoader* GetDocumentLoader() const;
  DocumentLoadTiming* GetDocumentLoadTiming() const;

  virtual ResourceLoadTiming* GetResourceLoadTiming() const;
  virtual bool AllowTimingDetails() const;
  virtual bool DidReuseConnection() const;
  virtual unsigned long long GetTransferSize() const;
  virtual unsigned long long GetEncodedBodySize() const;
  virtual unsigned long long GetDecodedBodySize() const;

  bool GetAllowRedirectDetails() const;

  AtomicString AlpnNegotiatedProtocol() const override;
  AtomicString ConnectionInfo() const override;

  RefPtr<ResourceTimingInfo> resource_timing_info_;
};
}  // namespace blink

#endif  // PerformanceNavigationTiming_h
