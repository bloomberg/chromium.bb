// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResizeObservation_h
#define ResizeObservation_h

#include "core/CoreExport.h"
#include "core/resize_observer/ResizeObserverEntry.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/heap/Handle.h"

namespace blink {

class Element;
class ResizeObserver;

// ResizeObservation represents an element that is being observed.
class CORE_EXPORT ResizeObservation final
    : public GarbageCollected<ResizeObservation> {
 public:
  ResizeObservation(Element* target, ResizeObserver*);

  Element* Target() const { return target_; }
  size_t TargetDepth();
  // True if observationSize differs from target's current size.
  bool ObservationSizeOutOfSync();
  void SetObservationSize(const LayoutSize&);
  void ElementSizeChanged();

  LayoutSize ComputeTargetSize() const;
  LayoutPoint ComputeTargetLocation() const;

  void Trace(blink::Visitor*);

 private:
  WeakMember<Element> target_;
  Member<ResizeObserver> observer_;
  // Target size sent in last observation notification.
  LayoutSize observation_size_;
  bool element_size_changed_;
};

}  // namespace blink

#endif  // ResizeObservation_h
