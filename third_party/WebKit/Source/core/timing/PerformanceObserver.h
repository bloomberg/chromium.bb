// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PerformanceObserver_h
#define PerformanceObserver_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/timing/PerformanceEntry.h"
#include "platform/heap/Handle.h"
#include "wtf/HashSet.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace blink {

class ExceptionState;
class PerformanceBase;
class PerformanceObserverCallback;
class PerformanceObserver;
class PerformanceObserverInit;

using PerformanceEntryVector = HeapVector<Member<PerformanceEntry>>;

class PerformanceObserver final : public GarbageCollectedFinalized<PerformanceObserver>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
    friend class PerformanceBase;
public:
    static PerformanceObserver* create(PerformanceBase*, PerformanceObserverCallback*);
    static void resumeSuspendedObservers();

    ~PerformanceObserver();

    void observe(const PerformanceObserverInit&, ExceptionState&);
    void disconnect();
    void enqueuePerformanceEntry(PerformanceEntry&);
    PerformanceEntryTypeMask filterOptions() const { return m_filterOptions; }

    // Eagerly finalized as destructor accesses heap object members.
    EAGERLY_FINALIZE();
    DECLARE_TRACE();

private:
    explicit PerformanceObserver(PerformanceBase*, PerformanceObserverCallback*);
    void deliver();
    bool shouldBeSuspended() const;

    Member<PerformanceObserverCallback> m_callback;
    WeakMember<PerformanceBase> m_performance;
    PerformanceEntryVector m_performanceEntries;
    PerformanceEntryTypeMask m_filterOptions;
    bool m_isRegistered;
};

} // namespace blink

#endif // PerformanceObserver_h
