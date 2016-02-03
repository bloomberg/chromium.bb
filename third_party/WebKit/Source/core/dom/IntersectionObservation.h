// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IntersectionObservation_h
#define IntersectionObservation_h

#include "core/dom/DOMHighResTimeStamp.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/heap/Handle.h"

namespace blink {

class Element;
class IntersectionObserver;
class Node;

// TODO(oilpan): Switch to GarbageCollected<> after oilpan ships.
class IntersectionObservation : public GarbageCollectedFinalized<IntersectionObservation> {
public:
    IntersectionObservation(IntersectionObserver&, Element&, bool shouldReportRootBounds);

    struct IntersectionGeometry {
        LayoutRect targetRect;
        LayoutRect intersectionRect;
        LayoutRect rootRect;
    };

    IntersectionObserver& observer() const { return *m_observer; }
    Element* target() const;
    bool isActive() const { return m_active; }
    void setActive(bool active) { m_active = active; }
    unsigned lastThresholdIndex() const { return m_lastThresholdIndex; }
    void setLastThresholdIndex(unsigned index) { m_lastThresholdIndex = index; }
    bool shouldReportRootBounds() const { return m_shouldReportRootBounds; }
    void computeIntersectionObservations(DOMHighResTimeStamp);
    void disconnect();
    void clearRootAndRemoveFromTarget();

    DECLARE_TRACE();

private:
    void initializeGeometry(IntersectionGeometry&);
    void clipToRoot(LayoutRect&);
    void clipToFrameView(IntersectionGeometry&);
    bool computeGeometry(IntersectionGeometry&);

    Member<IntersectionObserver> m_observer;

    // TODO(szager): Why Node instead of Element?  Because NodeIntersectionObserverData::createWeakPtr()
    // returns a WeakPtr<Node>, which cannot be coerced into a WeakPtr<Element>.  When oilpan rolls out,
    // this can be changed to WeakMember<Element>.
    WeakPtrWillBeWeakMember<Node> m_target;

    unsigned m_active : 1;
    unsigned m_shouldReportRootBounds : 1;
    unsigned m_lastThresholdIndex : 30;
};

} // namespace blink

#endif // IntersectionObservation_h
