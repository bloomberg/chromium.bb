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

// IntersectionObservation represents the result of calling
// IntersectionObserver::observe(target) for some target element; it tracks the
// intersection between a single target element and the IntersectionObserver's
// root.  It is an implementation-internal class without any exposed interface.
class IntersectionObservation final
    : public GarbageCollected<IntersectionObservation> {
 public:
  IntersectionObservation(IntersectionObserver&, Element&);

  IntersectionObserver* Observer() const { return observer_.Get(); }
  Element* Target() const { return target_; }
  unsigned LastThresholdIndex() const { return last_threshold_index_; }
  void ComputeIntersectionObservations(DOMHighResTimeStamp);
  void Disconnect();
  void UpdateShouldReportRootBoundsAfterDomChange();

  DECLARE_TRACE();

 private:
  void SetLastThresholdIndex(unsigned index) { last_threshold_index_ = index; }

  Member<IntersectionObserver> observer_;
  WeakMember<Element> target_;

  unsigned should_report_root_bounds_ : 1;

  unsigned last_threshold_index_ : 30;
  static const unsigned kMaxThresholdIndex = (unsigned)0x40000000;
};

}  // namespace blink

#endif  // IntersectionObservation_h
