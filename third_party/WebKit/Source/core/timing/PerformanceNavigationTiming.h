// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PerformanceNavigationTiming_h
#define PerformanceNavigationTiming_h

#include "core/CoreExport.h"
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
    : public PerformanceResourceTiming {
  DEFINE_WRAPPERTYPEINFO();
  friend class PerformanceNavigationTimingTest;

 public:
  enum class NavigationType { Navigate, Reload, BackForward, Prerender };

  PerformanceNavigationTiming(LocalFrame*,
                              ResourceTimingInfo*,
                              double timeOrigin);

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
  void buildJSONValue(V8ObjectBuilder&) const override;

 private:
  ~PerformanceNavigationTiming() override;

  const DocumentTiming* documentTiming() const;
  DocumentLoader* documentLoader() const;
  DocumentLoadTiming* documentLoadTiming() const;

  virtual ResourceLoadTiming* resourceLoadTiming() const;
  virtual bool allowTimingDetails() const;
  virtual bool didReuseConnection() const;
  virtual unsigned long long getTransferSize() const;
  virtual unsigned long long getEncodedBodySize() const;
  virtual unsigned long long getDecodedBodySize() const;

  bool getAllowRedirectDetails() const;

  double m_timeOrigin;
  RefPtr<ResourceTimingInfo> m_resourceTimingInfo;
  // TODO(sunjian): Investigate why not using a Member instead.
  WeakMember<LocalFrame> m_frame;
};
}  // namespace blink

#endif  // PerformanceNavigationTiming_h
