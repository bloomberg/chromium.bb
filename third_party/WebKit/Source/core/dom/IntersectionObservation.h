// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IntersectionObservation_h
#define IntersectionObservation_h

#include "core/dom/DOMHighResTimeStamp.h"
#include "platform/heap/Handle.h"

namespace blink {

class Element;
class IntersectionObserver;

class IntersectionObservation final
    : public GarbageCollected<IntersectionObservation> {
 public:
  IntersectionObservation(IntersectionObserver&,
                          Element&,
                          bool shouldReportRootBounds);

  IntersectionObserver* observer() const { return m_observer.get(); }
  Element* target() const { return m_target; }
  unsigned lastThresholdIndex() const { return m_lastThresholdIndex; }
  void computeIntersectionObservations(DOMHighResTimeStamp);
  void disconnect();

  DECLARE_TRACE();

 private:
  void setLastThresholdIndex(unsigned index) { m_lastThresholdIndex = index; }

  Member<IntersectionObserver> m_observer;
  WeakMember<Element> m_target;

  const unsigned m_shouldReportRootBounds : 1;

  unsigned m_lastThresholdIndex : 30;
  static const unsigned kMaxThresholdIndex = (unsigned)0x40000000;
};

}  // namespace blink

#endif  // IntersectionObservation_h
