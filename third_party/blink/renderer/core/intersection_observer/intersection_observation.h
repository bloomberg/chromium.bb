// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVATION_H_

#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

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
  // If the parameter is true and the observer doesn't have an explicit root,
  // then any notifications generated will contain root bounds geometry.
  void Compute(bool should_report_implicit_root_bounds);
  void TakeRecords(HeapVector<Member<IntersectionObserverEntry>>&);
  void Disconnect();

  void Trace(blink::Visitor*);

 private:
  void SetLastThresholdIndex(unsigned index) { last_threshold_index_ = index; }
  void SetWasVisible(bool last_is_visible) {
    last_is_visible_ = last_is_visible ? 1 : 0;
  }

  // If trackVisibility is true, don't compute observations more frequently
  // than this many milliseconds.
  static const DOMHighResTimeStamp s_v2_throttle_delay_;

  Member<IntersectionObserver> observer_;
  WeakMember<Element> target_;
  HeapVector<Member<IntersectionObserverEntry>> entries_;
  DOMHighResTimeStamp last_run_time_;

  unsigned last_is_visible_ : 1;
  unsigned last_threshold_index_ : 31;
  static const unsigned kMaxThresholdIndex = (unsigned)0x80000000;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVATION_H_
