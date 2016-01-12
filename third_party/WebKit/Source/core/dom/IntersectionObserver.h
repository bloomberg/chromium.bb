// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IntersectionObserver_h
#define IntersectionObserver_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/Element.h"
#include "core/dom/IntersectionObservation.h"
#include "core/dom/IntersectionObserverEntry.h"
#include "platform/heap/Handle.h"
#include "wtf/HashSet.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace blink {

class ExceptionState;
class IntersectionObserverCallback;
class IntersectionObserverInit;

class IntersectionObserver final : public GarbageCollectedFinalized<IntersectionObserver>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();

public:
    static IntersectionObserver* create(const IntersectionObserverInit&, IntersectionObserverCallback&, ExceptionState&);
    static void resumeSuspendedObservers();

    // API methods
    void observe(Element*, ExceptionState&);
    void unobserve(Element*, ExceptionState&);
    HeapVector<Member<IntersectionObserverEntry>> takeRecords();

    Element* root() { return m_root.get(); }
    LayoutObject* rootLayoutObject();
    bool isDescendantOfRoot(const Element*) const;
    void computeIntersectionObservations(double timestamp);
    void enqueueIntersectionObserverEntry(IntersectionObserverEntry&);
    unsigned firstThresholdGreaterThan(float ratio) const;
    void deliver();
    void setActive(bool);
    void disconnect();
    void removeObservation(IntersectionObservation&);
    bool hasEntries() const { return m_entries.size(); }

    DECLARE_TRACE();

private:
    explicit IntersectionObserver(IntersectionObserverCallback&, Element&, const Vector<float>& thresholds);

    void checkRootAndDetachIfNeeded();

    Member<IntersectionObserverCallback> m_callback;
    WeakPtrWillBeWeakMember<Element> m_root;
    HeapHashSet<WeakMember<IntersectionObservation>> m_observations;
    HeapVector<Member<IntersectionObserverEntry>> m_entries;
    Vector<float> m_thresholds;
};

} // namespace blink

#endif // IntersectionObserver_h
