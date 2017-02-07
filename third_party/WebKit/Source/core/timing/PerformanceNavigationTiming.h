// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PerformanceNavigationTiming_h
#define PerformanceNavigationTiming_h

#include "core/CoreExport.h"
#include "core/dom/DOMHighResTimeStamp.h"
#include "core/timing/PerformanceResourceTiming.h"

namespace blink {

class CORE_EXPORT PerformanceNavigationTiming final
    : public PerformanceResourceTiming {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class NavigationType { Navigate, Reload, BackForward, Prerender };

  PerformanceNavigationTiming(double timeOrigin,
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
                              NavigationType,
                              double redirectStart,
                              double redirectEnd,
                              double fetchStart,
                              double responseEnd,
                              bool allowRedirectDetails,
                              bool hasSameOriginAsPreviousDocument,
                              ResourceLoadTiming*,
                              double lastRedirectEndTime,
                              double finishTime,
                              unsigned long long transferSize,
                              unsigned long long encodedBodyLength,
                              unsigned long long decodedBodyLength,
                              bool didReuseConnection);

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

 protected:
  void buildJSONValue(V8ObjectBuilder&) const override;

 private:
  ~PerformanceNavigationTiming() override;

  double m_timeOrigin;
  double m_unloadEventStart;
  double m_unloadEventEnd;
  double m_loadEventStart;
  double m_loadEventEnd;
  unsigned short m_redirectCount;
  double m_domInteractive;
  double m_domContentLoadedEventStart;
  double m_domContentLoadedEventEnd;
  double m_domComplete;
  NavigationType m_type;
  double m_redirectStart;
  double m_redirectEnd;
  double m_fetchStart;
  double m_responseEnd;
  bool m_allowRedirectDetails;
  bool m_hasSameOriginAsPreviousDocument;
};
}  // namespace blink

#endif  // PerformanceNavigationTiming_h
