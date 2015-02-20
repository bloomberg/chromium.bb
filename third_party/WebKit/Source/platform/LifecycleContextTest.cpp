/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
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

#include "config.h"

#include "platform/LifecycleNotifier.h"
#include "platform/heap/Handle.h"
#include <gtest/gtest.h>

using namespace blink;

namespace blink {

class DummyContext final : public NoBaseWillBeGarbageCollectedFinalized<DummyContext>, public LifecycleNotifier<DummyContext> {
public:
    DummyContext()
        : LifecycleNotifier<DummyContext>(this)
    {
    }

    DEFINE_INLINE_TRACE()
    {
        LifecycleNotifier<DummyContext>::trace(visitor);
    }
};

template<> void observeContext(DummyContext* context, LifecycleObserver<DummyContext>* observer)
{
    context->addObserver(observer);
}

template<> void unobserveContext(DummyContext* context, LifecycleObserver<DummyContext>* observer)
{
    context->removeObserver(observer);
}

class TestingObserver final : public GarbageCollectedFinalized<TestingObserver>, public LifecycleObserver<DummyContext> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(TestingObserver);
public:
    explicit TestingObserver(DummyContext* context)
        : LifecycleObserver<DummyContext>(context)
        , m_contextDestroyedCalled(false)
    { }

    virtual void contextDestroyed() override
    {
        LifecycleObserver<DummyContext>::contextDestroyed();
        m_contextDestroyedCalled = true;
    }

    DEFINE_INLINE_TRACE()
    {
        LifecycleObserver<DummyContext>::trace(visitor);
    }

    bool m_contextDestroyedCalled;

    void unobserve() { setContext(nullptr); }
};

TEST(LifecycleContextTest, shouldObserveContextDestroyed)
{
    OwnPtrWillBeRawPtr<DummyContext> context = adoptPtrWillBeNoop(new DummyContext());
    Persistent<TestingObserver> observer = new TestingObserver(context.get());

    EXPECT_EQ(observer->lifecycleContext(), context.get());
    EXPECT_FALSE(observer->m_contextDestroyedCalled);
    context->notifyContextDestroyed();
    context = nullptr;
    Heap::collectAllGarbage();
    EXPECT_EQ(observer->lifecycleContext(), static_cast<DummyContext*>(0));
    EXPECT_TRUE(observer->m_contextDestroyedCalled);
}

TEST(LifecycleContextTest, shouldNotObserveContextDestroyedIfUnobserve)
{
    OwnPtrWillBeRawPtr<DummyContext> context = adoptPtrWillBeNoop(new DummyContext());
    Persistent<TestingObserver> observer = new TestingObserver(context.get());
    observer->unobserve();
    context->notifyContextDestroyed();
    context = nullptr;
    Heap::collectAllGarbage();
    EXPECT_EQ(observer->lifecycleContext(), static_cast<DummyContext*>(0));
    EXPECT_FALSE(observer->m_contextDestroyedCalled);
}

}
