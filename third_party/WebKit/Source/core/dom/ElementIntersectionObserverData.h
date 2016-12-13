// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ElementIntersectionObserverData_h
#define ElementIntersectionObserverData_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/TraceWrapperMember.h"
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

  IntersectionObservation* getObservationFor(IntersectionObserver&);
  void addObserver(IntersectionObserver&);
  void removeObserver(IntersectionObserver&);
  void addObservation(IntersectionObservation&);
  void removeObservation(IntersectionObserver&);
  void activateValidIntersectionObservers(Node&);
  void deactivateAllIntersectionObservers(Node&);

  DECLARE_TRACE();
  DECLARE_TRACE_WRAPPERS();

 private:
  // IntersectionObservers for which the Node owning this data is root.
  HeapHashSet<WeakMember<IntersectionObserver>> m_intersectionObservers;
  // IntersectionObservations for which the Node owning this data is target.
  HeapHashMap<TraceWrapperMember<IntersectionObserver>,
              Member<IntersectionObservation>>
      m_intersectionObservations;
};

}  // namespace blink

#endif  // ElementIntersectionObserverData_h
