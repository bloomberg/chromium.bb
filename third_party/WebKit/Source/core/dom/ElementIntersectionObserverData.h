// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ElementIntersectionObserverData_h
#define ElementIntersectionObserverData_h

#include "platform/heap/Handle.h"

namespace blink {

class Element;
class IntersectionObservation;
class IntersectionObserver;

class ElementIntersectionObserverData : public GarbageCollectedFinalized<ElementIntersectionObserverData> {
public:
    DECLARE_TRACE();
    ElementIntersectionObserverData();
    ~ElementIntersectionObserverData();

    bool hasIntersectionObserver() const;
    bool hasIntersectionObservation() const;
    IntersectionObservation* getObservationFor(IntersectionObserver&);
    void addObservation(IntersectionObservation&);
    void removeObservation(IntersectionObserver&);
    void activateValidIntersectionObservers(Element&);
    void deactivateAllIntersectionObservers(Element&);

#if !ENABLE(OILPAN)
    void dispose();
#endif

    WeakPtrWillBeRawPtr<Element> createWeakPtr(Element*);

private:
    // IntersectionObservers for which the Element owning this data is root.
    HeapHashSet<WeakMember<IntersectionObserver>> m_intersectionObservers;
    // IntersectionObservations for which the Element owning this data is target.
    HeapHashMap<Member<IntersectionObserver>, Member<IntersectionObservation>> m_intersectionObservations;

#if !ENABLE(OILPAN)
    OwnPtr<WeakPtrFactory<Element>> m_weakPointerFactory;
#endif
};

} // namespace blink

#endif // ElementIntersectionObserverData_h
