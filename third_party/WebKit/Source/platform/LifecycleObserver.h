/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 *
 */

#ifndef LifecycleObserver_h
#define LifecycleObserver_h

#include "platform/heap/Handle.h"
#include "wtf/Assertions.h"

namespace blink {

template<typename T>
class LifecycleObserver : public WillBeGarbageCollectedMixin {
    // FIXME: Oilpan: Remove the pre-finalizer by moving LifecycleNotifier
    // to Oilpan's heap and making LifecycleNotifier::m_observers
    // a hash set of weak members.
    WILL_BE_USING_PRE_FINALIZER(LifecycleObserver, dispose);
public:
    typedef T Context;

    enum Type {
        ActiveDOMObjectType,
        DocumentLifecycleObserverType,
        GenericType,
        PageLifecycleObserverType,
        DOMWindowLifecycleObserverType
    };

    explicit LifecycleObserver(Context* context, Type type = GenericType)
        : m_lifecycleContext(nullptr)
        , m_observerType(type)
    {
#if ENABLE(OILPAN)
        ThreadState::current()->registerPreFinalizer(*this);
#endif
        setContext(context);
    }
    virtual ~LifecycleObserver()
    {
#if !ENABLE(OILPAN)
        dispose();
#endif
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_lifecycleContext);
    }
    virtual void contextDestroyed() { }
    void dispose()
    {
        setContext(nullptr);
    }

    Context* lifecycleContext() const { return m_lifecycleContext; }
    void clearLifecycleContext() { m_lifecycleContext = nullptr; }
    Type observerType() const { return m_observerType; }

protected:
    void setContext(Context*);

    RawPtrWillBeWeakMember<Context> m_lifecycleContext;
    Type m_observerType;
};

//
// These functions should be specialized for each LifecycleObserver instances.
//
template<typename T> void observeContext(T*, LifecycleObserver<T>*) { ASSERT_NOT_REACHED(); }
template<typename T> void unobserveContext(T*, LifecycleObserver<T>*) { ASSERT_NOT_REACHED(); }


template<typename T>
inline void LifecycleObserver<T>::setContext(typename LifecycleObserver<T>::Context* context)
{
    if (m_lifecycleContext)
        unobserveContext(m_lifecycleContext.get(), this);

    m_lifecycleContext = context;

    if (m_lifecycleContext)
        observeContext(m_lifecycleContext.get(), this);
}

} // namespace blink

#endif // LifecycleObserver_h
