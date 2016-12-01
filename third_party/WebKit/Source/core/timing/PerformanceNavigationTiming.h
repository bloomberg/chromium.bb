// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PerformanceNavigationTiming_h
#define PerformanceNavigationTiming_h

#include "core/CoreExport.h"
#include "core/timing/PerformanceResourceTiming.h"

namespace blink {

class CORE_EXPORT PerformanceNavigationTiming final
    : public PerformanceResourceTiming {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class NavigationType { Navigate, Reload, BackForward, Prerender };

  PerformanceNavigationTiming(double timeOrigin,
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
                              bool hasCrossOriginRedirect,
                              bool hasSameOriginAsPreviousDocument,
                              ResourceLoadTiming*,
                              double lastRedirectEndTime,
                              double finishTime,
                              unsigned long long transferSize,
                              unsigned long long encodedBodyLength,
                              unsigned long long decodedBodyLength,
                              bool didReuseConnection);

  double unloadEventStart() const;
  double unloadEventEnd() const;
  double domInteractive() const;
  double domContentLoadedEventStart() const;
  double domContentLoadedEventEnd() const;
  double domComplete() const;
  double loadEventStart() const;
  double loadEventEnd() const;
  AtomicString type() const;
  unsigned short redirectCount() const;

  // PerformanceResourceTiming overrides:
  double fetchStart() const override;
  double redirectStart() const override;
  double redirectEnd() const override;
  double responseEnd() const override;

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
  bool m_hasCrossOriginRedirect;
  bool m_hasSameOriginAsPreviousDocument;
};
}  // namespace blink

#endif  // PerformanceNavigationTiming_h
