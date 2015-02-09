/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Timer_h
#define Timer_h

#include "platform/PlatformExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/Noncopyable.h"
#include "wtf/Threading.h"
#include "wtf/Vector.h"

namespace blink {

// Time intervals are all in seconds.

class PLATFORM_EXPORT TimerBase {
    WTF_MAKE_NONCOPYABLE(TimerBase);
public:
    TimerBase();
    virtual ~TimerBase();

    void start(double nextFireInterval, double repeatInterval, const WebTraceLocation&);

    void startRepeating(double repeatInterval, const WebTraceLocation& caller)
    {
        start(repeatInterval, repeatInterval, caller);
    }
    void startOneShot(double interval, const WebTraceLocation& caller)
    {
        start(interval, 0, caller);
    }

    void stop();
    bool isActive() const;
    const WebTraceLocation& location() const { return m_location; }

    double nextFireInterval() const;
    double nextUnalignedFireInterval() const;
    double repeatInterval() const { return m_repeatInterval; }

    void augmentRepeatInterval(double delta) {
        setNextFireTime(m_nextFireTime + delta);
        m_repeatInterval += delta;
    }

    void didChangeAlignmentInterval();

    static void fireTimersInNestedEventLoop();

private:
    virtual void fired() = 0;

    virtual double alignedFireTime(double fireTime) const { return fireTime; }

    void checkConsistency() const;
    void checkHeapIndex() const;

    void setNextFireTime(double);

    bool inHeap() const { return m_heapIndex != -1; }

    bool hasValidHeapPosition() const;
    void updateHeapIfNeeded(double oldTime);

    void heapDecreaseKey();
    void heapDelete();
    void heapDeleteMin();
    void heapIncreaseKey();
    void heapInsert();
    void heapPop();
    void heapPopMin();

    Vector<TimerBase*>& timerHeap() const { ASSERT(m_cachedThreadGlobalTimerHeap); return *m_cachedThreadGlobalTimerHeap; }

    double m_nextFireTime; // 0 if inactive
    double m_unalignedNextFireTime; // m_nextFireTime not considering alignment interval
    double m_repeatInterval; // 0 if not repeating
    int m_heapIndex; // -1 if not in heap
    unsigned m_heapInsertionOrder; // Used to keep order among equal-fire-time timers
    Vector<TimerBase*>* m_cachedThreadGlobalTimerHeap;
    WebTraceLocation m_location;

#if ENABLE(ASSERT)
    ThreadIdentifier m_thread;
#endif

    friend class ThreadTimers;
    friend class TimerHeapLessThanFunction;
    friend class TimerHeapReference;
};

template<typename T, bool = IsGarbageCollectedType<T>::value>
class TimerIsObjectAliveTrait {
public:
    static bool isAlive(T*) { return true; }
};

template<typename T>
class TimerIsObjectAliveTrait<T, true> {
public:
    static bool isAlive(T* objectPointer)
    {
        // Oilpan: if a timer fires while Oilpan heaps are being lazily
        // swept, it is not safe to proceed if the object is about to
        // be swept (and this timer will be stopped while doing so.)
        return !Heap::willObjectBeLazilySwept(objectPointer);
    }
};

template <typename TimerFiredClass>
class Timer final : public TimerBase {
public:
    typedef void (TimerFiredClass::*TimerFiredFunction)(Timer*);

    Timer(TimerFiredClass* o, TimerFiredFunction f)
        : m_object(o), m_function(f) { }

private:
    virtual void fired() override
    {
        if (!TimerIsObjectAliveTrait<TimerFiredClass>::isAlive(m_object))
            return;
        (m_object->*m_function)(this);
    }

    // FIXME: oilpan: TimerBase should be moved to the heap and m_object should be traced.
    // This raw pointer is safe as long as Timer<X> is held by the X itself (That's the case
    // in the current code base).
    GC_PLUGIN_IGNORE("363031")
    TimerFiredClass* m_object;
    TimerFiredFunction m_function;
};

inline bool TimerBase::isActive() const
{
    ASSERT(m_thread == currentThread());
    return m_nextFireTime;
}

}

#endif
