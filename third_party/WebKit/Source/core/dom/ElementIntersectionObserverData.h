// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ElementIntersectionObserverData_h
#define ElementIntersectionObserverData_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"

namespace blink {

class Node;
class IntersectionObservation;
class IntersectionObserver;

class ElementIntersectionObserverData
    : public GarbageCollected<ElementIntersectionObserverData>,
      public TraceWrapperBase {
 public:
  ElementIntersectionObserverData();

  IntersectionObservation* GetObservationFor(IntersectionObserver&);
  void AddObserver(IntersectionObserver&);
  void RemoveObserver(IntersectionObserver&);
  void AddObservation(IntersectionObservation&);
  void RemoveObservation(IntersectionObserver&);
  void ActivateValidIntersectionObservers(Node&);
  void DeactivateAllIntersectionObservers(Node&);

  DECLARE_TRACE();
  DECLARE_TRACE_WRAPPERS();

 private:
  // IntersectionObservers for which the Node owning this data is root.
  HeapHashSet<WeakMember<IntersectionObserver>> intersection_observers_;
  // IntersectionObservations for which the Node owning this data is target.
  HeapHashMap<TraceWrapperMember<IntersectionObserver>,
              Member<IntersectionObservation>>
      intersection_observations_;
};

}  // namespace blink

#endif  // ElementIntersectionObserverData_h
